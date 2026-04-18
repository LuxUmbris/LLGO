/* LLGO PE ARM64 codegen */

#pragma once
#include "codegen_arm64_def.hpp"
#include "codegen_x86_64_def.hpp" // for ObjectFile (shared)

#include <cstring>

namespace llgo
{
    namespace codegen
    {
        class PEARM64Codegen
        {
        public:
            // symbolName: exported function symbol (e.g. "main", "_foo")
            void generate(const LinearIR& ir,
                          const std::string& symbolName,
                          ObjectFile& out)
            {
                ARM64CodeBuffer codeBuf;
                lowerLinearIRToARM64(ir, codeBuf);

                const auto& text = codeBuf.data();

                std::vector<std::uint8_t> strtab;
                buildStringTable(strtab, symbolName);

                std::vector<CoffSymbol> symtab;
                buildSymbolTable(symtab, strtab);

                buildCOFF(out.data, text, symtab, strtab);
            }

        private:
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

            void buildStringTable(std::vector<std::uint8_t>& out,
                                  const std::string& symbolName)
            {
                out.clear();
                out.resize(4);

                auto appendStr = [&](const std::string& s)
                {
                    out.insert(out.end(), s.begin(), s.end());
                    out.push_back(0x00);
                };

                appendStr(symbolName);

                std::uint32_t total = static_cast<std::uint32_t>(out.size());
                std::memcpy(out.data(), &total, 4);
            }

            void buildSymbolTable(std::vector<CoffSymbol>& symtab,
                                  const std::vector<std::uint8_t>& /*strtab*/)
            {
                symtab.clear();

                CoffSymbol undef{};
                std::memset(&undef, 0, sizeof(undef));
                symtab.push_back(undef);

                CoffSymbol func{};
                func.N.LongName.Zeroes = 0;
                func.N.LongName.Offset = 4; // first string after size field
                func.Value             = 0; // offset in .text
                func.SectionNumber     = 1; // .text
                func.Type              = 0x20; // function
                func.StorageClass      = 2; // external
                func.NumberOfAuxSymbols= 0;

                symtab.push_back(func);
            }

            void buildCOFF(std::vector<std::uint8_t>& out,
                           const std::vector<std::uint8_t>& text,
                           const std::vector<CoffSymbol>& symtab,
                           const std::vector<std::uint8_t>& strtab)
            {
                out.clear();

                const std::size_t fileHeaderSize    = sizeof(CoffFileHeader);
                const std::size_t sectionHeaderSize = sizeof(CoffSectionHeader);
                const std::size_t symtabSize        = symtab.size() * sizeof(CoffSymbol);
                const std::size_t strtabSize        = strtab.size();

                const std::size_t textOffset   = fileHeaderSize + sectionHeaderSize;
                const std::size_t symtabOffset = textOffset + text.size();
                const std::size_t strtabOffset = symtabOffset + symtabSize;

                const std::size_t totalSize =
                    fileHeaderSize +
                    sectionHeaderSize +
                    text.size() +
                    symtabSize +
                    strtabSize;

                out.resize(totalSize);
                std::memset(out.data(), 0, out.size());

                CoffFileHeader fh{};
                fh.Machine              = 0xAA64; // ARM64
                fh.NumberOfSections     = 1;
                fh.TimeDateStamp        = 0;
                fh.PointerToSymbolTable = static_cast<std::uint32_t>(symtabOffset);
                fh.NumberOfSymbols      = static_cast<std::uint32_t>(symtab.size());
                fh.SizeOfOptionalHeader = 0;
                fh.Characteristics      = 0;

                std::memcpy(out.data(), &fh, sizeof(fh));

                CoffSectionHeader sh{};
                std::memset(sh.Name, 0, sizeof(sh.Name));
                std::memcpy(sh.Name, ".text", 5);

                sh.VirtualSize         = 0;
                sh.VirtualAddress      = 0;
                sh.SizeOfRawData       = static_cast<std::uint32_t>(text.size());
                sh.PointerToRawData    = static_cast<std::uint32_t>(textOffset);
                sh.PointerToRelocations= 0;
                sh.PointerToLinenumbers= 0;
                sh.NumberOfRelocations = 0;
                sh.NumberOfLinenumbers = 0;
                sh.Characteristics     = 0x60000020; // code | execute | read

                std::memcpy(out.data() + fileHeaderSize, &sh, sizeof(sh));

                std::memcpy(out.data() + textOffset, text.data(), text.size());

                std::memcpy(out.data() + symtabOffset,
                            symtab.data(),
                            symtabSize);

                std::memcpy(out.data() + strtabOffset,
                            strtab.data(),
                            strtabSize);
            }
        };
    } // namespace codegen
} // namespace llgo
