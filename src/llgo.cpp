/* LLGO – Unity Build Entry Point */

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <exception>

// ---- Pipeline headers (order matters) ----------------------------------------
#include "frontend.hpp"
#include "son_builder.hpp"
#include "optimizer.hpp"
#include "alloc.hpp"
#include "realloc.hpp"
#include "lowering.hpp"

// ---- x86-64 backends ---------------------------------------------------------
#include "codegen_x86_64_def.hpp"
#include "codegen_elf_x86_64.hpp"
#include "codegen_pe_x86_64.hpp"

// ---- ARM64 backends ----------------------------------------------------------
#include "codegen_arm64_def.hpp"
#include "codegen_elf_arm64.hpp"
#include "codegen_pe_arm64.hpp"
#include "codegen_macho_arm64.hpp"

// ---- RISC-V 64 backends ------------------------------------------------------
#include "codegen_riscv64_def.hpp"
#include "codegen_elf_risc64.hpp"
#include "codegen_pe_riscv64.hpp"
#include "codegen_macho_riscv64.hpp"

// ---- RISC-V 32 backends ------------------------------------------------------
#include "codegen_riscv32_def.hpp"
#include "codegen_elf_riscv32.hpp"
#include "codegen_pe_riscv32.hpp"
#include "codegen_macho_riscv32.hpp"

namespace llgo
{
    enum class Arch
    {
        X86_64,
        ARM64,
        RISCV64,
        RISCV32
    };

    enum class Format
    {
        ELF,
        PE,
        MachO
    };

    enum class OptLevel
    {
        O0,
        O1,
        O2
    };

    struct CompileOptions
    {
        Arch        arch       = Arch::X86_64;
        Format      fmt        = Format::ELF;
        OptLevel    opt        = OptLevel::O1;
        std::string symbolName = "main";
    };

    struct CompileResult
    {
        bool                      ok        = false;
        std::string               error;
        std::vector<std::uint8_t> objectData;
    };

    CompileResult compile(const frontend::Module& module,
                          const CompileOptions&   opts)
    {
        CompileResult result;

        try
        {
            SONBuilder builder;
            builder.build(module);
            Graph graph = builder.getGraph();

            if (opts.opt != OptLevel::O0)
            {
                SONOptimizer optimizer;
                optimizer.run(graph);
            }

            Arena arena;
            (void)arena;

            LinearIR linearIR;
            Lowering lowering;
            lowering.run(graph, linearIR);

            Reallocator realloc(arena);
            (void)realloc;

            codegen::ObjectFile objFile;

            switch (opts.arch)
            {
                case Arch::X86_64:
                {
                    switch (opts.fmt)
                    {
                        case Format::ELF:
                        {
                            codegen::ELF64Codegen cg;
                            cg.generate(linearIR, opts.symbolName, objFile);
                            break;
                        }
                        case Format::PE:
                        {
                            codegen::PE64Codegen cg;
                            cg.generate(linearIR, opts.symbolName, objFile);
                            break;
                        }
                        case Format::MachO:
                        {
                            result.error =
                                "x86-64 Mach-O not yet implemented; "
                                "use ELF or PE for x86-64";
                            return result;
                        }
                    }
                    break;
                }

                case Arch::ARM64:
                {
                    switch (opts.fmt)
                    {
                        case Format::ELF:
                        {
                            codegen::ELFARM64Codegen cg;
                            cg.generate(linearIR, opts.symbolName, objFile);
                            break;
                        }
                        case Format::PE:
                        {
                            codegen::PEARM64Codegen cg;
                            cg.generate(linearIR, opts.symbolName, objFile);
                            break;
                        }
                        case Format::MachO:
                        {
                            codegen::MachOARM64Codegen cg;
                            cg.generate(linearIR, opts.symbolName, objFile);
                            break;
                        }
                    }
                    break;
                }

                case Arch::RISCV64:
                {
                    switch (opts.fmt)
                    {
                        case Format::ELF:
                        {
                            codegen::ELFRISCV64Codegen cg;
                            cg.generate(linearIR, opts.symbolName, objFile);
                            break;
                        }
                        case Format::PE:
                        {
                            codegen::PERISCV64Codegen cg;
                            cg.generate(linearIR, opts.symbolName, objFile);
                            break;
                        }
                        case Format::MachO:
                        {
                            codegen::MachORISCV64Codegen cg;
                            cg.generate(linearIR, opts.symbolName, objFile);
                            break;
                        }
                    }
                    break;
                }

                case Arch::RISCV32:
                {
                    switch (opts.fmt)
                    {
                        case Format::ELF:
                        {
                            codegen::ELFRISCV32Codegen cg;
                            cg.generate(linearIR, opts.symbolName, objFile);
                            break;
                        }
                        case Format::PE:
                        {
                            codegen::PERISCV32Codegen cg;
                            cg.generate(linearIR, opts.symbolName, objFile);
                            break;
                        }
                        case Format::MachO:
                        {
                            codegen::MachORISCV32Codegen cg;
                            cg.generate(linearIR, opts.symbolName, objFile);
                            break;
                        }
                    }
                    break;
                }
            }

            result.objectData = std::move(objFile.data);
            result.ok         = true;
        }
        catch (const std::exception& e)
        {
            result.ok    = false;
            result.error = e.what();
        }
        catch (...)
        {
            result.ok    = false;
            result.error = "unknown error during compilation";
        }

        return result;
    }

