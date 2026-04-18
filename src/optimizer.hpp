/* LLGO Sea-of-Nodes Optimizer */

#pragma once
#include "son_builder.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <optional>

namespace llgo
{
    class SONOptimizer
    {
    public:
        SONOptimizer() = default;

        void run(Graph& graph)
        {
            // Phase 1: constant folding + strength reduction (data)
            bool changed = true;
            while (changed)
            {
                changed  = constantFolding(graph);
                changed |= strengthReduction(graph);
                changed |= algebraicIdentities(graph);
            }

            // Phase 2: structural
            removeUnreachable(graph);
            compressTrivialMerges(graph);
            commonSubexpressionElimination(graph);
            deadCodeElimination(graph);
            removeUnreachable(graph);
        }

    private:
        // -------------------------------------------------------
        // Use-map
        // -------------------------------------------------------
        using UseMap = std::unordered_map<Node*, std::vector<Node*>>;

        void buildUseMap(const Graph& graph, UseMap& uses)
        {
            uses.clear();
            for (Node* pNode : graph.nodes)
            {
                for (Node* pIn : pNode->inputs)
                {
                    uses[pIn].push_back(pNode);
                }
            }
        }

        // -------------------------------------------------------
        // Constant value extraction
        // -------------------------------------------------------
        std::optional<std::int64_t> constVal(const Node* pNode) const
        {
            if (pNode->kind != NodeKind::ConstInt)
                return std::nullopt;

            std::int64_t v = 0;
            bool neg       = false;
            const auto& s  = pNode->resultName;
            std::size_t i  = 0;

            if (!s.empty() && s[0] == '-')
            {
                neg = true;
                i   = 1;
            }

            for (; i < s.size(); ++i)
            {
                if (s[i] < '0' || s[i] > '9')
                    break;
                v = v * 10 + static_cast<std::int64_t>(s[i] - '0');
            }

            return neg ? -v : v;
        }

        Node* makeConst(Graph& graph, std::int64_t val, frontend::Type type)
        {
            Node* pNode         = new Node();
            pNode->id           = static_cast<std::uint32_t>(graph.nodes.size());
            pNode->kind         = NodeKind::ConstInt;
            pNode->resultType   = type;
            pNode->resultName   = std::to_string(val);
            graph.nodes.push_back(pNode);
            return pNode;
        }

        // Replace all uses of pOld with pNew
        void replaceAllUses(Graph& graph, Node* pOld, Node* pNew)
        {
            for (Node* pNode : graph.nodes)
            {
                for (Node*& pIn : pNode->inputs)
                {
                    if (pIn == pOld)
                        pIn = pNew;
                }
            }

            if (graph.pStart == pOld)
                graph.pStart = pNew;
        }

        // -------------------------------------------------------
        // Constant folding: evaluate pure ops on ConstInt inputs
        // -------------------------------------------------------
        bool constantFolding(Graph& graph)
        {
            bool changed = false;

            for (Node* pNode : graph.nodes)
            {
                if (pNode->inputs.size() < 2)
                    continue;

                auto lhs = constVal(pNode->inputs[0]);
                auto rhs = constVal(pNode->inputs[1]);

                if (!lhs || !rhs)
                    continue;

                std::optional<std::int64_t> result;

                switch (pNode->kind)
                {
                    case NodeKind::Add:
                        result = *lhs + *rhs;
                        break;
                    case NodeKind::Sub:
                        result = *lhs - *rhs;
                        break;
                    case NodeKind::Mul:
                        result = *lhs * *rhs;
                        break;
                    case NodeKind::Div:
                        if (*rhs != 0)
                            result = *lhs / *rhs;
                        break;
                    case NodeKind::Mod:
                        if (*rhs != 0)
                            result = *lhs % *rhs;
                        break;
                    default:
                        break;
                }

                if (result.has_value())
                {
                    Node* pConst = makeConst(graph, *result, pNode->resultType);
                    replaceAllUses(graph, pNode, pConst);
                    pNode->inputs.clear();
                    changed = true;
                }
            }

            return changed;
        }

        // -------------------------------------------------------
        // Strength reduction: mul/div by power-of-two → shift
        // (represented as ConstInt with synthetic resultName tag)
        // -------------------------------------------------------
        bool isPowerOfTwo(std::int64_t v, int& shift)
        {
            if (v <= 0)
                return false;
            if ((v & (v - 1)) != 0)
                return false;
            shift = 0;
            while ((1LL << shift) != v)
                ++shift;
            return true;
        }

