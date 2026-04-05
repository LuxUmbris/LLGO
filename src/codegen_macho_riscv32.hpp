/* LLGO Mach-O RISC-V 32 codegen */

#pragma once
#include "codegen_riscv32_def.hpp"
#include "codegen_x86_64_def.hpp" // for ObjectFile
#include <cstring>

namespace llgo
{
    namespace codegen
    {
        class MachORISCV32Codegen
        {
        public:
            // symbolName: exported function symbol (e.g. "_main", "_foo")
            void generate(const LinearIR& ir,
                          const std::string& symbolName,
                          ObjectFile& out)
            {
                RISCV32CodeBuffer codeBuf;
                lowerLinearIRToRISCV32(ir, codeBuf);

                const auto& text = codeBuf.data();

                std::vector<std::uint8_t> strtab;
                buildStringTable(strtab, symbolName);

                std::vector<nlist_32> symtab;
                buildSymbolTable(symtab, strtab);

                buildMachO(out.data, text, symtab, strtab);
            }

        private:
            // -------------------------------------------------------
            // 32-bit Mach-O structures
            // -------------------------------------------------------
            struct mach_header_32
            {
                std::uint32_t magic;
                std::int32_t  cputype;
                std::int32_t  cpusubtype;
                std::uint32_t filetype;
                std::uint32_t ncmds;
                std::uint32_t sizeofcmds;
                std::uint32_t flags;
            };

            struct segment_command_32
            {
                std::uint32_t cmd;
                std::uint32_t cmdsize;
                char          segname[16];
                std::uint32_t vmaddr;
                std::uint32_t vmsize;
                std::uint32_t fileoff;
                std::uint32_t filesize;
                std::int32_t  maxprot;
                std::int32_t  initprot;
                std::uint32_t nsects;
                std::uint32_t flags;
            };

            struct section_32
            {
                char          sectname[16];
                char          segname[16];
                std::uint32_t addr;
                std::uint32_t size;
                std::uint32_t offset;
                std::uint32_t align;
                std::uint32_t reloff;
                std::uint32_t nreloc;
                std::uint32_t flags;
                std::uint32_t reserved1;
                std::uint32_t reserved2;
            };

            struct symtab_command_32
            {
                std::uint32_t cmd;
                std::uint32_t cmdsize;
                std::uint32_t symoff;
                std::uint32_t nsyms;
                std::uint32_t stroff;
                std::uint32_t strsize;
            };

            struct nlist_32
            {
                std::uint32_t n_strx;
                std::uint8_t  n_type;
                std::uint8_t  n_sect;
                std::int16_t  n_desc;
                std::uint32_t n_value;
            };

            enum : std::uint32_t
            {
                MH_MAGIC_32   = 0xFEEDFACEu,
                MH_OBJECT_32  = 0x1u,
                LC_SEGMENT_32 = 0x1u,
                LC_SYMTAB_32  = 0x2u
            };

            // Unofficial CPU type for RISC-V 32 (cross-compilation / embedded)
            enum : std::int32_t
            {
                CPU_TYPE_RISCV32    = 0x17,
                CPU_SUBTYPE_RISCV32 = 0x0
            };

            enum : std::uint8_t
            {
                N_SECT_32 = 0x0Eu,
                N_EXT_32  = 0x01u
            };

            // -------------------------------------------------------
            void buildStringTable(std::vector<std::uint8_t>& strtab,
                                  const std::string& symbolName)
            {
                strtab.clear();
                strtab.push_back(0x20);
                strtab.push_back(0x00);
                strtab.insert(strtab.end(), symbolName.begin(), symbolName.end());
                strtab.push_back(0x00);
            }

            void buildSymbolTable(std::vector<nlist_32>& symtab,
                                  const std::vector<std::uint8_t>&)
            {
                symtab.clear();
                nlist_32 sym{};
                sym.n_strx  = 2;
                sym.n_type  = N_SECT_32 | N_EXT_32;
                sym.n_sect  = 1;
                sym.n_desc  = 0;
                sym.n_value = 0;
                symtab.push_back(sym);
            }

