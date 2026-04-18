/* LLGO Sea-of-Nodes Builder */

#pragma once
#include "frontend.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cctype>

namespace llgo
{
    enum class NodeKind
    {
        Start,
        Merge,

        Add, Sub, Mul, Div, Mod,
        Icmp, Fcmp,
        Load, Store, Gep,
        Phi,
        Br, CondBr, Ret,
        Call,
        ConstInt, ConstFloat, ConstString,
        Undef
    };

    struct Node
    {
        std::uint32_t id;
        NodeKind kind;
        frontend::Type resultType;
        std::string resultName;
        std::vector<Node*> inputs;
    };

    struct Graph
    {
        std::vector<Node*> nodes;
        Node* pStart = nullptr;
    };

    class SONBuilder
    {
    public:
        SONBuilder() = default;

        void build(const frontend::Module& module)
        {
            m_graph.nodes.clear();
            m_graph.pStart = nullptr;
            m_valueMap.clear();
            m_controlMap.clear();
            m_memoryMap.clear();
            m_pCurrentMemory = nullptr;
            m_currentBlock.clear();
            m_nextId = 0;

            for (const auto& fn : module.functions)
            {
                buildFunction(fn);
            }
        }

        const Graph& getGraph() const
        {
            return m_graph;
        }

    private:
        Graph m_graph;

        std::unordered_map<std::string, Node*> m_valueMap;
        std::unordered_map<std::string, Node*> m_controlMap;
        std::unordered_map<std::string, Node*> m_memoryMap;

        Node* m_pCurrentMemory = nullptr;
        std::string m_currentBlock;
        std::uint32_t m_nextId = 0;

        static bool isIntegerLiteral(const std::string& s)
        {
            if (s.empty())
                return false;

            std::size_t i = 0;
            if (s[0] == '-' || s[0] == '+')
            {
                if (s.size() == 1)
                    return false;
                i = 1;
            }

            for (; i < s.size(); ++i)
            {
                if (!std::isdigit(static_cast<unsigned char>(s[i])))
                    return false;
            }
            return true;
        }

        void buildFunction(const frontend::Function& fn)
        {
            Node* pStart = createNode(NodeKind::Start, fn.returnType, fn.name);
            m_graph.pStart = pStart;
            m_controlMap[fn.name + ".entry"] = pStart;
            m_pCurrentMemory = pStart;

            // Parameters: keep current contract (all mapped to Start)
            for (const auto& param : fn.params)
            {
                if (!param.name.empty())
                {
                    m_valueMap[param.name] = pStart;
                }
            }

            for (const auto& block : fn.blocks)
            {
                buildBlock(block);
            }
        }

        void buildBlock(const frontend::Block& block)
        {
            m_currentBlock = block.name;

            Node* pMerge = createNode(NodeKind::Merge, frontend::Type::boolean, block.name);
            m_controlMap[block.name] = pMerge;

            for (const auto& instr : block.instructions)
            {
                buildInstr(instr);
            }
        }