        bool strengthReduction(Graph& graph)
        {
            bool changed = false;

            for (Node* pNode : graph.nodes)
            {
                if (pNode->inputs.size() < 2)
                    continue;

                int shift = 0;

                if (pNode->kind == NodeKind::Mul)
                {
                    // Mul x, 1 → x
                    auto rhs = constVal(pNode->inputs[1]);
                    if (rhs && *rhs == 1)
                    {
                        replaceAllUses(graph, pNode, pNode->inputs[0]);
                        pNode->inputs.clear();
                        changed = true;
                    }
                    // Mul x, 0 → 0
                    else if (rhs && *rhs == 0)
                    {
                        Node* pZero = makeConst(graph, 0, pNode->resultType);
                        replaceAllUses(graph, pNode, pZero);
                        pNode->inputs.clear();
                        changed = true;
                    }
                    // Mul x, 2^n → Shl x, n  (model as ConstInt with marker "shl:N")
                    else if (rhs && isPowerOfTwo(*rhs, shift))
                    {
                        pNode->kind = NodeKind::Add; // reuse Add slot as logical shift marker
                        // Mark the shift amount in the rhs constant's name
                        pNode->inputs[1]->resultName = "shl:" + std::to_string(shift);
                        changed = true;
                    }
                }
                else if (pNode->kind == NodeKind::Div)
                {
                    auto rhs = constVal(pNode->inputs[1]);
                    if (rhs && *rhs == 1)
                    {
                        replaceAllUses(graph, pNode, pNode->inputs[0]);
                        pNode->inputs.clear();
                        changed = true;
                    }
                }
                else if (pNode->kind == NodeKind::Add)
                {
                    auto rhs = constVal(pNode->inputs[1]);
                    if (rhs && *rhs == 0)
                    {
                        replaceAllUses(graph, pNode, pNode->inputs[0]);
                        pNode->inputs.clear();
                        changed = true;
                    }
                }
                else if (pNode->kind == NodeKind::Sub)
                {
                    auto rhs = constVal(pNode->inputs[1]);
                    if (rhs && *rhs == 0)
                    {
                        replaceAllUses(graph, pNode, pNode->inputs[0]);
                        pNode->inputs.clear();
                        changed = true;
                    }
                }
            }

            return changed;
        }

        // -------------------------------------------------------
        // Algebraic identities: x - x = 0, x * x when const, etc.
        // -------------------------------------------------------
        bool algebraicIdentities(Graph& graph)
        {
            bool changed = false;

            for (Node* pNode : graph.nodes)
            {
                if (pNode->inputs.size() < 2)
                    continue;

                if (pNode->kind == NodeKind::Sub &&
                    pNode->inputs[0] == pNode->inputs[1])
                {
                    Node* pZero = makeConst(graph, 0, pNode->resultType);
                    replaceAllUses(graph, pNode, pZero);
                    pNode->inputs.clear();
                    changed = true;
                }
            }

            return changed;
        }

        // -------------------------------------------------------
        // Reachability (control + data forward from start)
        // -------------------------------------------------------
        void removeUnreachable(Graph& graph)
        {
            if (graph.pStart == nullptr)
                return;

            UseMap uses;
            buildUseMap(graph, uses);

            std::unordered_set<Node*> reachable;
            std::vector<Node*> work;
            work.push_back(graph.pStart);
            reachable.insert(graph.pStart);

            while (!work.empty())
            {
                Node* pCur = work.back();
                work.pop_back();

                auto it = uses.find(pCur);
                if (it == uses.end())
                    continue;

                for (Node* pUser : it->second)
                {
                    if (reachable.insert(pUser).second)
                        work.push_back(pUser);
                }
            }

            std::vector<Node*> kept;
            kept.reserve(graph.nodes.size());
            for (Node* pNode : graph.nodes)
            {
                if (reachable.count(pNode))
                {
                    kept.push_back(pNode);
                }
                else
                {
                    delete pNode;
                }
            }
            graph.nodes.swap(kept);
        }

