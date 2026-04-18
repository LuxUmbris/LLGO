/* LLGO ELF RISC-V 32 codegen */

#pragma once
#include "codegen_riscv32_def.hpp"
#include "codegen_x86_64_def.hpp" // for ObjectFile
#include <cstring>

namespace llgo
{
    namespace codegen
    {
        class ELFRISCV32Codegen
        {
        public:
            // symbolName: exported function symbol (e.g. "main", "_start")
            void generate(const LinearIR& ir,
                          const std::string& symbolName,
                          ObjectFile& out)
            {
                RISCV32CodeBuffer codeBuf;
                lowerLinearIRToRISCV32(ir, codeBuf);

                const auto& text = codeBuf.data();

                std::vector<std::uint8_t> shstrtab;
                buildShStrTab(shstrtab);

                std::vector<std::uint8_t> strtab;
                std::vector<std::uint8_t> symtab;
                buildSymStrTabs(strtab, symtab, text.size(), symbolName);

                buildELF(out.data, text, shstrtab, strtab, symtab);
            }

        private:
            // 32-bit ELF structures
            struct Elf32_Ehdr
            {
                unsigned char e_ident[16];
                std::uint16_t e_type;
                std::uint16_t e_machine;
                std::uint32_t e_version;
                std::uint32_t e_entry;
                std::uint32_t e_phoff;
                std::uint32_t e_shoff;
                std::uint32_t e_flags;
                std::uint16_t e_ehsize;
                std::uint16_t e_phentsize;
                std::uint16_t e_phnum;
                std::uint16_t e_shentsize;
                std::uint16_t e_shnum;
                std::uint16_t e_shstrndx;
            };

            struct Elf32_Shdr
            {
                std::uint32_t sh_name;
                std::uint32_t sh_type;
                std::uint32_t sh_flags;
                std::uint32_t sh_addr;
                std::uint32_t sh_offset;
                std::uint32_t sh_size;
                std::uint32_t sh_link;
                std::uint32_t sh_info;
                std::uint32_t sh_addralign;
                std::uint32_t sh_entsize;
            };

            struct Elf32_Sym
            {
                std::uint32_t st_name;
                std::uint32_t st_value;
                std::uint32_t st_size;
                std::uint8_t  st_info;
                std::uint8_t  st_other;
                std::uint16_t st_shndx;
            };

            enum : std::uint32_t
            {
                SHT_NULL     = 0,
                SHT_PROGBITS = 1,
                SHT_SYMTAB   = 2,
                SHT_STRTAB   = 3
            };

            // .shstrtab:
            //  0: ""
            //  1: ".text"
            //  7: ".symtab"
            // 15: ".strtab"
            // 23: ".shstrtab"
            void buildShStrTab(std::vector<std::uint8_t>& shstr)
            {
                // NOTE: std::strlen(s) would return 0 here because the table
                // starts with a \0 byte. Use sizeof - 1 to capture all 33 bytes.
                static const char s[] =
                    "\0"
                    ".text\0"
                    ".symtab\0"
                    ".strtab\0"
                    ".shstrtab\0";
                shstr.assign(s, s + sizeof(s) - 1);
            }

            void buildSymStrTabs(std::vector<std::uint8_t>& strtab,
                                 std::vector<std::uint8_t>& symtab,
                                 std::size_t textSize,
                                 const std::string& symbolName)
            {
                strtab.clear();
                strtab.push_back(0x00);

                std::uint32_t nameOffset = static_cast<std::uint32_t>(strtab.size());
                strtab.insert(strtab.end(), symbolName.begin(), symbolName.end());
                strtab.push_back(0x00);

                symtab.resize(2 * sizeof(Elf32_Sym));
                std::memset(symtab.data(), 0, symtab.size());

                Elf32_Sym func{};
                func.st_name  = nameOffset;
                func.st_value = 0;
                func.st_size  = static_cast<std::uint32_t>(textSize);
                func.st_info  = (1u << 4) | 2u; // GLOBAL | FUNC
                func.st_other = 0;
                func.st_shndx = 1; // .text

                std::memcpy(symtab.data() + sizeof(Elf32_Sym), &func, sizeof(func));
            }

