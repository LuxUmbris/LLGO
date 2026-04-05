/* LLGO x86-64 codegen definition */

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
        struct ObjectFile
        {
            std::vector<std::uint8_t> data;
        };

        // -------------------------------------------------------
        // Code buffer (byte-level emit)
        // -------------------------------------------------------
        class CodeBuffer
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

            void emit64(std::uint64_t v)
            {
                for (int i = 0; i < 8; ++i)
                    m_data.push_back((v >> (i * 8)) & 0xFF);
            }

            void patch32(std::size_t pos, std::uint32_t v)
            {
                m_data[pos + 0] = (v >>  0) & 0xFF;
                m_data[pos + 1] = (v >>  8) & 0xFF;
                m_data[pos + 2] = (v >> 16) & 0xFF;
                m_data[pos + 3] = (v >> 24) & 0xFF;
            }

            std::size_t size() const { return m_data.size(); }

            const std::vector<std::uint8_t>& data() const { return m_data; }
            std::vector<std::uint8_t>&       data()       { return m_data; }

        private:
            std::vector<std::uint8_t> m_data;
        };

        // -------------------------------------------------------
        // Virtual register (unlimited during selection)
        // -------------------------------------------------------
        using VReg = std::uint32_t;
        static constexpr VReg kInvalidVReg = 0xFFFFFFFFu;

        // x86-64 physical registers (caller-save first for simplicity)
        enum X64Reg : std::uint8_t
        {
            RAX = 0, RCX = 1, RDX = 2, RBX = 3,
            RSP = 4, RBP = 5, RSI = 6, RDI = 7,
            R8  = 8, R9  = 9, R10 =10, R11 =11,
            R12 =12, R13 =13, R14 =14, R15 =15
        };

        // Caller-save registers available for linear-scan allocation
        static constexpr X64Reg k_allocRegs[] = {
            RAX, RCX, RDX, RSI, RDI, R8, R9, R10, R11
        };
        static constexpr int k_numAllocRegs =
            static_cast<int>(sizeof(k_allocRegs) / sizeof(k_allocRegs[0]));

        // -------------------------------------------------------
        // Full-featured x86-64 assembler
        // -------------------------------------------------------
        class X86_64Assembler
        {
        public:
            explicit X86_64Assembler(CodeBuffer& buf)
                : m_buf(buf)
            {}

            // --- Frame ---
            void push_rbp()
            {
                m_buf.emit8(0x55);
            }

            void pop_rbp()
            {
                m_buf.emit8(0x5D);
            }

            void mov_rsp_rbp()
            {
                // MOV rbp, rsp
                m_buf.emit8(0x48); m_buf.emit8(0x89); m_buf.emit8(0xE5);
            }

            void ret()
            {
                m_buf.emit8(0xC3);
            }

            // --- MOV reg, imm64 ---
            void mov_imm64(X64Reg rd, std::uint64_t imm)
            {
                // REX.W + MOVABS
                rexW(rd);
                m_buf.emit8(0xB8 | (rd & 0x7));
                m_buf.emit64(imm);
            }

            void mov_imm32_eax(std::uint32_t imm)
            {
                m_buf.emit8(0xB8);
                m_buf.emit32(imm);
            }

            // --- MOV reg, reg ---
            void mov_rr(X64Reg dst, X64Reg src)
            {
                rexWRB(src, dst);
                m_buf.emit8(0x89);
                modRM(3, src, dst);
            }

            // --- ADD reg, reg ---
            void add_rr(X64Reg dst, X64Reg src)
            {
                rexWRB(src, dst);
                m_buf.emit8(0x01);
                modRM(3, src, dst);
            }

            // --- ADD reg, imm32 ---
            void add_ri(X64Reg dst, std::int32_t imm)
            {
                rexWB(dst);
                if (imm >= -128 && imm <= 127)
                {
                    m_buf.emit8(0x83);
                    modRM(3, 0, dst);
                    m_buf.emit8(static_cast<std::uint8_t>(imm));
                }
                else
                {
                    m_buf.emit8(0x81);
                    modRM(3, 0, dst);
                    m_buf.emit32(static_cast<std::uint32_t>(imm));
                }
            }

            // --- SUB reg, reg ---
            void sub_rr(X64Reg dst, X64Reg src)
            {
                rexWRB(src, dst);
                m_buf.emit8(0x29);
                modRM(3, src, dst);
            }

            // --- SUB reg, imm32 ---
            void sub_ri(X64Reg dst, std::int32_t imm)
            {
                rexWB(dst);
                if (imm >= -128 && imm <= 127)
                {
                    m_buf.emit8(0x83);
                    modRM(3, 5, dst);
                    m_buf.emit8(static_cast<std::uint8_t>(imm));
                }
                else
                {
                    m_buf.emit8(0x81);
                    modRM(3, 5, dst);
                    m_buf.emit32(static_cast<std::uint32_t>(imm));
                }
            }

            // --- IMUL reg, reg ---
            void imul_rr(X64Reg dst, X64Reg src)
            {
                rexWRB(dst, src);
                m_buf.emit8(0x0F); m_buf.emit8(0xAF);
                modRM(3, dst, src);
            }

            // --- CQO (sign-extend rax into rdx:rax) ---
            void cqo()
            {
                m_buf.emit8(0x48); m_buf.emit8(0x99);
            }

            // --- IDIV reg (signed divide rdx:rax by reg) ---
            void idiv_r(X64Reg src)
            {
                rexWB(src);
                m_buf.emit8(0xF7);
                modRM(3, 7, src);
            }

            // --- SHL reg, cl ---
            void shl_rcl(X64Reg dst)
            {
                rexWB(dst);
                m_buf.emit8(0xD3);
                modRM(3, 4, dst);
            }

            // --- SHL reg, imm8 ---
            void shl_ri(X64Reg dst, std::uint8_t shift)
            {
                rexWB(dst);
                m_buf.emit8(0xC1);
                modRM(3, 4, dst);
                m_buf.emit8(shift);
            }

            // --- SAR reg, imm8 ---
            void sar_ri(X64Reg dst, std::uint8_t shift)
            {
                rexWB(dst);
                m_buf.emit8(0xC1);
                modRM(3, 7, dst);
                m_buf.emit8(shift);
            }

            // --- CMP reg, reg ---
            void cmp_rr(X64Reg lhs, X64Reg rhs)
            {
                rexWRB(rhs, lhs);
                m_buf.emit8(0x3B);
                modRM(3, lhs, rhs);
            }

            // --- CALL rel32 placeholder ---
            std::size_t call_rel32_placeholder()
            {
                m_buf.emit8(0xE8);
                std::size_t pos = m_buf.size();
                m_buf.emit32(0);
                return pos;
            }

            // --- JMP rel32 placeholder ---
            std::size_t jmp_rel32_placeholder()
            {
                m_buf.emit8(0xE9);
                std::size_t pos = m_buf.size();
                m_buf.emit32(0);
                return pos;
            }

            // --- JE rel32 placeholder ---
            std::size_t je_rel32_placeholder()
            {
                m_buf.emit8(0x0F); m_buf.emit8(0x84);
                std::size_t pos = m_buf.size();
                m_buf.emit32(0);
                return pos;
            }

            // --- JNE rel32 placeholder ---
            std::size_t jne_rel32_placeholder()
            {
                m_buf.emit8(0x0F); m_buf.emit8(0x85);
                std::size_t pos = m_buf.size();
                m_buf.emit32(0);
                return pos;
            }

            // Patch a previously emitted rel32 offset
            void patch_rel32(std::size_t patchPos)
            {
                std::int32_t rel = static_cast<std::int32_t>(
                    m_buf.size() - (patchPos + 4));
                m_buf.patch32(patchPos, static_cast<std::uint32_t>(rel));
            }

        private:
            // REX.W prefix (operand size override)
            void rexW(X64Reg reg)
            {
                std::uint8_t rex = 0x48;
                if (reg >= 8) rex |= 0x01; // REX.B
                m_buf.emit8(rex);
            }

            // REX.W + REX.R (reg) + REX.B (rm)
            void rexWRB(X64Reg r, X64Reg rm)
            {
                std::uint8_t rex = 0x48;
                if (r  >= 8) rex |= 0x04; // REX.R
                if (rm >= 8) rex |= 0x01; // REX.B
                m_buf.emit8(rex);
            }

            void rexWB(X64Reg rm)
            {
                std::uint8_t rex = 0x48;
                if (rm >= 8) rex |= 0x01;
                m_buf.emit8(rex);
            }

            // ModRM byte: mod=3 (register), reg, rm
            void modRM(std::uint8_t mod, std::uint8_t reg, std::uint8_t rm)
            {
                m_buf.emit8(static_cast<std::uint8_t>(
                    ((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7)));
            }

            CodeBuffer& m_buf;
        };

        // -------------------------------------------------------
        // Linear-scan register allocator (greedy, SSA-like names)
        // -------------------------------------------------------
        class X86_64RegAlloc
        {
        public:
            // Map SSA name → physical register
            // Returns kInvalidVReg (0xFF) for spilled (not yet implemented: spill to stack)
            std::unordered_map<std::string, X64Reg> allocate(const LinearIR& ir)
            {
                std::unordered_map<std::string, X64Reg> assignment;
                bool taken[k_numAllocRegs] = {};
                int  nextSlot              = 0;

                for (const LinearBlock& blk : ir.blocks)
                {
                    for (const LinearInstr& instr : blk.instrs)
                    {
                        if (instr.resultName.empty())
                            continue;

                        // Params get first k registers
                        if (instr.kind == NodeKind::Start)
                            continue;

                        // Assign next free reg
                        for (int i = 0; i < k_numAllocRegs; ++i)
                        {
                            int slot = (nextSlot + i) % k_numAllocRegs;
                            if (!taken[slot])
                            {
                                taken[slot]                    = true;
                                assignment[instr.resultName]  = k_allocRegs[slot];
                                nextSlot                       = (slot + 1) % k_numAllocRegs;
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
        inline std::uint32_t parseImmediateU32(const std::string& s)
        {
            std::uint64_t v = 0;
            for (char c : s)
            {
                if (c < '0' || c > '9')
                    break;
                v = v * 10 + static_cast<std::uint64_t>(c - '0');
            }
            return static_cast<std::uint32_t>(v);
        }

        inline std::int64_t parseImmediateS64(const std::string& s)
        {
            if (s.empty())
                return 0;

            bool neg = (s[0] == '-');
            std::int64_t v = 0;

            for (std::size_t i = neg ? 1 : 0; i < s.size(); ++i)
            {
                if (s[i] < '0' || s[i] > '9')
                    break;
                v = v * 10 + static_cast<std::int64_t>(s[i] - '0');
            }

            return neg ? -v : v;
        }

        // Parse strength-reduction shift tag "shl:N"
        inline bool parseShlTag(const std::string& s, int& shift)
        {
            if (s.size() > 4 && s[0]=='s' && s[1]=='h' && s[2]=='l' && s[3]==':')
            {
                shift = std::stoi(s.substr(4));
                return true;
            }
            return false;
        }

        // -------------------------------------------------------
        // Full IR lowering to x86-64
        // -------------------------------------------------------
        inline void lowerLinearIRToX86_64(const LinearIR& ir, CodeBuffer& buf)
        {
            X86_64Assembler as(buf);
            X86_64RegAlloc  ra;

            if (ir.blocks.empty())
            {
                // mov eax, 0; ret
                as.mov_imm64(RAX, 0);
                as.ret();
                return;
            }

            auto regMap = ra.allocate(ir);

            // Prologue
            as.push_rbp();
            as.mov_rsp_rbp();

            // Block-label → code offset (for branch patching)
            std::unordered_map<std::string, std::size_t> blockOffset;
            // Pending forward branch patches: (patchPos, targetBlockName)
            std::vector<std::pair<std::size_t, std::string>> patches;

            for (const LinearBlock& blk : ir.blocks)
            {
                blockOffset[blk.name] = buf.size();

                for (const LinearInstr& instr : blk.instrs)
                {
                    auto findReg = [&](const std::string& name) -> X64Reg
                    {
                        auto it = regMap.find(name);
                        return (it != regMap.end()) ? it->second : RAX;
                    };

                    switch (instr.kind)
                    {
                        case NodeKind::ConstInt:
                        {
                            if (instr.resultName.empty())
                                break;

                            // Check strength-reduction tag
                            int shift = 0;
                            if (parseShlTag(instr.resultName, shift))
                                break; // handled at usage site

                            X64Reg dst = findReg(instr.resultName);
                            std::int64_t imm = parseImmediateS64(instr.resultName);
                            as.mov_imm64(dst, static_cast<std::uint64_t>(imm));
                            break;
                        }

                        case NodeKind::Add:
                        {
                            if (instr.operandNames.size() < 2)
                                break;

                            // Detect strength-reduced shift
                            int shift = 0;
                            if (parseShlTag(instr.operandNames[1], shift))
                            {
                                X64Reg dst = findReg(instr.resultName);
                                X64Reg src = findReg(instr.operandNames[0]);
                                as.mov_rr(dst, src);
                                as.shl_ri(dst, static_cast<std::uint8_t>(shift));
                            }
                            else
                            {
                                X64Reg dst = findReg(instr.resultName);
                                X64Reg lhs = findReg(instr.operandNames[0]);
                                X64Reg rhs = findReg(instr.operandNames[1]);
                                as.mov_rr(dst, lhs);
                                as.add_rr(dst, rhs);
                            }
                            break;
                        }

                        case NodeKind::Sub:
                        {
                            if (instr.operandNames.size() < 2)
                                break;
                            X64Reg dst = findReg(instr.resultName);
                            X64Reg lhs = findReg(instr.operandNames[0]);
                            X64Reg rhs = findReg(instr.operandNames[1]);
                            as.mov_rr(dst, lhs);
                            as.sub_rr(dst, rhs);
                            break;
                        }

                        case NodeKind::Mul:
                        {
                            if (instr.operandNames.size() < 2)
                                break;
                            X64Reg dst = findReg(instr.resultName);
                            X64Reg lhs = findReg(instr.operandNames[0]);
                            X64Reg rhs = findReg(instr.operandNames[1]);
                            as.mov_rr(dst, lhs);
                            as.imul_rr(dst, rhs);
                            break;
                        }

                        case NodeKind::Div:
                        {
                            if (instr.operandNames.size() < 2)
                                break;
                            // IDIV: load lhs into RAX, sign-extend, divide by rhs
                            X64Reg dst = findReg(instr.resultName);
                            X64Reg lhs = findReg(instr.operandNames[0]);
                            X64Reg rhs = findReg(instr.operandNames[1]);
                            as.mov_rr(RAX, lhs);
                            // Use RCX as divisor if rhs == RAX or RDX
                            X64Reg divisor = rhs;
                            if (rhs == RAX || rhs == RDX)
                            {
                                as.mov_rr(RCX, rhs);
                                divisor = RCX;
                            }
                            as.cqo();
                            as.idiv_r(divisor);
                            if (dst != RAX)
                                as.mov_rr(dst, RAX);
                            break;
                        }

                        case NodeKind::Mod:
                        {
                            if (instr.operandNames.size() < 2)
                                break;
                            // Same as DIV but result is in RDX
                            X64Reg dst = findReg(instr.resultName);
                            X64Reg lhs = findReg(instr.operandNames[0]);
                            X64Reg rhs = findReg(instr.operandNames[1]);
                            as.mov_rr(RAX, lhs);
                            X64Reg divisor = rhs;
                            if (rhs == RAX || rhs == RDX)
                            {
                                as.mov_rr(RCX, rhs);
                                divisor = RCX;
                            }
                            as.cqo();
                            as.idiv_r(divisor);
                            if (dst != RDX)
                                as.mov_rr(dst, RDX);
                            break;
                        }

                        case NodeKind::Ret:
                        {
                            if (!instr.operandNames.empty())
                            {
                                X64Reg src = findReg(instr.operandNames[0]);
                                if (src != RAX)
                                    as.mov_rr(RAX, src);
                            }
                            // Epilogue + ret
                            as.pop_rbp();
                            as.ret();
                            break;
                        }

                        case NodeKind::Br:
                        {
                            if (!instr.operandNames.empty())
                            {
                                std::size_t patchPos = as.jmp_rel32_placeholder();
                                patches.emplace_back(patchPos, instr.operandNames[0]);
                            }
                            break;
                        }

                        case NodeKind::CondBr:
                        {
                            if (instr.operandNames.size() >= 3)
                            {
                                X64Reg cond = findReg(instr.operandNames[0]);
                                as.cmp_rr(cond, cond); // test for zero
                                std::size_t truePos = as.je_rel32_placeholder();
                                patches.emplace_back(truePos, instr.operandNames[1]);
                                std::size_t falsePos = as.jmp_rel32_placeholder();
                                patches.emplace_back(falsePos, instr.operandNames[2]);
                            }
                            break;
                        }

                        default:
                            break;
                    }
                }
            }

            // Patch forward branches
            for (auto& [patchPos, target] : patches)
            {
                auto it = blockOffset.find(target);
                if (it != blockOffset.end())
                {
                    std::int32_t rel = static_cast<std::int32_t>(
                        it->second - (patchPos + 4));
                    buf.patch32(patchPos, static_cast<std::uint32_t>(rel));
                }
            }

            // Ensure function always terminates
            bool hasRet = false;
            for (const LinearBlock& blk : ir.blocks)
            {
                for (const LinearInstr& instr : blk.instrs)
                {
                    if (instr.kind == NodeKind::Ret)
                    {
                        hasRet = true;
                        break;
                    }
                }
            }

            if (!hasRet)
            {
                as.pop_rbp();
                as.ret();
            }
        }

    } // namespace codegen
} // namespace llgo