        void buildInstr(const frontend::Instr& instr)
        {
            // Explicit Const* instructions are not part of the intended frontend contract.
            // Ignore them defensively instead of letting later passes crash.
            if (instr.kind == frontend::InstrKind::ConstInt ||
                instr.kind == frontend::InstrKind::ConstFloat ||
                instr.kind == frontend::InstrKind::ConstString)
            {
                return;
            }

            NodeKind kind = mapInstrKind(instr.kind);
            Node* pNode = createNode(kind, instr.resultType, instr.resultName);

            // Data edges
            for (const auto& op : instr.operands)
            {
                Node* pSrc = resolveValue(op);
                if (pSrc != nullptr)
                {
                    pNode->inputs.push_back(pSrc);
                }
                // If not found and not a literal, we simply skip.
                // Contract: frontend must not reference SSA values before they are defined.
            }

            // Control edge
            if (!instr.control.empty())
            {
                Node* pCtrl = resolveControl(instr.control);
                if (pCtrl != nullptr)
                {
                    pNode->inputs.push_back(pCtrl);
                }
            }
            else
            {
                Node* pCtrl = resolveControl(m_currentBlock);
                if (pCtrl != nullptr)
                {
                    pNode->inputs.push_back(pCtrl);
                }
            }

            // Memory edge: explicit + implicit tokens
            Node* pMemInput = nullptr;

            if (!instr.memory.empty())
            {
                pMemInput = resolveMemory(instr.memory);
            }
            else if (instr.kind == frontend::InstrKind::Load ||
                     instr.kind == frontend::InstrKind::Store ||
                     instr.kind == frontend::InstrKind::Call)
            {
                pMemInput = m_pCurrentMemory;
            }

            if (pMemInput != nullptr)
            {
                pNode->inputs.push_back(pMemInput);
            }

            // If instruction changes memory state, update current memory and optionally name it
            if (instr.kind == frontend::InstrKind::Load ||
                instr.kind == frontend::InstrKind::Store ||
                instr.kind == frontend::InstrKind::Call)
            {
                m_pCurrentMemory = pNode;

                if (!instr.resultName.empty())
                {
                    m_memoryMap[instr.resultName] = pNode;
                }
            }

            // Branch targets
            if (instr.kind == frontend::InstrKind::Br)
            {
                Node* pTarget = resolveControl(instr.target);
                if (pTarget != nullptr)
                {
                    pTarget->inputs.push_back(pNode);
                }
            }

            if (instr.kind == frontend::InstrKind::CondBr)
            {
                Node* pTrue = resolveControl(instr.targetTrue);
                Node* pFalse = resolveControl(instr.targetFalse);

                if (pTrue != nullptr)
                {
                    pTrue->inputs.push_back(pNode);
                }
                if (pFalse != nullptr)
                {
                    pFalse->inputs.push_back(pNode);
                }
            }

            // Result binding: SSA def
            if (!instr.resultName.empty())
            {
                m_valueMap[instr.resultName] = pNode;
            }
        }

        Node* createNode(NodeKind kind, frontend::Type resultType, const std::string& resultName)
        {
            Node* pNode = new Node();
            pNode->id = m_nextId++;
            pNode->kind = kind;
            pNode->resultType = resultType;
            pNode->resultName = resultName;
            m_graph.nodes.push_back(pNode);
            return pNode;
        }

        Node* resolveValue(const frontend::Value& v)
        {
            // First try SSA value
            auto it = m_valueMap.find(v.name);
            if (it != m_valueMap.end())
            {
                return it->second;
            }

            // Then treat plain integer literals as implicit ConstInt nodes
            if (isIntegerLiteral(v.name))
            {
                auto litIt = m_valueMap.find("$const.int." + v.name);
                if (litIt != m_valueMap.end())
                {
                    return litIt->second;
                }

                Node* pConst = createNode(NodeKind::ConstInt, v.type, v.name);
                m_valueMap["$const.int." + v.name] = pConst;
                return pConst;
            }

            return nullptr;
        }

        Node* resolveControl(const std::string& name)
        {
            auto it = m_controlMap.find(name);
            if (it != m_controlMap.end())
            {
                return it->second;
            }
            return nullptr;
        }

        Node* resolveMemory(const std::string& name)
        {
            auto it = m_memoryMap.find(name);
            if (it != m_memoryMap.end())
            {
                return it->second;
            }
            return nullptr;
        }

        NodeKind mapInstrKind(frontend::InstrKind k)
        {
            using IK = frontend::InstrKind;

            switch (k)
            {
                case IK::Add:        return NodeKind::Add;
                case IK::Sub:        return NodeKind::Sub;
                case IK::Mul:        return NodeKind::Mul;
                case IK::Div:        return NodeKind::Div;
                case IK::Mod:        return NodeKind::Mod;

                case IK::Icmp:       return NodeKind::Icmp;
                case IK::Fcmp:       return NodeKind::Fcmp;

                case IK::Load:       return NodeKind::Load;
                case IK::Store:      return NodeKind::Store;
                case IK::Gep:        return NodeKind::Gep;

                case IK::Phi:        return NodeKind::Phi;

                case IK::Br:         return NodeKind::Br;
                case IK::CondBr:     return NodeKind::CondBr;
                case IK::Ret:        return NodeKind::Ret;

                case IK::Call:       return NodeKind::Call;

                case IK::ConstInt:   return NodeKind::ConstInt;
                case IK::ConstFloat: return NodeKind::ConstFloat;
                case IK::ConstString:return NodeKind::ConstString;

                case IK::Undef:      return NodeKind::Undef;
            }

            return NodeKind::Undef;
        }
    };
} // namespace llgo