            void buildELF(std::vector<std::uint8_t>& out,
                          const std::vector<std::uint8_t>& text,
                          const std::vector<std::uint8_t>& shstr,
                          const std::vector<std::uint8_t>& strtab,
                          const std::vector<std::uint8_t>& symtab)
            {
                out.clear();

                const std::size_t ehdrSize = sizeof(Elf32_Ehdr);
                const std::size_t shdrSize = sizeof(Elf32_Shdr);
                const std::size_t shnum    = 5;

                std::size_t off = ehdrSize + shnum * shdrSize;

                std::size_t textOff   = off; off += text.size();
                std::size_t symOff    = off; off += symtab.size();
                std::size_t strOff    = off; off += strtab.size();
                std::size_t shstrOff  = off; off += shstr.size();

                out.resize(off);
                std::memset(out.data(), 0, out.size());

                Elf32_Ehdr eh{};
                eh.e_ident[0]  = 0x7f; eh.e_ident[1] = 'E';
                eh.e_ident[2]  = 'L';  eh.e_ident[3] = 'F';
                eh.e_ident[4]  = 1;  // 32-bit
                eh.e_ident[5]  = 1;  // little endian
                eh.e_ident[6]  = 1;  // version
                eh.e_type      = 1;  // relocatable
                eh.e_machine   = 243; // EM_RISCV
                eh.e_version   = 1;
                eh.e_entry     = 0;
                eh.e_phoff     = 0;
                eh.e_shoff     = static_cast<std::uint32_t>(ehdrSize);
                eh.e_flags     = 0x0001; // RVC not required, ILP32
                eh.e_ehsize    = static_cast<std::uint16_t>(ehdrSize);
                eh.e_phentsize = 0;
                eh.e_phnum     = 0;
                eh.e_shentsize = static_cast<std::uint16_t>(shdrSize);
                eh.e_shnum     = static_cast<std::uint16_t>(shnum);
                eh.e_shstrndx  = 4;
                std::memcpy(out.data(), &eh, sizeof(eh));

                auto writeSH = [&](int idx, Elf32_Shdr& sh)
                {
                    std::memcpy(out.data() + ehdrSize + idx * shdrSize,
                                &sh, sizeof(sh));
                };

                Elf32_Shdr sh_null{};

                Elf32_Shdr sh_text{};
                sh_text.sh_name      = 1;
                sh_text.sh_type      = SHT_PROGBITS;
                sh_text.sh_flags     = 0x6;
                sh_text.sh_offset    = static_cast<std::uint32_t>(textOff);
                sh_text.sh_size      = static_cast<std::uint32_t>(text.size());
                sh_text.sh_addralign = 4;

                Elf32_Shdr sh_sym{};
                sh_sym.sh_name      = 7;
                sh_sym.sh_type      = SHT_SYMTAB;
                sh_sym.sh_offset    = static_cast<std::uint32_t>(symOff);
                sh_sym.sh_size      = static_cast<std::uint32_t>(symtab.size());
                sh_sym.sh_link      = 3;
                sh_sym.sh_info      = 1;
                sh_sym.sh_addralign = 4;
                sh_sym.sh_entsize   = sizeof(Elf32_Sym);

                Elf32_Shdr sh_str{};
                sh_str.sh_name      = 15;
                sh_str.sh_type      = SHT_STRTAB;
                sh_str.sh_offset    = static_cast<std::uint32_t>(strOff);
                sh_str.sh_size      = static_cast<std::uint32_t>(strtab.size());
                sh_str.sh_addralign = 1;

                Elf32_Shdr sh_shstr{};
                sh_shstr.sh_name      = 23;
                sh_shstr.sh_type      = SHT_STRTAB;
                sh_shstr.sh_offset    = static_cast<std::uint32_t>(shstrOff);
                sh_shstr.sh_size      = static_cast<std::uint32_t>(shstr.size());
                sh_shstr.sh_addralign = 1;

                writeSH(0, sh_null);
                writeSH(1, sh_text);
                writeSH(2, sh_sym);
                writeSH(3, sh_str);
                writeSH(4, sh_shstr);

                std::memcpy(out.data() + textOff,  text.data(),   text.size());
                std::memcpy(out.data() + symOff,   symtab.data(), symtab.size());
                std::memcpy(out.data() + strOff,   strtab.data(), strtab.size());
                std::memcpy(out.data() + shstrOff, shstr.data(),  shstr.size());
            }
        };

    } // namespace codegen
} // namespace llgo
