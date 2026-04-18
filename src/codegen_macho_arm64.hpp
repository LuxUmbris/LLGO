/* LLGO Mach-o ARM64 codegen */

#pragma once
#include "codegen_arm64_def.hpp"
#include "codegen_x86_64_def.hpp" // for ObjectFile (shared)
#include <cstring>

namespace llgo
{
    namespace codegen
    {
        class MachOARM64Codegen
        {
        public:
            // symbolName: exported function symbol (e.g. "_main", "_foo")
            void generate(const LinearIR& ir,
                          const std::string& symbolName,
                          ObjectFile& out)
            {
                ARM64CodeBuffer codeBuf;
                lowerLinearIRToARM64(ir, codeBuf);

                const auto& text = codeBuf.data();

                std::vector<std::uint8_t> strtab;
                buildStringTable(strtab, symbolName);

                std::vector<nlist_64> symtab;
                buildSymbolTable(symtab, strtab);

                buildMachO(out.data, text, symtab, strtab);
            }

        private:
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

            struct nlist_64
            {
                std::uint32_t n_strx;
                std::uint8_t  n_type;
                std::uint8_t  n_sect;
                std::int16_t  n_desc;
                std::uint64_t n_value;
            };

            void buildStringTable(std::vector<std::uint8_t>& out,
                                  const std::string& symbolName)
            {
                out.clear();
                out.push_back(0x00);

                auto appendStr = [&](const std::string& s) -> std::uint32_t
                {
                    std::uint32_t off = static_cast<std::uint32_t>(out.size());
                    out.insert(out.end(), s.begin(), s.end());
                    out.push_back(0x00);
                    return off;
                };

                appendStr(symbolName);
            }

            void buildSymbolTable(std::vector<nlist_64>& symtab,
                                  const std::vector<std::uint8_t>& /*strtab*/)
            {
                symtab.clear();

                nlist_64 undef{};
                undef.n_strx = 0;
                undef.n_type = 0x00;
                undef.n_sect = 0;
                undef.n_desc = 0;
                undef.n_value= 0;
                symtab.push_back(undef);

                nlist_64 func{};
                func.n_strx = 1;
                func.n_type = 0x0F;
                func.n_sect = 1;
                func.n_desc = 0;
                func.n_value= 0;
                symtab.push_back(func);
            }

            void buildMachO(std::vector<std::uint8_t>& out,
                            const std::vector<std::uint8_t>& text,
                            const std::vector<nlist_64>& symtab,
                            const std::vector<std::uint8_t>& strtab)
            {
                out.clear();

                const std::uint32_t MH_MAGIC_64   = 0xFEEDFACF;
                const std::int32_t  CPU_TYPE_ARM64   = 0x0100000C;
                const std::int32_t  CPU_SUBTYPE_ARM64_ALL = 0x00000000;
                const std::uint32_t MH_OBJECT     = 0x1;
                const std::uint32_t MH_SUBSECTIONS_VIA_SYMBOLS = 0x2000;

                const std::uint32_t LC_SEGMENT_64 = 0x19;
                const std::uint32_t LC_SYMTAB     = 0x2;

                const std::size_t headerSize  = sizeof(mach_header_64);
                const std::size_t segcmdSize  = sizeof(segment_command_64);
                const std::size_t sectSize    = sizeof(section_64);
                const std::size_t symtabCmdSize = 24;

                const std::size_t ncmds      = 2;
                const std::size_t sizeofcmds = segcmdSize + sectSize + symtabCmdSize;

                const std::size_t textOffset = headerSize + sizeofcmds;
                const std::size_t textSize   = text.size();

                const std::size_t symoff     = textOffset + textSize;
                const std::size_t nsyms      = symtab.size();
                const std::size_t symSize    = nsyms * sizeof(nlist_64);

                const std::size_t stroff     = symoff + symSize;
                const std::size_t strSize    = strtab.size();

                const std::size_t totalSize  = stroff + strSize;

                out.resize(totalSize);
                std::memset(out.data(), 0, out.size());

                mach_header_64 mh{};
                mh.magic      = MH_MAGIC_64;
                mh.cputype    = CPU_TYPE_ARM64;
                mh.cpusubtype = CPU_SUBTYPE_ARM64_ALL;
                mh.filetype   = MH_OBJECT;
                mh.ncmds      = static_cast<std::uint32_t>(ncmds);
                mh.sizeofcmds = static_cast<std::uint32_t>(sizeofcmds);
                mh.flags      = MH_SUBSECTIONS_VIA_SYMBOLS;
                mh.reserved   = 0;

                std::memcpy(out.data(), &mh, sizeof(mh));

                segment_command_64 seg{};
                seg.cmd      = LC_SEGMENT_64;
                seg.cmdsize  = static_cast<std::uint32_t>(segcmdSize + sectSize);
                std::memset(seg.segname, 0, sizeof(seg.segname));
                std::memcpy(seg.segname, "__TEXT", 6);
                seg.vmaddr   = 0;
                seg.vmsize   = textSize;
                seg.fileoff  = textOffset;
                seg.filesize = textSize;
                seg.maxprot  = 0x7;
                seg.initprot = 0x5;
                seg.nsects   = 1;
                seg.flags    = 0;

                std::uint8_t* p = out.data() + headerSize;
                std::memcpy(p, &seg, sizeof(seg));
                p += sizeof(seg);

                section_64 sect{};
                std::memset(sect.sectname, 0, sizeof(sect.sectname));
                std::memcpy(sect.sectname, "__text", 6);
                std::memset(sect.segname, 0, sizeof(sect.segname));
                std::memcpy(sect.segname, "__TEXT", 6);
                sect.addr    = 0;
                sect.size    = textSize;
                sect.offset  = static_cast<std::uint32_t>(textOffset);
                sect.align   = 2;
                sect.reloff  = 0;
                sect.nreloc  = 0;
                sect.flags   = 0x80000400;
                sect.reserved1 = 0;
                sect.reserved2 = 0;
                sect.reserved3 = 0;

                std::memcpy(p, &sect, sizeof(sect));
                p += sizeof(sect);

                std::uint32_t* p32 = reinterpret_cast<std::uint32_t*>(p);
                p32[0] = LC_SYMTAB;
                p32[1] = static_cast<std::uint32_t>(symtabCmdSize);
                p32[2] = static_cast<std::uint32_t>(symoff);
                p32[3] = static_cast<std::uint32_t>(nsyms);
                p32[4] = static_cast<std::uint32_t>(stroff);
                p32[5] = static_cast<std::uint32_t>(strSize);

                std::memcpy(out.data() + textOffset, text.data(), textSize);

                std::memcpy(out.data() + symoff,
                            symtab.data(),
                            symSize);

                std::memcpy(out.data() + stroff,
                            strtab.data(),
                            strSize);
            }
        };
    } // namespace codegen
} // namespace llgo
