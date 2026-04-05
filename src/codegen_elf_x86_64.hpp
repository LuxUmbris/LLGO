/* LLGO ELF x86-64 codegen */

#pragma once
#include "codegen_x86_64_def.hpp"
#include <cstring>

namespace llgo
{
    namespace codegen
    {
        class ELF64Codegen
        {
        public:
            // symbolName: Name des exportierten Funktionssymbols (z.B. "main", "_start", "foo")
            void generate(const LinearIR& ir,
                          const std::string& symbolName,
                          ObjectFile& out)
            {
                CodeBuffer textBuf;
                lowerLinearIRToX86_64(ir, textBuf);

                const auto& text = textBuf.data();

                std::vector<std::uint8_t> shstrtab;
                buildShStrTab(shstrtab);

                std::vector<std::uint8_t> strtab;
                std::vector<std::uint8_t> symtab;
                buildSymStrTabs(strtab, symtab, text.size(), symbolName);

                buildELF(out.data, text, shstrtab, strtab, symtab);
            }

        private:
            // ------------------------------------------------------------
            // ELF structures
            // ------------------------------------------------------------
            struct Elf64_Ehdr
            {
                unsigned char e_ident[16];
                std::uint16_t e_type;
                std::uint16_t e_machine;
                std::uint32_t e_version;
                std::uint64_t e_entry;
                std::uint64_t e_phoff;
                std::uint64_t e_shoff;
                std::uint32_t e_flags;
                std::uint16_t e_ehsize;
                std::uint16_t e_phentsize;
                std::uint16_t e_phnum;
                std::uint16_t e_shentsize;
                std::uint16_t e_shnum;
                std::uint16_t e_shstrndx;
            };

            struct Elf64_Shdr
            {
                std::uint32_t sh_name;
                std::uint32_t sh_type;
                std::uint64_t sh_flags;
                std::uint64_t sh_addr;
                std::uint64_t sh_offset;
                std::uint64_t sh_size;
                std::uint32_t sh_link;
                std::uint32_t sh_info;
                std::uint64_t sh_addralign;
                std::uint64_t sh_entsize;
            };

            struct Elf64_Sym
            {
                std::uint32_t st_name;
                std::uint8_t  st_info;
                std::uint8_t  st_other;
                std::uint16_t st_shndx;
                std::uint64_t st_value;
                std::uint64_t st_size;
            };

            enum : std::uint32_t
            {
                SHT_NULL    = 0,
                SHT_PROGBITS= 1,
                SHT_SYMTAB  = 2,
                SHT_STRTAB  = 3
            };

            // ------------------------------------------------------------
            // .shstrtab: Section-Namen
            // Indexe:
            //  0: ""
            //  1: ".text"
            //  7: ".symtab"
            // 15: ".strtab"
            // 23: ".shstrtab"
            // ------------------------------------------------------------
            void buildShStrTab(std::vector<std::uint8_t>& shstr)
            {
                const char* s =
                    "\0"
                    ".text\0"
                    ".symtab\0"
                    ".strtab\0"
                    ".shstrtab\0";

                shstr.assign(s, s + std::strlen(s) + 1);
            }

            // ------------------------------------------------------------
            // .strtab + .symtab
            // sym[0] = undef
            // sym[1] = global function in .text
            // ------------------------------------------------------------
            void buildSymStrTabs(std::vector<std::uint8_t>& strtab,
                                 std::vector<std::uint8_t>& symtab,
                                 std::size_t textSize,
                                 const std::string& symbolName)
            {
                strtab.clear();
                strtab.push_back(0x00); // index 0 = ""

                std::uint32_t nameOffset = static_cast<std::uint32_t>(strtab.size());
                strtab.insert(strtab.end(),
                              symbolName.begin(),
                              symbolName.end());
                strtab.push_back(0x00);

                symtab.resize(2 * sizeof(Elf64_Sym));
                std::memset(symtab.data(), 0, symtab.size());

                Elf64_Sym func{};
                func.st_name  = nameOffset;
                func.st_info  = (1u << 4) | 2u; // GLOBAL | FUNC
                func.st_other = 0;
                func.st_shndx = 1;              // .text
                func.st_value = 0;
                func.st_size  = textSize;

                std::memcpy(symtab.data() + sizeof(Elf64_Sym), &func, sizeof(func));
            }

