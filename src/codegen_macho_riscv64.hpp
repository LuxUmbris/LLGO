/* LLGO Mach-O RISC-V 64 codegen */

#pragma once
#include "codegen_riscv64_def.hpp"
#include "codegen_x86_64_def.hpp" // for ObjectFile
#include <cstring>

namespace llgo
{
    namespace codegen
    {
        class MachORISCV64Codegen
        {
        public:
            // symbolName: exported function symbol (e.g. "_main", "_foo")
            void generate(const LinearIR& ir,
                          const std::string& symbolName,
                          ObjectFile& out)
            {
                RISCVCodeBuffer codeBuf;
                lowerLinearIRToRISCV64(ir, codeBuf);

                const auto& text = codeBuf.data();

                std::vector<std::uint8_t> strtab;
                buildStringTable(strtab, symbolName);

                std::vector<nlist_64> symtab;
                buildSymbolTable(symtab, strtab);

                buildMachO(out.data, text, symtab, strtab);
            }

        private:
            // -------------------------------------------------------
            // Mach-O structures
            // -------------------------------------------------------
            struct mach_header_64
            {
                std::uint32_t magic;
                std::int32_t  cputype;
                std::int32_t  cpusubtype;
                std::uint32_t filetype;
                std::uint32_t ncmds;
                std::uint32_t sizeofcmds;
                std::uint32_t flags;
                std::uint32_t reserved;
            };

            struct segment_command_64
            {
                std::uint32_t cmd;
                std::uint32_t cmdsize;
                char          segname[16];
                std::uint64_t vmaddr;
                std::uint64_t vmsize;
                std::uint64_t fileoff;
                std::uint64_t filesize;
                std::int32_t  maxprot;
                std::int32_t  initprot;
                std::uint32_t nsects;
                std::uint32_t flags;
            };

            struct section_64
            {
                char          sectname[16];
                char          segname[16];
                std::uint64_t addr;
                std::uint64_t size;
                std::uint32_t offset;
                std::uint32_t align;
                std::uint32_t reloff;
                std::uint32_t nreloc;
                std::uint32_t flags;
                std::uint32_t reserved1;
                std::uint32_t reserved2;
                std::uint32_t reserved3;
            };

            struct symtab_command
            {
                std::uint32_t cmd;
                std::uint32_t cmdsize;
                std::uint32_t symoff;
                std::uint32_t nsyms;
                std::uint32_t stroff;
                std::uint32_t strsize;
            };

            struct nlist_64
            {
                std::uint32_t n_strx;
                std::uint8_t  n_type;
                std::uint8_t  n_sect;
                std::uint16_t n_desc;
                std::uint64_t n_value;
            };

            enum : std::uint32_t
            {
                MH_MAGIC_64  = 0xFEEDFACFu,
                MH_OBJECT    = 0x1u,
                LC_SEGMENT_64= 0x19u,
                LC_SYMTAB    = 0x2u
            };

            // CPU_TYPE_ARM64 = 0xC000000C, but RISC-V isn't an official Apple type.
            // We use the unofficial value for RISC-V 64 Mach-O objects.
            // CPU_TYPE_RISCV = 0x00000018 (provisional / cross-build usage)
            enum : std::int32_t
            {
                CPU_TYPE_RISCV64    = 0x18,
                CPU_SUBTYPE_RISCV64 = 0x0
            };

            enum : std::uint8_t
            {
                N_SECT  = 0x0Eu,
                N_EXT   = 0x01u
            };

            // -------------------------------------------------------
            void buildStringTable(std::vector<std::uint8_t>& strtab,
                                  const std::string& symbolName)
            {
                strtab.clear();
                strtab.push_back(0x20); // leading space (Mach-O convention)
                strtab.push_back(0x00);
                strtab.insert(strtab.end(), symbolName.begin(), symbolName.end());
                strtab.push_back(0x00);
            }

            void buildSymbolTable(std::vector<nlist_64>& symtab,
                                  const std::vector<std::uint8_t>&)
            {
                symtab.clear();
                nlist_64 sym{};
                sym.n_strx  = 2; // after " \0"
                sym.n_type  = N_SECT | N_EXT;
                sym.n_sect  = 1;
                sym.n_desc  = 0;
                sym.n_value = 0;
                symtab.push_back(sym);
            }

