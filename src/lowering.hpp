/* LLGO IR lowerer and lineazer */

#pragma once
#include "son_builder.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace llgo
{
    struct LinearInstr
    {
        NodeKind kind;
        frontend::Type type;
        std::string resultName;
        std::vector<std::string> operandNames;
    };

    struct LinearBlock
    {
        std::string name;
        std::vector<LinearInstr> instrs;
    };

    struct LinearIR
    {
        std::vector<LinearBlock> blocks;
    };

    class Lowering
    {
    public:
        Lowering() = default;

        void run(const Graph& graph, LinearIR& out)
        {
            out.blocks.clear();

            // 1. Topological order
            std::vector<Node*> order;
            topoSort(graph, order);

            // 2. Assign block names
            assignBlocks(order);

            // 3. Emit linear IR
            emitLinear(order, out);
        }

    private:
        std::unordered_map<Node*, std::string> m_blockOf;
        std::unordered_map<Node*, std::string> m_tempName;
        std::uint32_t m_tempCounter = 0;

    private:
        // Topological sort (Kahn)
        void topoSort(const Graph& graph, std::vector<Node*>& out)
        {
            std::unordered_map<Node*, int> indeg;

            for (Node* p : graph.nodes)
            {
                indeg[p] = 0;
            }

            for (Node* p : graph.nodes)
            {
                for (Node* in : p->inputs)
                {
                    indeg[p]++;
                }
            }

            std::vector<Node*> work;
            for (auto& kv : indeg)
            {
                if (kv.second == 0)
                {
                    work.push_back(kv.first);
                }
            }

            while (!work.empty())
            {
                Node* p = work.back();
                work.pop_back();
                out.push_back(p);

                for (Node* q : usersOf(graph, p))
                {
                    if (--indeg[q] == 0)
                    {
                        work.push_back(q);
                    }
                }
            }
        }

        std::vector<Node*> usersOf(const Graph& graph, Node* target)
        {
            std::vector<Node*> out;
            for (Node* p : graph.nodes)
            {
                for (Node* in : p->inputs)
                {
                    if (in == target)
                    {
                        out.push_back(p);
                        break;
                    }
                }
            }
            return out;
        }

        // Assign blocks based on control nodes
        void assignBlocks(const std::vector<Node*>& order)
        {
            m_blockOf.clear();
            m_tempName.clear();
            m_tempCounter = 0;

            std::string current = "entry";

            for (Node* p : order)
            {
                if (p->kind == NodeKind::Merge)
                {
                    current = "block_" + std::to_string(p->id);
                }
                m_blockOf[p] = current;
            }
        }

        // Emit linear IR
        void emitLinear(const std::vector<Node*>& order, LinearIR& out)
        {
            std::unordered_map<std::string, LinearBlock*> blockMap;

            for (Node* p : order)
            {
                const std::string& bname = m_blockOf[p];

                LinearBlock* blk = nullptr;
                auto it = blockMap.find(bname);
                if (it == blockMap.end())
                {
                    out.blocks.push_back(LinearBlock{bname});
                    blk = &out.blocks.back();
                    blockMap[bname] = blk;
                }
                else
                {
                    blk = it->second;
                }

                LinearInstr li;
                li.kind = p->kind;
                li.type = p->resultType;
                li.resultName = getName(p);

                for (Node* in : p->inputs)
                {
                    li.operandNames.push_back(getName(in));
                }

                blk->instrs.push_back(std::move(li));
            }
        }

        // Name assignment
        std::string getName(Node* p)
        {
            if (!p->resultName.empty())
            {
                return p->resultName;
            }

            auto it = m_tempName.find(p);
            if (it != m_tempName.end())
            {
                return it->second;
            }

            std::string name = "%t" + std::to_string(m_tempCounter++);
            m_tempName[p] = name;
            return name;
        }
    };
} // namespace llgo
