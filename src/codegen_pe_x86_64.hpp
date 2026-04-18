/* LLGO PE x86-64 codegen */

#pragma once
#include "codegen_x86_64_def.hpp"
#include <cstring>

namespace llgo
{
    namespace codegen
    {
        class PE64Codegen
        {
        public:
            // symbolName: name of the exportierted function symbol
            void generate(const LinearIR& ir,
                          const std::string& symbolName,
                          ObjectFile& out)
            {
                CodeBuffer textBuf;
                lowerLinearIRToX86_64(ir, textBuf);

                const auto& text = textBuf.data();

                std::vector<std::uint8_t> strtab;
                buildStringTable(strtab, symbolName);

                std::vector<CoffSymbol> symtab;
                buildSymbolTable(symtab, strtab);

                buildCOFF(out.data, text, symtab, strtab);
            }

        private:
            // ------------------------------------------------------------
            // COFF structures
            // ------------------------------------------------------------
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

            // ------------------------------------------------------------
            // Stringtable
            // Format:
            //   [4 bytes: total size] [strings...]
            //   Offsets beziehen sich auf den Beginn NACH den 4 Bytes.
            // ------------------------------------------------------------
            void buildStringTable(std::vector<std::uint8_t>& out,
                                  const std::string& symbolName)
            {
                out.clear();

                // Space for size field
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

            // ------------------------------------------------------------
            // Symboltable
            // sym[0] = undef
            // sym[1] = global function in .text
            // ------------------------------------------------------------
            void buildSymbolTable(std::vector<CoffSymbol>& symtab,
                                  const std::vector<std::uint8_t>& strtab)
            {
                symtab.clear();

                // undef
                CoffSymbol undef{};
                std::memset(&undef, 0, sizeof(undef));
                symtab.push_back(undef);

                // Funkction symbol
                CoffSymbol func{};
                func.N.LongName.Zeroes = 0;
                func.N.LongName.Offset = 4;
                func.Value             = 0; // Offset inside .text
                func.SectionNumber     = 1; // .text
                func.Type              = 0x20; // function
                func.StorageClass      = 2; // external
                func.NumberOfAuxSymbols= 0;

                symtab.push_back(func);

                (void)strtab;
            }

            // ------------------------------------------------------------
            // Build COFF object
            //
            // Layout:
            //   [CoffFileHeader]
            //   [CoffSectionHeader (.text)]
            //   [.text]
            //   [Symboltable]
            //   [Stringtable]
            // ------------------------------------------------------------
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

                // -------------------------------
                // File Header
                // -------------------------------
                CoffFileHeader fh{};
                fh.Machine              = 0x8664; // AMD64
                fh.NumberOfSections     = 1;
                fh.TimeDateStamp        = 0;
                fh.PointerToSymbolTable = static_cast<std::uint32_t>(symtabOffset);
                fh.NumberOfSymbols      = static_cast<std::uint32_t>(symtab.size());
                fh.SizeOfOptionalHeader = 0;
                fh.Characteristics      = 0;

                std::memcpy(out.data(), &fh, sizeof(fh));

                // -------------------------------
                // Section Header (.text)
                // -------------------------------
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
                sh.Characteristics     = 0x60000020; // CODE | EXECUTE | READ

                std::memcpy(out.data() + fileHeaderSize, &sh, sizeof(sh));

                // -------------------------------
                // .text
                // -------------------------------
                std::memcpy(out.data() + textOffset, text.data(), text.size());

                // -------------------------------
                // Symboltable
                // -------------------------------
                std::memcpy(out.data() + symtabOffset,
                            symtab.data(),
                            symtabSize);

                // -------------------------------
                // Stringtable
                // -------------------------------
                std::memcpy(out.data() + strtabOffset,
                            strtab.data(),
                            strtabSize);
            }
        };
    } // namespace codegen
} // namespace llgo