            void buildMachO(std::vector<std::uint8_t>& out,
                            const std::vector<std::uint8_t>& text,
                            const std::vector<nlist_32>& symtab,
                            const std::vector<std::uint8_t>& strtab)
            {
                out.clear();

                const std::size_t mhSize      = sizeof(mach_header_32);
                const std::size_t segCmdSize  = sizeof(segment_command_32)
                                              + sizeof(section_32);
                const std::size_t symCmdSize  = sizeof(symtab_command_32);
                const std::size_t sizeofcmds  = segCmdSize + symCmdSize;
                const std::size_t headerEnd   = mhSize + sizeofcmds;

                std::size_t textOff  = (headerEnd + 3) & ~std::size_t(3);
                std::size_t textSize = text.size();
                std::size_t symOff   = (textOff + textSize + 3) & ~std::size_t(3);
                std::size_t symSize  = symtab.size() * sizeof(nlist_32);
                std::size_t strOff   = symOff + symSize;
                std::size_t strSize  = strtab.size();
                std::size_t total    = strOff + strSize;

                out.resize(total);
                std::memset(out.data(), 0, out.size());

                mach_header_32 mh{};
                mh.magic      = MH_MAGIC_32;
                mh.cputype    = CPU_TYPE_RISCV32;
                mh.cpusubtype = CPU_SUBTYPE_RISCV32;
                mh.filetype   = MH_OBJECT_32;
                mh.ncmds      = 2;
                mh.sizeofcmds = static_cast<std::uint32_t>(sizeofcmds);
                mh.flags      = 0;
                std::memcpy(out.data(), &mh, sizeof(mh));

                segment_command_32 seg{};
                seg.cmd      = LC_SEGMENT_32;
                seg.cmdsize  = static_cast<std::uint32_t>(segCmdSize);
                std::memset(seg.segname, 0, 16);
                seg.vmaddr   = 0;
                seg.vmsize   = static_cast<std::uint32_t>(textSize);
                seg.fileoff  = static_cast<std::uint32_t>(textOff);
                seg.filesize = static_cast<std::uint32_t>(textSize);
                seg.maxprot  = 7;
                seg.initprot = 5;
                seg.nsects   = 1;
                seg.flags    = 0;
                std::memcpy(out.data() + mhSize, &seg, sizeof(seg));

                section_32 sect{};
                std::memcpy(sect.sectname, "__text\0\0\0\0\0\0\0\0\0\0", 16);
                std::memcpy(sect.segname,  "__TEXT\0\0\0\0\0\0\0\0\0\0", 16);
                sect.addr    = 0;
                sect.size    = static_cast<std::uint32_t>(textSize);
                sect.offset  = static_cast<std::uint32_t>(textOff);
                sect.align   = 2;
                sect.flags   = 0x80000400u;
                std::memcpy(out.data() + mhSize + sizeof(seg), &sect, sizeof(sect));

                symtab_command_32 sc{};
                sc.cmd     = LC_SYMTAB_32;
                sc.cmdsize = sizeof(symtab_command_32);
                sc.symoff  = static_cast<std::uint32_t>(symOff);
                sc.nsyms   = static_cast<std::uint32_t>(symtab.size());
                sc.stroff  = static_cast<std::uint32_t>(strOff);
                sc.strsize = static_cast<std::uint32_t>(strSize);
                std::memcpy(out.data() + mhSize + segCmdSize, &sc, sizeof(sc));

                std::memcpy(out.data() + textOff, text.data(), textSize);
                for (std::size_t i = 0; i < symtab.size(); ++i)
                    std::memcpy(out.data() + symOff + i * sizeof(nlist_32),
                                &symtab[i], sizeof(nlist_32));
                std::memcpy(out.data() + strOff, strtab.data(), strSize);
            }
        };

    } // namespace codegen
} // namespace llgo