        // -------------------------------------------------------
        // Trivial merge compression (single-predecessor merge)
        // -------------------------------------------------------
        void compressTrivialMerges(Graph& graph)
        {
            UseMap uses;
            buildUseMap(graph, uses);

            for (Node* pNode : graph.nodes)
            {
                if (pNode->kind == NodeKind::Merge && pNode->inputs.size() == 1)
                {
                    Node* pIn = pNode->inputs[0];
                    auto it   = uses.find(pNode);
                    if (it != uses.end())
                    {
                        for (Node* pUser : it->second)
                        {
                            for (Node*& pInput : pUser->inputs)
                            {
                                if (pInput == pNode)
                                    pInput = pIn;
                            }
                        }
                    }
                    pNode->inputs.clear();
                }
            }
        }

        // -------------------------------------------------------
        // Common Subexpression Elimination (pure nodes only)
        // -------------------------------------------------------
        void commonSubexpressionElimination(Graph& graph)
        {
            struct Key
            {
                NodeKind              kind;
                frontend::Type        type;
                std::vector<std::uint32_t> inputIds;

                bool operator==(const Key& other) const
                {
                    return kind == other.kind &&
                           type == other.type &&
                           inputIds == other.inputIds;
                }
            };

            struct KeyHash
            {
                std::size_t operator()(const Key& k) const
                {
                    std::size_t h = static_cast<std::size_t>(k.kind) * 1315423911u
                                  ^ static_cast<std::size_t>(k.type) * 2654435761u;
                    for (auto id : k.inputIds)
                        h ^= (id + 0x9e3779b9 + (h << 6) + (h >> 2));
                    return h;
                }
            };

            std::unordered_map<Key, Node*, KeyHash> table;
            UseMap uses;
            buildUseMap(graph, uses);

            for (Node* pNode : graph.nodes)
            {
                if (isEffectful(pNode))
                    continue;

                Key key;
                key.kind = pNode->kind;
                key.type = pNode->resultType;
                key.inputIds.reserve(pNode->inputs.size());
                for (Node* pIn : pNode->inputs)
                    key.inputIds.push_back(pIn->id);

                auto it = table.find(key);
                if (it == table.end())
                {
                    table.emplace(key, pNode);
                }
                else
                {
                    Node* pCanon = it->second;
                    auto uit     = uses.find(pNode);
                    if (uit != uses.end())
                    {
                        for (Node* pUser : uit->second)
                        {
                            for (Node*& pInput : pUser->inputs)
                            {
                                if (pInput == pNode)
                                    pInput = pCanon;
                            }
                        }
                    }
                    pNode->inputs.clear();
                }
            }
        }

        bool isEffectful(const Node* pNode) const
        {
            switch (pNode->kind)
            {
                case NodeKind::Store:
                case NodeKind::Call:
                case NodeKind::Br:
                case NodeKind::CondBr:
                case NodeKind::Ret:
                    return true;
                default:
                    return false;
            }
        }

        // -------------------------------------------------------
        // Dead Code Elimination (backward liveness from roots)
        // -------------------------------------------------------
        void deadCodeElimination(Graph& graph)
        {
            std::unordered_set<Node*> live;
            std::vector<Node*>        work;

            for (Node* pNode : graph.nodes)
            {
                if (isRoot(pNode))
                {
                    work.push_back(pNode);
                    live.insert(pNode);
                }
            }

            if (graph.pStart != nullptr)
                live.insert(graph.pStart);

            while (!work.empty())
            {
                Node* pCur = work.back();
                work.pop_back();

                for (Node* pIn : pCur->inputs)
                {
                    if (pIn && !live.count(pIn))
                    {
                        live.insert(pIn);
                        work.push_back(pIn);
                    }
                }
            }

            std::vector<Node*> kept;
            kept.reserve(graph.nodes.size());
            for (Node* pNode : graph.nodes)
            {
                if (live.count(pNode))
                {
                    kept.push_back(pNode);
                }
                else
                {
                    delete pNode;
                }
            }
            graph.nodes.swap(kept);
        }

        bool isRoot(const Node* pNode) const
        {
            switch (pNode->kind)
            {
                case NodeKind::Ret:
                case NodeKind::Store:
                case NodeKind::Call:
                case NodeKind::Br:
                case NodeKind::CondBr:
                    return true;
                default:
                    return false;
            }
        }
    };

} // namespace llgo
