/* LLGO RISC-V 32 codegen definition */

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
        // -------------------------------------------------------
        // RISC-V 32 code buffer
        // -------------------------------------------------------
        class RISCV32CodeBuffer
        {
        public:
            void emit8(std::uint8_t v)  { m_data.push_back(v); }

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

        private:
            std::vector<std::uint8_t> m_data;
        };

        // -------------------------------------------------------
        // RISC-V 32 register set (same ABI names, 32-bit wide)
        // -------------------------------------------------------
        enum RV32Reg : std::uint8_t
        {
            RV32_X0  = 0,  // zero
            RV32_X1  = 1,  // ra
            RV32_X2  = 2,  // sp
            RV32_X5  = 5,  // t0
            RV32_X6  = 6,  // t1
            RV32_X7  = 7,  // t2
            RV32_X10 = 10, // a0 / return value
            RV32_X11 = 11, // a1
            RV32_X12 = 12, // a2
            RV32_X13 = 13, // a3
            RV32_X14 = 14, // a4
            RV32_X15 = 15, // a5
            RV32_X16 = 16, // a6
            RV32_X17 = 17, // a7
            RV32_X28 = 28, // t3
            RV32_X29 = 29, // t4
            RV32_X30 = 30, // t5
            RV32_X31 = 31  // t6
        };

        static constexpr RV32Reg k_rv32AllocRegs[] = {
            RV32_X10, RV32_X11, RV32_X12, RV32_X13,
            RV32_X14, RV32_X15, RV32_X16, RV32_X17,
            RV32_X5,  RV32_X6,  RV32_X7,
            RV32_X28, RV32_X29, RV32_X30, RV32_X31
        };
        static constexpr int k_numRV32AllocRegs =
            static_cast<int>(sizeof(k_rv32AllocRegs) / sizeof(k_rv32AllocRegs[0]));

        // -------------------------------------------------------
        // RISC-V 32 assembler
        // -------------------------------------------------------
        class RISCV32Assembler
        {
        public:
            explicit RISCV32Assembler(RISCV32CodeBuffer& buf)
                : m_buf(buf)
            {}

            // LI (load immediate, max 32-bit value via LUI+ADDI)
            void li(RV32Reg rd, std::int32_t imm)
            {
                std::uint32_t lo12 = static_cast<std::uint32_t>(imm & 0xFFF);
                std::int32_t  lo   = static_cast<std::int32_t>(lo12);
                if (lo >= 2048) lo -= 4096;

                std::uint32_t hi20 = (static_cast<std::uint32_t>(imm) + 0x800u) >> 12;

                if (hi20 != 0)
                    m_buf.emit32(encodeLUI(rd, hi20));
                else
                    m_buf.emit32(encodeADDI(rd, RV32_X0, 0));

                m_buf.emit32(encodeADDI(rd, rd, lo));
            }

            void mv(RV32Reg rd, RV32Reg rs)
            {
                m_buf.emit32(encodeADD(rd, rs, RV32_X0));
            }

            void add(RV32Reg rd, RV32Reg rs1, RV32Reg rs2)
            {
                m_buf.emit32(encodeADD(rd, rs1, rs2));
            }

            void sub(RV32Reg rd, RV32Reg rs1, RV32Reg rs2)
            {
                m_buf.emit32(encodeSUB(rd, rs1, rs2));
            }

            void mul(RV32Reg rd, RV32Reg rs1, RV32Reg rs2)
            {
                m_buf.emit32(encodeMUL(rd, rs1, rs2));
            }

            void div(RV32Reg rd, RV32Reg rs1, RV32Reg rs2)
            {
                m_buf.emit32(encodeDIV(rd, rs1, rs2));
            }

            void rem(RV32Reg rd, RV32Reg rs1, RV32Reg rs2)
            {
                m_buf.emit32(encodeREM(rd, rs1, rs2));
            }

            void slli(RV32Reg rd, RV32Reg rs, std::uint8_t shamt)
            {
                // shamt is 5-bit for RV32
                std::uint32_t instr = ((static_cast<std::uint32_t>(shamt) & 0x1F) << 20)
                                    | (static_cast<std::uint32_t>(rs) << 15)
                                    | (0x1u << 12)
                                    | (static_cast<std::uint32_t>(rd) <<  7)
                                    | 0x13u;
                m_buf.emit32(instr);
            }

            std::size_t jal_placeholder()
            {
                std::size_t pos = m_buf.size();
                m_buf.emit32(encodeJAL(RV32_X0, 0));
                return pos;
            }

            void patch_jal(std::size_t pos, std::int32_t byteOffset)
            {
                m_buf.patch32(pos, encodeJAL(RV32_X0, byteOffset));
            }

            void ret()
            {
                // JALR x0, x1, 0
                m_buf.emit32(encodeJALR(RV32_X0, RV32_X1, 0));
            }

        private:
            static std::uint32_t encodeR(std::uint8_t funct7, std::uint8_t rs2,
                                          std::uint8_t rs1, std::uint8_t funct3,
                                          std::uint8_t rd, std::uint8_t opcode)
            {
                return (static_cast<std::uint32_t>(funct7) << 25)
                     | (static_cast<std::uint32_t>(rs2)    << 20)
                     | (static_cast<std::uint32_t>(rs1)    << 15)
                     | (static_cast<std::uint32_t>(funct3) << 12)
                     | (static_cast<std::uint32_t>(rd)     <<  7)
                     | opcode;
            }

            static std::uint32_t encodeADD(RV32Reg rd, RV32Reg rs1, RV32Reg rs2)
            { return encodeR(0x00, rs2, rs1, 0x0, rd, 0x33); }

            static std::uint32_t encodeSUB(RV32Reg rd, RV32Reg rs1, RV32Reg rs2)
            { return encodeR(0x20, rs2, rs1, 0x0, rd, 0x33); }

            static std::uint32_t encodeMUL(RV32Reg rd, RV32Reg rs1, RV32Reg rs2)
            { return encodeR(0x01, rs2, rs1, 0x0, rd, 0x33); }

            static std::uint32_t encodeDIV(RV32Reg rd, RV32Reg rs1, RV32Reg rs2)
            { return encodeR(0x01, rs2, rs1, 0x4, rd, 0x33); }

            static std::uint32_t encodeREM(RV32Reg rd, RV32Reg rs1, RV32Reg rs2)
            { return encodeR(0x01, rs2, rs1, 0x6, rd, 0x33); }

            static std::uint32_t encodeLUI(RV32Reg rd, std::uint32_t imm20)
            {
                return (imm20 << 12)
                     | (static_cast<std::uint32_t>(rd) << 7)
                     | 0x37u;
            }

            static std::uint32_t encodeADDI(RV32Reg rd, RV32Reg rs1, std::int32_t imm12)
            {
                std::uint32_t imm = static_cast<std::uint32_t>(imm12 & 0xFFF);
                return (imm << 20)
                     | (static_cast<std::uint32_t>(rs1) << 15)
                     | (0b000u << 12)
                     | (static_cast<std::uint32_t>(rd)  <<  7)
                     | 0x13u;
            }

            static std::uint32_t encodeJAL(RV32Reg rd, std::int32_t imm)
            {
                std::uint32_t u = 0;
                std::int32_t  v = imm >> 1;
                u |= ((v >> 20) & 0x1)   << 31;
                u |= ((v >>  1) & 0x3FF) << 21;
                u |= ((v >> 11) & 0x1)   << 20;
                u |= ((v >> 12) & 0xFF)  << 12;
                return u | (static_cast<std::uint32_t>(rd) << 7) | 0x6Fu;
            }

            static std::uint32_t encodeJALR(RV32Reg rd, RV32Reg rs1, std::int32_t imm12)
            {
                std::uint32_t imm = static_cast<std::uint32_t>(imm12 & 0xFFF);
                return (imm << 20)
                     | (static_cast<std::uint32_t>(rs1) << 15)
                     | (0b000u << 12)
                     | (static_cast<std::uint32_t>(rd)  <<  7)
                     | 0x67u;
            }

            RISCV32CodeBuffer& m_buf;
        };

        // -------------------------------------------------------
        // Greedy register allocator for RISC-V 32
        // -------------------------------------------------------
        class RV32RegAlloc
        {
        public:
            std::unordered_map<std::string, RV32Reg> allocate(const LinearIR& ir)
            {
                std::unordered_map<std::string, RV32Reg> assignment;
                bool taken[k_numRV32AllocRegs] = {};
                int  nextSlot = 0;

                for (const LinearBlock& blk : ir.blocks)
                {
                    for (const LinearInstr& instr : blk.instrs)
                    {
                        if (instr.resultName.empty()) continue;
                        if (instr.kind == NodeKind::Start) continue;

                        for (int i = 0; i < k_numRV32AllocRegs; ++i)
                        {
                            int slot = (nextSlot + i) % k_numRV32AllocRegs;
                            if (!taken[slot])
                            {
                                taken[slot]                   = true;
                                assignment[instr.resultName]  = k_rv32AllocRegs[slot];
                                nextSlot = (slot + 1) % k_numRV32AllocRegs;
                                break;
                            }
                        }
                    }
                }
                return assignment;
            }
        };

        // -------------------------------------------------------
        // IR lowering to RISC-V 32
        // -------------------------------------------------------
        inline void lowerLinearIRToRISCV32(const LinearIR& ir,
                                           RISCV32CodeBuffer& buf)
        {
            RISCV32Assembler as(buf);
            RV32RegAlloc     ra;

            if (ir.blocks.empty())
            {
                as.li(RV32_X10, 0);
                as.ret();
                return;
            }

            auto regMap = ra.allocate(ir);

            std::unordered_map<std::string, std::size_t> blockOffset;
            std::vector<std::pair<std::size_t, std::string>> patches;

            auto findReg = [&](const std::string& name) -> RV32Reg
            {
                auto it = regMap.find(name);
                return (it != regMap.end()) ? it->second : RV32_X10;
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
                            RV32Reg dst = findReg(instr.resultName);
                            std::int32_t imm = static_cast<std::int32_t>(
                                parseImmediateU64_rv64(instr.resultName));
                            as.li(dst, imm);
                            break;
                        }

                        case NodeKind::Add:
                        {
                            if (instr.operandNames.size() < 2) break;
                            int shift = 0;
                            if (parseRVShlTag(instr.operandNames[1], shift))
                            {
                                RV32Reg dst = findReg(instr.resultName);
                                RV32Reg src = findReg(instr.operandNames[0]);
                                as.mv(dst, src);
                                as.slli(dst, dst, static_cast<std::uint8_t>(shift));
                            }
                            else
                            {
                                RV32Reg dst = findReg(instr.resultName);
                                RV32Reg rs1 = findReg(instr.operandNames[0]);
                                RV32Reg rs2 = findReg(instr.operandNames[1]);
                                as.mv(dst, rs1);
                                as.add(dst, dst, rs2);
                            }
                            break;
                        }

                        case NodeKind::Sub:
                        {
                            if (instr.operandNames.size() < 2) break;
                            RV32Reg dst = findReg(instr.resultName);
                            as.mv(dst, findReg(instr.operandNames[0]));
                            as.sub(dst, dst, findReg(instr.operandNames[1]));
                            break;
                        }

                        case NodeKind::Mul:
                        {
                            if (instr.operandNames.size() < 2) break;
                            RV32Reg dst = findReg(instr.resultName);
                            as.mv(dst, findReg(instr.operandNames[0]));
                            as.mul(dst, dst, findReg(instr.operandNames[1]));
                            break;
                        }

                        case NodeKind::Div:
                        {
                            if (instr.operandNames.size() < 2) break;
                            RV32Reg dst = findReg(instr.resultName);
                            as.mv(dst, findReg(instr.operandNames[0]));
                            as.div(dst, dst, findReg(instr.operandNames[1]));
                            break;
                        }

                        case NodeKind::Mod:
                        {
                            if (instr.operandNames.size() < 2) break;
                            RV32Reg dst = findReg(instr.resultName);
                            as.mv(dst, findReg(instr.operandNames[0]));
                            as.rem(dst, dst, findReg(instr.operandNames[1]));
                            break;
                        }

                        case NodeKind::Ret:
                        {
                            if (!instr.operandNames.empty())
                            {
                                RV32Reg src = findReg(instr.operandNames[0]);
                                if (src != RV32_X10) as.mv(RV32_X10, src);
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

                        default: break;
                    }
                }
            }

            for (auto& [pos, target] : patches)
            {
                auto it = blockOffset.find(target);
                if (it != blockOffset.end())
                {
                    std::int32_t rel = static_cast<std::int32_t>(it->second - pos);
                    as.patch_jal(pos, rel);
                }
            }

            bool hasRet = false;
            for (const LinearBlock& blk : ir.blocks)
                for (const LinearInstr& i2 : blk.instrs)
                    if (i2.kind == NodeKind::Ret) hasRet = true;

            if (!hasRet) { as.li(RV32_X10, 0); as.ret(); }
        }

    } // namespace codegen
} // namespace llgo
