/* LLGO – Unity Build Entry Point */

#pragma once
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <exception>

#include "llgo_api.hpp"

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

// ============================================================================
// INTERNAL PIPELINE ONLY — NO PUBLIC API HERE
// ============================================================================

CompileResult compile(const frontend::Module& module,
                      const CompileOptions&   opts)
{
    CompileResult result;

    try
    {
        // Stage 1: Build SoN graph
        SONBuilder builder;
        builder.build(module);
        Graph graph = builder.getGraph();

        // Stage 2: Optimisation
        if (opts.opt != OptLevel::O0)
        {
            SONOptimizer optimizer;
            optimizer.run(graph);
        }

        // Stage 3: Arena
        Arena arena;
        (void)arena;

        // Stage 4: Lowering
        LinearIR linearIR;
        Lowering lowering;
        lowering.run(graph, linearIR);

        // Stage 5: Reallocator
        Reallocator realloc(arena);
        (void)realloc;

        // Stage 6: Codegen
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
                            "x86-64 Mach-O not yet implemented; use ELF or PE";
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
    if (!fp)
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

} // namespace llgo