            void buildMachO(std::vector<std::uint8_t>& out,
                            const std::vector<std::uint8_t>& text,
                            const std::vector<nlist_64>& symtab,
                            const std::vector<std::uint8_t>& strtab)
            {
                out.clear();

                const std::size_t mhSize      = sizeof(mach_header_64);
                const std::size_t segCmdSize  = sizeof(segment_command_64)
                                              + sizeof(section_64);
                const std::size_t symCmdSize  = sizeof(symtab_command);
                const std::size_t sizeofcmds  = segCmdSize + symCmdSize;
                const std::size_t headerEnd   = mhSize + sizeofcmds;

                // Align text to 4 bytes
                std::size_t textOff   = (headerEnd + 3) & ~std::size_t(3);
                std::size_t textSize  = text.size();

                std::size_t symOff    = (textOff + textSize + 7) & ~std::size_t(7);
                std::size_t symSize   = symtab.size() * sizeof(nlist_64);

                std::size_t strOff    = symOff + symSize;
                std::size_t strSize   = strtab.size();

                std::size_t totalSize = strOff + strSize;

                out.resize(totalSize);
                std::memset(out.data(), 0, out.size());

                // Mach-O header
                mach_header_64 mh{};
                mh.magic      = MH_MAGIC_64;
                mh.cputype    = CPU_TYPE_RISCV64;
                mh.cpusubtype = CPU_SUBTYPE_RISCV64;
                mh.filetype   = MH_OBJECT;
                mh.ncmds      = 2;
                mh.sizeofcmds = static_cast<std::uint32_t>(sizeofcmds);
                mh.flags      = 0;
                mh.reserved   = 0;
                std::memcpy(out.data(), &mh, sizeof(mh));

                // LC_SEGMENT_64 + section
                segment_command_64 seg{};
                seg.cmd      = LC_SEGMENT_64;
                seg.cmdsize  = static_cast<std::uint32_t>(segCmdSize);
                // segname = "" (unnamed segment for relocatable objects)
                std::memset(seg.segname, 0, 16);
                seg.vmaddr   = 0;
                seg.vmsize   = textSize;
                seg.fileoff  = textOff;
                seg.filesize = textSize;
                seg.maxprot  = 7;
                seg.initprot = 5;
                seg.nsects   = 1;
                seg.flags    = 0;
                std::memcpy(out.data() + mhSize, &seg, sizeof(seg));

                section_64 sect{};
                std::memcpy(sect.sectname, "__text\0\0\0\0\0\0\0\0\0\0", 16);
                std::memcpy(sect.segname,  "__TEXT\0\0\0\0\0\0\0\0\0\0", 16);
                sect.addr     = 0;
                sect.size     = textSize;
                sect.offset   = static_cast<std::uint32_t>(textOff);
                sect.align    = 2; // 2^2 = 4 byte alignment
                sect.reloff   = 0;
                sect.nreloc   = 0;
                sect.flags    = 0x80000400u; // S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS
                std::memcpy(out.data() + mhSize + sizeof(seg), &sect, sizeof(sect));

                // LC_SYMTAB
                symtab_command sc{};
                sc.cmd     = LC_SYMTAB;
                sc.cmdsize = sizeof(symtab_command);
                sc.symoff  = static_cast<std::uint32_t>(symOff);
                sc.nsyms   = static_cast<std::uint32_t>(symtab.size());
                sc.stroff  = static_cast<std::uint32_t>(strOff);
                sc.strsize = static_cast<std::uint32_t>(strSize);
                std::memcpy(out.data() + mhSize + segCmdSize, &sc, sizeof(sc));

                // Data
                std::memcpy(out.data() + textOff, text.data(), textSize);
                for (std::size_t i = 0; i < symtab.size(); ++i)
                    std::memcpy(out.data() + symOff + i * sizeof(nlist_64),
                                &symtab[i], sizeof(nlist_64));
                std::memcpy(out.data() + strOff, strtab.data(), strSize);
            }
        };

    } // namespace codegen
} // namespace llgo