            // ------------------------------------------------------------
            // Vollständiges ELF64-Objekt bauen
            // Sections:
            //  0: NULL
            //  1: .text
            //  2: .symtab
            //  3: .strtab
            //  4: .shstrtab
            // ------------------------------------------------------------
            void buildELF(std::vector<std::uint8_t>& out,
                          const std::vector<std::uint8_t>& text,
                          const std::vector<std::uint8_t>& shstr,
                          const std::vector<std::uint8_t>& strtab,
                          const std::vector<std::uint8_t>& symtab)
            {
                out.clear();

                const std::size_t ehdrSize = sizeof(Elf64_Ehdr);
                const std::size_t shdrSize = sizeof(Elf64_Shdr);
                const std::size_t shnum    = 5;

                std::size_t off = ehdrSize + shnum * shdrSize;

                std::size_t textOff   = off;
                std::size_t textSize  = text.size();
                off += textSize;

                std::size_t symOff    = off;
                std::size_t symSize   = symtab.size();
                off += symSize;

                std::size_t strOff    = off;
                std::size_t strSize   = strtab.size();
                off += strSize;

                std::size_t shstrOff  = off;
                std::size_t shstrSize = shstr.size();
                off += shstrSize;

                out.resize(off);
                std::memset(out.data(), 0, out.size());

                // -------------------------------
                // ELF-Header
                // -------------------------------
                Elf64_Ehdr eh{};
                eh.e_ident[0] = 0x7f;
                eh.e_ident[1] = 'E';
                eh.e_ident[2] = 'L';
                eh.e_ident[3] = 'F';
                eh.e_ident[4] = 2; // 64-bit
                eh.e_ident[5] = 1; // little endian
                eh.e_ident[6] = 1; // version
                eh.e_type      = 1;   // relocatable
                eh.e_machine   = 62;  // x86-64
                eh.e_version   = 1;
                eh.e_entry     = 0;
                eh.e_phoff     = 0;
                eh.e_shoff     = ehdrSize;
                eh.e_flags     = 0;
                eh.e_ehsize    = static_cast<std::uint16_t>(ehdrSize);
                eh.e_phentsize = 0;
                eh.e_phnum     = 0;
                eh.e_shentsize = static_cast<std::uint16_t>(shdrSize);
                eh.e_shnum     = static_cast<std::uint16_t>(shnum);
                eh.e_shstrndx  = 4;   // .shstrtab

                std::memcpy(out.data(), &eh, sizeof(eh));

                // -------------------------------
                // Section-Header
                // -------------------------------
                Elf64_Shdr sh_null{};
                Elf64_Shdr sh_text{};
                Elf64_Shdr sh_sym{};
                Elf64_Shdr sh_str{};
                Elf64_Shdr sh_shstr{};

                // .text
                sh_text.sh_name      = 1; // ".text"
                sh_text.sh_type      = SHT_PROGBITS;
                sh_text.sh_flags     = 0x6; // ALLOC | EXECINSTR
                sh_text.sh_addr      = 0;
                sh_text.sh_offset    = textOff;
                sh_text.sh_size      = textSize;
                sh_text.sh_link      = 0;
                sh_text.sh_info      = 0;
                sh_text.sh_addralign = 16;
                sh_text.sh_entsize   = 0;

                // .symtab
                sh_sym.sh_name       = 7; // ".symtab"
                sh_sym.sh_type       = SHT_SYMTAB;
                sh_sym.sh_flags      = 0;
                sh_sym.sh_addr       = 0;
                sh_sym.sh_offset     = symOff;
                sh_sym.sh_size       = symSize;
                sh_sym.sh_link       = 3; // link to .strtab
                sh_sym.sh_info       = 1; // first global symbol index
                sh_sym.sh_addralign  = 8;
                sh_sym.sh_entsize    = sizeof(Elf64_Sym);

                // .strtab
                sh_str.sh_name       = 15; // ".strtab"
                sh_str.sh_type       = SHT_STRTAB;
                sh_str.sh_flags      = 0;
                sh_str.sh_addr       = 0;
                sh_str.sh_offset     = strOff;
                sh_str.sh_size       = strSize;
                sh_str.sh_link       = 0;
                sh_str.sh_info       = 0;
                sh_str.sh_addralign  = 1;
                sh_str.sh_entsize    = 0;

                // .shstrtab
                sh_shstr.sh_name     = 23; // ".shstrtab"
                sh_shstr.sh_type     = SHT_STRTAB;
                sh_shstr.sh_flags    = 0;
                sh_shstr.sh_addr     = 0;
                sh_shstr.sh_offset   = shstrOff;
                sh_shstr.sh_size     = shstrSize;
                sh_shstr.sh_link     = 0;
                sh_shstr.sh_info     = 0;
                sh_shstr.sh_addralign= 1;
                sh_shstr.sh_entsize  = 0;

                std::uint8_t* pSh = out.data() + eh.e_shoff;
                std::memcpy(pSh + 0 * shdrSize, &sh_null,  sizeof(Elf64_Shdr));
                std::memcpy(pSh + 1 * shdrSize, &sh_text,  sizeof(Elf64_Shdr));
                std::memcpy(pSh + 2 * shdrSize, &sh_sym,   sizeof(Elf64_Shdr));
                std::memcpy(pSh + 3 * shdrSize, &sh_str,   sizeof(Elf64_Shdr));
                std::memcpy(pSh + 4 * shdrSize, &sh_shstr, sizeof(Elf64_Shdr));

                // -------------------------------
                // Section-Daten
                // -------------------------------
                std::memcpy(out.data() + textOff,  text.data(),   textSize);
                std::memcpy(out.data() + symOff,   symtab.data(), symSize);
                std::memcpy(out.data() + strOff,   strtab.data(), strSize);
                std::memcpy(out.data() + shstrOff, shstr.data(),  shstrSize);
            }
        };
    } // namespace codegen
} // namespace llgo
