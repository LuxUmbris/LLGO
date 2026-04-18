/* LLGO PE RISC-V 64 codegen */

#pragma once
#include "codegen_riscv64_def.hpp"
#include "codegen_x86_64_def.hpp" // for ObjectFile
#include <cstring>

namespace llgo
{
    namespace codegen
    {
        class PERISCV64Codegen
        {
        public:
            // symbolName: exported function symbol (e.g. "main", "_start")
            void generate(const LinearIR& ir,
                          const std::string& symbolName,
                          ObjectFile& out)
            {
                RISCVCodeBuffer codeBuf;
                lowerLinearIRToRISCV64(ir, codeBuf);

                const auto& text = codeBuf.data();

                std::vector<std::uint8_t> strtab;
                buildStringTable(strtab, symbolName);

                std::vector<CoffSymbol> symtab;
                buildSymbolTable(symtab, strtab);

                buildCOFF(out.data, text, symtab, strtab);
            }

        private:
            // -------------------------------------------------------
            // COFF structures
            // -------------------------------------------------------
            struct CoffFileHeader
            {
                std::uint16_t Machine;
                std::uint16_t NumberOfSections;
                std::uint32_t TimeDateStamp;
                std::uint32_t PointerToSymbolTable;
                std::uint32_t NumberOfSymbols;
                std::uint16_t SizeOfOptionalHeader;
                std::uint16_t Characteristics;
            };

            struct CoffSectionHeader
            {
                char          Name[8];
                std::uint32_t VirtualSize;
                std::uint32_t VirtualAddress;
                std::uint32_t SizeOfRawData;
                std::uint32_t PointerToRawData;
                std::uint32_t PointerToRelocations;
                std::uint32_t PointerToLinenumbers;
                std::uint16_t NumberOfRelocations;
                std::uint16_t NumberOfLinenumbers;
                std::uint32_t Characteristics;
            };

            struct CoffSymbol
            {
                union
                {
                    char ShortName[8];
                    struct
                    {
                        std::uint32_t Zeroes;
                        std::uint32_t Offset;
                    } LongName;
                } N;

                std::uint32_t Value;
                std::int16_t  SectionNumber;
                std::uint16_t Type;
                std::uint8_t  StorageClass;
                std::uint8_t  NumberOfAuxSymbols;
            };

            static constexpr std::uint16_t IMAGE_FILE_MACHINE_RISCV64 = 0x5064;

            enum : std::uint32_t
            {
                IMAGE_SCN_CNT_CODE               = 0x00000020,
                IMAGE_SCN_MEM_EXECUTE            = 0x20000000,
                IMAGE_SCN_MEM_READ               = 0x40000000,
                IMAGE_SCN_ALIGN_4BYTES           = 0x00300000
            };

            enum : std::uint8_t
            {
                IMAGE_SYM_CLASS_EXTERNAL = 2
            };

            // -------------------------------------------------------
            void buildStringTable(std::vector<std::uint8_t>& strtab,
                                  const std::string& symbolName)
            {
                strtab.clear();
                // 4-byte size prefix (filled in later)
                strtab.resize(4, 0);
                strtab.insert(strtab.end(), symbolName.begin(), symbolName.end());
                strtab.push_back(0);
                // Patch size
                std::uint32_t sz = static_cast<std::uint32_t>(strtab.size());
                std::memcpy(strtab.data(), &sz, 4);
            }

            void buildSymbolTable(std::vector<CoffSymbol>& symtab,
                                  const std::vector<std::uint8_t>& strtab)
            {
                symtab.clear();
                CoffSymbol sym{};
                std::memset(&sym, 0, sizeof(sym));

                // Symbol name > 8 chars → string table offset
                sym.N.LongName.Zeroes = 0;
                sym.N.LongName.Offset = 4; // after size prefix in strtab

                sym.Value         = 0;
                sym.SectionNumber = 1;
                sym.Type          = 0x20; // function
                sym.StorageClass  = IMAGE_SYM_CLASS_EXTERNAL;
                sym.NumberOfAuxSymbols = 0;
                symtab.push_back(sym);
            }

            void buildCOFF(std::vector<std::uint8_t>& out,
                           const std::vector<std::uint8_t>& text,
                           const std::vector<CoffSymbol>& symtab,
                           const std::vector<std::uint8_t>& strtab)
            {
                out.clear();

                const std::size_t fileHdrSize  = sizeof(CoffFileHeader);
                const std::size_t sectHdrSize  = sizeof(CoffSectionHeader);
                const std::size_t symEntrySize = sizeof(CoffSymbol);

                std::size_t textOff  = fileHdrSize + sectHdrSize;
                std::size_t symOff   = textOff + text.size();
                std::size_t strOff   = symOff + symtab.size() * symEntrySize;
                std::size_t totalSz  = strOff + strtab.size();

                out.resize(totalSz);
                std::memset(out.data(), 0, out.size());

                CoffFileHeader fh{};
                fh.Machine              = IMAGE_FILE_MACHINE_RISCV64;
                fh.NumberOfSections     = 1;
                fh.TimeDateStamp        = 0;
                fh.PointerToSymbolTable = static_cast<std::uint32_t>(symOff);
                fh.NumberOfSymbols      = static_cast<std::uint32_t>(symtab.size());
                fh.SizeOfOptionalHeader = 0;
                fh.Characteristics      = 0;
                std::memcpy(out.data(), &fh, sizeof(fh));

                CoffSectionHeader sh{};
                std::memcpy(sh.Name, ".text\0\0\0", 8);
                sh.VirtualSize          = static_cast<std::uint32_t>(text.size());
                sh.VirtualAddress       = 0;
                sh.SizeOfRawData        = static_cast<std::uint32_t>(text.size());
                sh.PointerToRawData     = static_cast<std::uint32_t>(textOff);
                sh.PointerToRelocations = 0;
                sh.PointerToLinenumbers = 0;
                sh.NumberOfRelocations  = 0;
                sh.NumberOfLinenumbers  = 0;
                sh.Characteristics      = IMAGE_SCN_CNT_CODE
                                        | IMAGE_SCN_MEM_EXECUTE
                                        | IMAGE_SCN_MEM_READ
                                        | IMAGE_SCN_ALIGN_4BYTES;
                std::memcpy(out.data() + fileHdrSize, &sh, sizeof(sh));

                std::memcpy(out.data() + textOff, text.data(), text.size());

                for (std::size_t i = 0; i < symtab.size(); ++i)
                {
                    std::memcpy(out.data() + symOff + i * symEntrySize,
                                &symtab[i], symEntrySize);
                }

                std::memcpy(out.data() + strOff, strtab.data(), strtab.size());
            }
        };

    } // namespace codegen
} // namespace llgo