    bool compileToFile(const frontend::Module& module,
                       const std::string&      path,
                       const CompileOptions&   opts,
                       std::string&            errorOut)
    {
        CompileResult res = compile(module, opts);
        if (!res.ok)
        {
            errorOut = res.error;
            return false;
        }

        std::FILE* fp = std::fopen(path.c_str(), "wb");
        if (fp == nullptr)
        {
            errorOut = "cannot open output file: " + path;
            return false;
        }

        std::size_t written = std::fwrite(res.objectData.data(),
                                          1,
                                          res.objectData.size(),
                                          fp);
        std::fclose(fp);

        if (written != res.objectData.size())
        {
            errorOut = "short write to: " + path;
            return false;
        }

        return true;
    }

    bool archFromString(const std::string& s, Arch& out)
    {
        if (s == "x86_64"  || s == "x86-64")  { out = Arch::X86_64;  return true; }
        if (s == "arm64"   || s == "aarch64") { out = Arch::ARM64;   return true; }
        if (s == "riscv64" || s == "riscv-64"){ out = Arch::RISCV64; return true; }
        if (s == "riscv32" || s == "riscv-32"){ out = Arch::RISCV32; return true; }
        return false;
    }

    bool formatFromString(const std::string& s, Format& out)
    {
        if (s == "elf")                 { out = Format::ELF;   return true; }
        if (s == "pe" || s == "coff")   { out = Format::PE;    return true; }
        if (s == "macho" || s == "mach-o") { out = Format::MachO; return true; }
        return false;
    }

    inline bool optLevelFromInt(int n, OptLevel& out)
    {
        switch (n)
        {
            case 0: out = OptLevel::O0; return true;
            case 1: out = OptLevel::O1; return true;
            case 2: out = OptLevel::O2; return true;
        }
        return false;
    }

    const char* version()
    {
        return "LLGO 1.0.0";
    }

    [[maybe_unused]] void prevent_dce_full_pipeline()
    {
        static volatile int force_keep = 0;
        if (force_keep == 0) return;

        Arena arena;
        Reallocator realloc(arena);
        SONBuilder builder;
        SONOptimizer opt;
        Lowering lowering;
        LinearIR lir;

        codegen::ELF64Codegen      elf_x64;
        codegen::ELFARM64Codegen   elf_arm64;
        codegen::ELFRISCV64Codegen elf_rv64;
        codegen::ELFRISCV32Codegen elf_rv32;

        codegen::PE64Codegen      pe_x64;
        codegen::PEARM64Codegen   pe_arm64;
        codegen::PERISCV64Codegen pe_rv64;
        codegen::PERISCV32Codegen pe_rv32;

        codegen::MachOARM64Codegen  macho_arm64;
        codegen::MachORISCV64Codegen macho_rv64;
        codegen::MachORISCV32Codegen macho_rv32;

        if (force_keep < 0)
        {
            codegen::ObjectFile obj;
            elf_x64.generate({}, "", obj);
            pe_x64.generate({}, "", obj);
            macho_arm64.generate({}, "", obj);
        }
    }

} // namespace llgo
