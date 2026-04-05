/* LLGO ARM64 codegen definition */

#pragma once
#include "lowering.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>

namespace llgo
{
    namespace codegen
    {
        struct ARM64Reloc
        {
            std::uint32_t offset;
            std::uint32_t symbolIndex;
            std::uint32_t kind;
        };

        class ARM64CodeBuffer
        {
        public:
            void emit8(std::uint8_t v)
            {
                m_data.push_back(v);
            }

            void emit32(std::uint32_t v)
            {
                m_data.push_back(static_cast<std::uint8_t>((v >> 0) & 0xFF));
                m_data.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
                m_data.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
                m_data.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
            }

            std::size_t size() const
            {
                return m_data.size();
            }

            std::vector<std::uint8_t>& data()
            {
                return m_data;
            }

            const std::vector<std::uint8_t>& data() const
            {
                return m_data;
            }

            void addReloc(std::uint32_t offset,
                          std::uint32_t symbolIndex,
                          std::uint32_t kind)
            {
                ARM64Reloc r{};
                r.offset      = offset;
                r.symbolIndex = symbolIndex;
                r.kind        = kind;
                m_relocs.push_back(r);
            }

            std::vector<ARM64Reloc>& relocs()
            {
                return m_relocs;
            }

            const std::vector<ARM64Reloc>& relocs() const
            {
                return m_relocs;
            }

        private:
            std::vector<std::uint8_t> m_data;
            std::vector<ARM64Reloc>   m_relocs;
        };

        class ARM64Assembler
        {
        public:
            explicit ARM64Assembler(ARM64CodeBuffer& buf)
                : m_buf(buf)
            {
            }

            void mov_imm64(std::uint8_t rd, std::uint64_t imm)
            {
                bool first = true;
                for (int hw = 0; hw < 4; ++hw)
                {
                    std::uint16_t part =
                        static_cast<std::uint16_t>((imm >> (hw * 16)) & 0xFFFFu);

                    if (!first && part == 0)
                        continue;

                    if (first)
                    {
                        emit_movz(rd, part, hw);
                        first = false;
                    }
                    else
                    {
                        emit_movk(rd, part, hw);
                    }
                }

                if (first)
                    emit_movz(rd, 0, 0);
            }

            void mov_imm64_x0(std::uint64_t imm)
            {
                mov_imm64(0, imm);
            }

            void add_imm(std::uint8_t rd, std::uint8_t rn, std::uint16_t imm12)
            {
                std::uint32_t instr = 0;
                instr |= (0b100010u << 24);
                instr |= ((imm12 & 0xFFFu) << 10);
                instr |= ((rn & 0x1Fu) << 5);
                instr |= (rd & 0x1Fu);
                m_buf.emit32(instr);
            }

            void sub_imm(std::uint8_t rd, std::uint8_t rn, std::uint16_t imm12)
            {
                std::uint32_t instr = 0;
                instr |= (0b110010u << 24);
                instr |= ((imm12 & 0xFFFu) << 10);
                instr |= ((rn & 0x1Fu) << 5);
                instr |= (rd & 0x1Fu);
                m_buf.emit32(instr);
            }

            std::uint32_t bl_reloc_placeholder(std::uint32_t symbolIndex,
                                               std::uint32_t relocKind)
            {
                std::uint32_t offset = static_cast<std::uint32_t>(m_buf.size());
                m_buf.emit32(0x94000000u);
                m_buf.addReloc(offset, symbolIndex, relocKind);
                return offset;
            }

            std::uint32_t b_placeholder()
            {
                std::uint32_t offset = static_cast<std::uint32_t>(m_buf.size());
                m_buf.emit32(0x14000000u);
                return offset;
            }

            static void patch_branch_imm26(std::vector<std::uint8_t>& code,
                                           std::uint32_t instrOffset,
                                           std::int32_t wordOffset)
            {
                std::uint32_t* pInstr =
                    reinterpret_cast<std::uint32_t*>(code.data() + instrOffset);

                std::uint32_t instr = *pInstr;
                instr &= ~0x03FFFFFFu;
                instr |= (static_cast<std::uint32_t>(wordOffset) & 0x03FFFFFFu);
                *pInstr = instr;
            }

            void adrp_add_pair(std::uint8_t rd,
                               std::uint8_t tmp,
                               std::uint32_t symbolIndex,
                               std::uint32_t relocKindADRP,
                               std::uint32_t relocKindADD)
            {
                std::uint32_t adrpOffset = static_cast<std::uint32_t>(m_buf.size());
                m_buf.emit32(0x90000000u | (tmp & 0x1Fu));
                m_buf.addReloc(adrpOffset, symbolIndex, relocKindADRP);

                std::uint32_t addOffset = static_cast<std::uint32_t>(m_buf.size());
                m_buf.emit32(0x91000000u | ((tmp & 0x1Fu) << 5) | (rd & 0x1Fu));
                m_buf.addReloc(addOffset, symbolIndex, relocKindADD);
            }

            void ret()
            {
                m_buf.emit32(0xD65F03C0u);
            }

        private:
            void emit_movz(std::uint8_t rd, std::uint16_t imm16, int hw)
            {
                std::uint32_t instr = 0;
                instr |= (0b110u << 29);
                instr |= (0b100101u << 23);
                instr |= ((hw & 0x3u) << 21);
                instr |= ((imm16 & 0xFFFFu) << 5);
                instr |= (rd & 0x1Fu);
                m_buf.emit32(instr);
            }

            void emit_movk(std::uint8_t rd, std::uint16_t imm16, int hw)
            {
                std::uint32_t instr = 0;
                instr |= (0b111u << 29);
                instr |= (0b100101u << 23);
                instr |= ((hw & 0x3u) << 21);
                instr |= ((imm16 & 0xFFFFu) << 5);
                instr |= (rd & 0x1Fu);
                m_buf.emit32(instr);
            }

            ARM64CodeBuffer& m_buf;
        };

        inline std::uint64_t parseImmediateU64(const std::string& s)
        {
            std::uint64_t v = 0;
            for (char c : s)
            {
                if (c < '0' || c > '9')
                    break;
                v = v * 10 + static_cast<std::uint64_t>(c - '0');
            }
            return v;
        }

        inline void lowerLinearIRToARM64(const LinearIR& ir,
                                         ARM64CodeBuffer& buf)
        {
            ARM64Assembler as(buf);

            std::uint64_t imm = 0;
            bool hasImm = false;

            if (!ir.blocks.empty())
            {
                const LinearBlock& blk = ir.blocks.front();
                for (const LinearInstr& instr : blk.instrs)
                {
                    if (instr.kind == NodeKind::ConstInt)
                    {
                        imm = parseImmediateU64(instr.resultName);
                        hasImm = true;
                    }
                }
            }

            if (!hasImm)
                imm = 0;

            as.mov_imm64_x0(imm);
            as.ret();
        }
    }
}
