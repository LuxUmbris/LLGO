/* LLGO RISC-V 64 codegen definition */

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
        struct RISCVReloc
        {
            std::uint32_t offset;
            std::uint32_t symbolIndex;
            std::uint32_t kind;
        };

        // -------------------------------------------------------
        // Code buffer
        // -------------------------------------------------------
        class RISCVCodeBuffer
        {
        public:
            void emit8(std::uint8_t v)
            {
                m_data.push_back(v);
            }

            void emit32(std::uint32_t v)
            {
                m_data.push_back((v >>  0) & 0xFF);
                m_data.push_back((v >>  8) & 0xFF);
                m_data.push_back((v >> 16) & 0xFF);
                m_data.push_back((v >> 24) & 0xFF);
            }

            void patch32(std::size_t pos, std::uint32_t v)
            {
                m_data[pos+0] = (v >>  0) & 0xFF;
                m_data[pos+1] = (v >>  8) & 0xFF;
                m_data[pos+2] = (v >> 16) & 0xFF;
                m_data[pos+3] = (v >> 24) & 0xFF;
            }

            std::size_t size() const { return m_data.size(); }

            std::vector<std::uint8_t>&       data()       { return m_data; }
            const std::vector<std::uint8_t>& data() const { return m_data; }

            void addReloc(std::uint32_t offset,
                          std::uint32_t symbolIndex,
                          std::uint32_t kind)
            {
                RISCVReloc r{};
                r.offset      = offset;
                r.symbolIndex = symbolIndex;
                r.kind        = kind;
                m_relocs.push_back(r);
            }

            std::vector<RISCVReloc>&       relocs()       { return m_relocs; }
            const std::vector<RISCVReloc>& relocs() const { return m_relocs; }

        private:
            std::vector<std::uint8_t> m_data;
            std::vector<RISCVReloc>   m_relocs;
        };

        // -------------------------------------------------------
        // RISC-V 64 registers
        // -------------------------------------------------------
        enum RV64Reg : std::uint8_t
        {
            X0  = 0,  // zero
            X1  = 1,  // ra (return address)
            X2  = 2,  // sp
            X3  = 3,  // gp
            X4  = 4,  // tp
            X5  = 5,  // t0
            X6  = 6,  // t1
            X7  = 7,  // t2
            X8  = 8,  // s0/fp
            X9  = 9,  // s1
            X10 = 10, // a0 (arg0 / return value)
            X11 = 11, // a1
            X12 = 12, // a2
            X13 = 13, // a3
            X14 = 14, // a4
            X15 = 15, // a5
            X16 = 16, // a6
            X17 = 17, // a7
            X28 = 28, // t3
            X29 = 29, // t4
            X30 = 30, // t5
            X31 = 31  // t6
        };

        // Allocatable caller-save regs: t0-t2, a0-a7, t3-t6
        static constexpr RV64Reg k_rv64AllocRegs[] = {
            X10, X11, X12, X13, X14, X15, X16, X17,
            X5,  X6,  X7,  X28, X29, X30, X31
        };
        static constexpr int k_numRV64AllocRegs =
            static_cast<int>(sizeof(k_rv64AllocRegs) / sizeof(k_rv64AllocRegs[0]));

        // -------------------------------------------------------
        // RISC-V 64 assembler
        // -------------------------------------------------------
        class RISCVAssembler
        {
        public:
            explicit RISCVAssembler(RISCVCodeBuffer& buf)
                : m_buf(buf)
            {}

            // --- LI (load immediate, full 64-bit sequence) ---
            void li(RV64Reg rd, std::int64_t imm)
            {
                emit_lui_addi_sequence(static_cast<std::uint8_t>(rd),
                                       static_cast<std::uint64_t>(imm));
            }

            void li_x10(std::uint64_t imm)
            {
                emit_lui_addi_sequence(10, imm);
            }

            // --- MV (ADD rd, rs, x0) ---
            void mv(RV64Reg rd, RV64Reg rs)
            {
                m_buf.emit32(encodeADD(
                    static_cast<std::uint8_t>(rd),
                    static_cast<std::uint8_t>(rs),
                    static_cast<std::uint8_t>(X0)));
            }

            // --- ADD ---
            void add(RV64Reg rd, RV64Reg rs1, RV64Reg rs2)
            {
                m_buf.emit32(encodeADD(
                    static_cast<std::uint8_t>(rd),
                    static_cast<std::uint8_t>(rs1),
                    static_cast<std::uint8_t>(rs2)));
            }

            // --- SUB ---
            void sub(RV64Reg rd, RV64Reg rs1, RV64Reg rs2)
            {
                m_buf.emit32(encodeSUB(
                    static_cast<std::uint8_t>(rd),
                    static_cast<std::uint8_t>(rs1),
                    static_cast<std::uint8_t>(rs2)));
            }

            // --- MUL ---
            void mul(RV64Reg rd, RV64Reg rs1, RV64Reg rs2)
            {
                m_buf.emit32(encodeMUL(
                    static_cast<std::uint8_t>(rd),
                    static_cast<std::uint8_t>(rs1),
                    static_cast<std::uint8_t>(rs2)));
            }

            // --- DIV (signed) ---
            void div(RV64Reg rd, RV64Reg rs1, RV64Reg rs2)
            {
                m_buf.emit32(encodeDIV(
                    static_cast<std::uint8_t>(rd),
                    static_cast<std::uint8_t>(rs1),
                    static_cast<std::uint8_t>(rs2)));
            }

            // --- REM (signed) ---
            void rem(RV64Reg rd, RV64Reg rs1, RV64Reg rs2)
            {
                m_buf.emit32(encodeREM(
                    static_cast<std::uint8_t>(rd),
                    static_cast<std::uint8_t>(rs1),
                    static_cast<std::uint8_t>(rs2)));
            }

            // --- SLLI ---
            void slli(RV64Reg rd, RV64Reg rs, std::uint8_t shamt)
            {
                m_buf.emit32(encodeSLLI(
                    static_cast<std::uint8_t>(rd),
                    static_cast<std::uint8_t>(rs),
                    shamt));
            }

            // --- SRAI ---
            void srai(RV64Reg rd, RV64Reg rs, std::uint8_t shamt)
            {
                m_buf.emit32(encodeSRAI(
                    static_cast<std::uint8_t>(rd),
                    static_cast<std::uint8_t>(rs),
                    shamt));
            }

            // --- BEQ branch placeholder ---
            std::uint32_t branch_placeholder()
            {
                std::uint32_t offset = static_cast<std::uint32_t>(m_buf.size());
                m_buf.emit32(encodeBEQ(0, 0, 0));
                return offset;
            }

            // --- JAL call placeholder with reloc ---
            std::uint32_t call_reloc_placeholder(std::uint32_t symbolIndex,
                                                 std::uint32_t relocKind)
            {
                std::uint32_t offset = static_cast<std::uint32_t>(m_buf.size());
                m_buf.emit32(encodeJAL(1, 0));
                m_buf.addReloc(offset, symbolIndex, relocKind);
                return offset;
            }

            // --- JAL x0 (unconditional jump placeholder) ---
            std::size_t jal_placeholder()
            {
                std::size_t pos = m_buf.size();
                m_buf.emit32(encodeJAL(0, 0));
                return pos;
            }

            // Patch a JAL at pos with actual byte offset
            void patch_jal(std::size_t pos, std::int32_t byteOffset)
            {
                std::uint32_t instr = encodeJAL(0, byteOffset);
                m_buf.patch32(pos, instr);
            }

            // Patch a BEQ at instrOffset with byte offset
            static void patch_branch(std::vector<std::uint8_t>& code,
                                     std::uint32_t instrOffset,
                                     std::int32_t byteOffset)
            {
                std::uint32_t* pInstr =
                    reinterpret_cast<std::uint32_t*>(code.data() + instrOffset);
                std::uint32_t instr = *pInstr;
                std::uint32_t imm   = encodeBranchImmediate(byteOffset);
                instr               = (instr & ~branchImmediateMask()) | imm;
                *pInstr             = instr;
            }

            // --- RET (JALR x0, x1, 0) ---
            void ret()
            {
                m_buf.emit32(encodeJALR(0, 1, 0));
            }

        private:
            // R-type
            static std::uint32_t encodeR(std::uint8_t funct7, std::uint8_t rs2,
                                          std::uint8_t rs1,    std::uint8_t funct3,
                                          std::uint8_t rd,     std::uint8_t opcode)
            {
                return (static_cast<std::uint32_t>(funct7) << 25)
                     | (static_cast<std::uint32_t>(rs2)    << 20)
                     | (static_cast<std::uint32_t>(rs1)    << 15)
                     | (static_cast<std::uint32_t>(funct3) << 12)
                     | (static_cast<std::uint32_t>(rd)     <<  7)
                     | opcode;
            }

            static std::uint32_t encodeADD(std::uint8_t rd, std::uint8_t rs1, std::uint8_t rs2)
            {
                return encodeR(0x00, rs2, rs1, 0x0, rd, 0x33);
            }

            static std::uint32_t encodeSUB(std::uint8_t rd, std::uint8_t rs1, std::uint8_t rs2)
            {
                return encodeR(0x20, rs2, rs1, 0x0, rd, 0x33);
            }

            // M-extension: MUL
            static std::uint32_t encodeMUL(std::uint8_t rd, std::uint8_t rs1, std::uint8_t rs2)
            {
                return encodeR(0x01, rs2, rs1, 0x0, rd, 0x33);
            }

            // M-extension: DIV (signed)
            static std::uint32_t encodeDIV(std::uint8_t rd, std::uint8_t rs1, std::uint8_t rs2)
            {
                return encodeR(0x01, rs2, rs1, 0x4, rd, 0x33);
            }

            // M-extension: REM (signed)
            static std::uint32_t encodeREM(std::uint8_t rd, std::uint8_t rs1, std::uint8_t rs2)
            {
                return encodeR(0x01, rs2, rs1, 0x6, rd, 0x33);
            }

            // I-type SLLI (shift left logical immediate, 64-bit shamt field)
            static std::uint32_t encodeSLLI(std::uint8_t rd, std::uint8_t rs, std::uint8_t shamt)
            {
                return ((static_cast<std::uint32_t>(shamt) & 0x3F) << 20)
                     | (static_cast<std::uint32_t>(rs) << 15)
                     | (0x1u << 12)
                     | (static_cast<std::uint32_t>(rd) << 7)
                     | 0x13u;
            }

            // I-type SRAI (shift right arith immediate, 64-bit shamt field)
            static std::uint32_t encodeSRAI(std::uint8_t rd, std::uint8_t rs, std::uint8_t shamt)
            {
                return (0x10u << 26)
                     | ((static_cast<std::uint32_t>(shamt) & 0x3F) << 20)
                     | (static_cast<std::uint32_t>(rs) << 15)
                     | (0x5u << 12)
                     | (static_cast<std::uint32_t>(rd) << 7)
                     | 0x13u;
            }

            static std::uint32_t encodeLUI(std::uint8_t rd, std::uint32_t imm20)
            {
                return (imm20 << 12) | (static_cast<std::uint32_t>(rd) << 7) | 0x37u;
            }

            static std::uint32_t encodeADDI(std::uint8_t rd, std::uint8_t rs1, std::int32_t imm12)
            {
                std::uint32_t imm = static_cast<std::uint32_t>(imm12 & 0xFFF);
                return (imm << 20)
                     | (static_cast<std::uint32_t>(rs1) << 15)
                     | (0b000u << 12)
                     | (static_cast<std::uint32_t>(rd) << 7)
                     | 0x13u;
            }

            static std::uint32_t encodeJAL(std::uint8_t rd, std::int32_t imm)
            {
                return encodeJALImmediate(imm)
                     | (static_cast<std::uint32_t>(rd) << 7)
                     | 0x6Fu;
            }

            static std::uint32_t encodeJALR(std::uint8_t rd, std::uint8_t rs1, std::int32_t imm12)
            {
                std::uint32_t imm = static_cast<std::uint32_t>(imm12 & 0xFFF);
                return (imm << 20)
                     | (static_cast<std::uint32_t>(rs1) << 15)
                     | (0b000u << 12)
                     | (static_cast<std::uint32_t>(rd) << 7)
                     | 0x67u;
            }

            static std::uint32_t encodeBEQ(std::uint8_t rs1, std::uint8_t rs2, std::int32_t imm)
            {
                return encodeBranchImmediate(imm)
                     | (static_cast<std::uint32_t>(rs2) << 20)
                     | (static_cast<std::uint32_t>(rs1) << 15)
                     | (0b000u << 12)
                     | 0x63u;
            }

            static std::uint32_t encodeJALImmediate(std::int32_t imm)
            {
                std::uint32_t u = 0;
                std::int32_t  v = imm >> 1;
                u |= ((v >> 20) & 0x1)   << 31;
                u |= ((v >>  1) & 0x3FF) << 21;
                u |= ((v >> 11) & 0x1)   << 20;
                u |= ((v >> 12) & 0xFF)  << 12;
                return u;
            }

            static std::uint32_t encodeBranchImmediate(std::int32_t imm)
            {
                std::uint32_t u = 0;
                std::int32_t  v = imm >> 1;
                u |= ((v >> 11) & 0x1) <<  7;
                u |= ((v >>  1) & 0xF) <<  8;
                u |= ((v >>  5) & 0x3F) << 25;
                u |= ((v >> 12) & 0x1) << 31;
                return u;
            }

            static constexpr std::uint32_t branchImmediateMask()
            {
                return 0b11111100000000000000111110000000u;
            }

            void emit_lui_addi_sequence(std::uint8_t rd, std::uint64_t imm)
            {
                // Simple 32-bit LUI+ADDI sequence for now
                // For full 64-bit: repeat for upper 32 bits with shifts
                std::uint32_t lo32 = static_cast<std::uint32_t>(imm & 0xFFFFFFFFu);
                std::uint32_t hi20 = (lo32 + 0x800u) >> 12;
                std::int32_t  lo12 = static_cast<std::int32_t>(lo32 & 0xFFF);
                if (lo12 >= 2048) lo12 -= 4096;

                if (hi20 != 0)
                    m_buf.emit32(encodeLUI(rd, hi20));
                else
                    m_buf.emit32(encodeADDI(rd, 0, 0)); // zero out

                m_buf.emit32(encodeADDI(rd, rd, lo12));

                // Handle upper 32 bits if needed
                std::uint32_t hi32 = static_cast<std::uint32_t>((imm >> 32) & 0xFFFFFFFFu);
                if (hi32 != 0)
                {
                    // Use a temporary x5 to hold the upper 32 bits and shift+OR
                    std::uint32_t hi20_upper = (hi32 + 0x800u) >> 12;
                    std::int32_t  lo12_upper = static_cast<std::int32_t>(hi32 & 0xFFF);
                    if (lo12_upper >= 2048) lo12_upper -= 4096;
                    m_buf.emit32(encodeLUI(5, hi20_upper));
                    m_buf.emit32(encodeADDI(5, 5, lo12_upper));
                    m_buf.emit32(encodeSLLI(5, 5, 32));
                    m_buf.emit32(encodeR(0x00, static_cast<std::uint8_t>(rd), 5, 0x0, rd, 0x33));
                }
            }

            RISCVCodeBuffer& m_buf;
        };

        // -------------------------------------------------------
        // Greedy register allocator for RISC-V 64
        // -------------------------------------------------------
        class RV64RegAlloc
        {
        public:
            std::unordered_map<std::string, RV64Reg> allocate(const LinearIR& ir)
            {
                std::unordered_map<std::string, RV64Reg> assignment;
                bool taken[k_numRV64AllocRegs] = {};
                int  nextSlot                  = 0;

                for (const LinearBlock& blk : ir.blocks)
                {
                    for (const LinearInstr& instr : blk.instrs)
                    {
                        if (instr.resultName.empty())
                            continue;
                        if (instr.kind == NodeKind::Start)
                            continue;

                        for (int i = 0; i < k_numRV64AllocRegs; ++i)
                        {
                            int slot = (nextSlot + i) % k_numRV64AllocRegs;
                            if (!taken[slot])
                            {
                                taken[slot]                   = true;
                                assignment[instr.resultName]  = k_rv64AllocRegs[slot];
                                nextSlot                      = (slot + 1) % k_numRV64AllocRegs;
                                break;
                            }
                        }
                    }
                }

                return assignment;
            }
        };

        // -------------------------------------------------------
        // Immediate helpers
        // -------------------------------------------------------
        inline std::uint64_t parseImmediateU64_rv64(const std::string& s)
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

        inline std::int64_t parseImmediateS64_rv(const std::string& s)
        {
            if (s.empty()) return 0;
            bool neg = (s[0] == '-');
            std::int64_t v = 0;
            for (std::size_t i = neg ? 1 : 0; i < s.size(); ++i)
            {
                if (s[i] < '0' || s[i] > '9') break;
                v = v * 10 + static_cast<std::int64_t>(s[i] - '0');
            }
            return neg ? -v : v;
        }

        inline bool parseRVShlTag(const std::string& s, int& shift)
        {
            if (s.size() > 4 && s[0]=='s' && s[1]=='h' && s[2]=='l' && s[3]==':')
            {
                shift = std::stoi(s.substr(4));
                return true;
            }
            return false;
        }

        // -------------------------------------------------------
        // Full IR lowering to RISC-V 64
        // -------------------------------------------------------
        inline void lowerLinearIRToRISCV64(const LinearIR& ir,
                                           RISCVCodeBuffer& buf)
        {
            RISCVAssembler as(buf);
            RV64RegAlloc   ra;

            if (ir.blocks.empty())
            {
                as.li_x10(0);
                as.ret();
                return;
            }

            auto regMap = ra.allocate(ir);

            std::unordered_map<std::string, std::size_t> blockOffset;
            std::vector<std::pair<std::size_t, std::string>> patches;

            auto findReg = [&](const std::string& name) -> RV64Reg
            {
                auto it = regMap.find(name);
                return (it != regMap.end()) ? it->second : X10;
            };

            for (const LinearBlock& blk : ir.blocks)
            {
                blockOffset[blk.name] = buf.size();

                for (const LinearInstr& instr : blk.instrs)
                {
                    switch (instr.kind)
                    {
                        case NodeKind::ConstInt:
                        {
                            if (instr.resultName.empty()) break;
                            int shift = 0;
                            if (parseRVShlTag(instr.resultName, shift)) break;
                            RV64Reg dst  = findReg(instr.resultName);
                            std::int64_t imm = parseImmediateS64_rv(instr.resultName);
                            as.li(dst, imm);
                            break;
                        }

                        case NodeKind::Add:
                        {
                            if (instr.operandNames.size() < 2) break;
                            int shift = 0;
                            if (parseRVShlTag(instr.operandNames[1], shift))
                            {
                                RV64Reg dst = findReg(instr.resultName);
                                RV64Reg src = findReg(instr.operandNames[0]);
                                as.mv(dst, src);
                                as.slli(dst, dst, static_cast<std::uint8_t>(shift));
                            }
                            else
                            {
                                RV64Reg dst = findReg(instr.resultName);
                                RV64Reg rs1 = findReg(instr.operandNames[0]);
                                RV64Reg rs2 = findReg(instr.operandNames[1]);
                                as.mv(dst, rs1);
                                as.add(dst, dst, rs2);
                            }
                            break;
                        }

                        case NodeKind::Sub:
                        {
                            if (instr.operandNames.size() < 2) break;
                            RV64Reg dst = findReg(instr.resultName);
                            RV64Reg rs1 = findReg(instr.operandNames[0]);
                            RV64Reg rs2 = findReg(instr.operandNames[1]);
                            as.mv(dst, rs1);
                            as.sub(dst, dst, rs2);
                            break;
                        }

                        case NodeKind::Mul:
                        {
                            if (instr.operandNames.size() < 2) break;
                            RV64Reg dst = findReg(instr.resultName);
                            RV64Reg rs1 = findReg(instr.operandNames[0]);
                            RV64Reg rs2 = findReg(instr.operandNames[1]);
                            as.mv(dst, rs1);
                            as.mul(dst, dst, rs2);
                            break;
                        }

                        case NodeKind::Div:
                        {
                            if (instr.operandNames.size() < 2) break;
                            RV64Reg dst = findReg(instr.resultName);
                            RV64Reg rs1 = findReg(instr.operandNames[0]);
                            RV64Reg rs2 = findReg(instr.operandNames[1]);
                            as.mv(dst, rs1);
                            as.div(dst, dst, rs2);
                            break;
                        }

                        case NodeKind::Mod:
                        {
                            if (instr.operandNames.size() < 2) break;
                            RV64Reg dst = findReg(instr.resultName);
                            RV64Reg rs1 = findReg(instr.operandNames[0]);
                            RV64Reg rs2 = findReg(instr.operandNames[1]);
                            as.mv(dst, rs1);
                            as.rem(dst, dst, rs2);
                            break;
                        }

                        case NodeKind::Ret:
                        {
                            if (!instr.operandNames.empty())
                            {
                                RV64Reg src = findReg(instr.operandNames[0]);
                                if (src != X10)
                                    as.mv(X10, src);
                            }
                            as.ret();
                            break;
                        }

                        case NodeKind::Br:
                        {
                            if (!instr.operandNames.empty())
                            {
                                std::size_t pos = as.jal_placeholder();
                                patches.emplace_back(pos, instr.operandNames[0]);
                            }
                            break;
                        }

                        default:
                            break;
                    }
                }
            }

            // Patch forward branches
            for (auto& [pos, target] : patches)
            {
                auto it = blockOffset.find(target);
                if (it != blockOffset.end())
                {
                    std::int32_t rel = static_cast<std::int32_t>(
                        it->second - pos);
                    as.patch_jal(pos, rel);
                }
            }

            // Ensure ret if none
            bool hasRet = false;
            for (const LinearBlock& blk : ir.blocks)
                for (const LinearInstr& i2 : blk.instrs)
                    if (i2.kind == NodeKind::Ret) hasRet = true;

            if (!hasRet)
            {
                as.li_x10(0);
                as.ret();
            }
        }

    } // namespace codegen
} // namespace llgo
