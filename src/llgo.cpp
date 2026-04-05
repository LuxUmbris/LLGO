/* LLGO – Unity Build Entry Point & Public API */

// =============================================================================
// Unity Build: all implementation headers are included here.
// Every other .hpp is a self-contained header-only unit; this .cpp
// is the sole translation unit that users compile.
//
// Build example (C++17):
//   g++ -std=c++17 -O2 -Isrc -c src/llgo.cpp -o llgo.o
//   ar rcs libllgo.a llgo.o
// =============================================================================

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

// =============================================================================
// Public C++ API
// =============================================================================

namespace llgo
{
    // -------------------------------------------------------------------------
    // Target architecture
    // -------------------------------------------------------------------------
    enum class Arch
    {
        X86_64,
        ARM64,
        RISCV64,
        RISCV32
    };

    // -------------------------------------------------------------------------
    // Target object-file format
    // -------------------------------------------------------------------------
    enum class Format
    {
        ELF,
        PE,
        MachO
    };

    // -------------------------------------------------------------------------
    // Optimisation level (mirrors common -O flags)
    // -------------------------------------------------------------------------
    enum class OptLevel
    {
        O0, // no optimisation
        O1, // DCE + CSE (default)
        O2  // O1 + constant folding + strength reduction
    };

    // -------------------------------------------------------------------------
    // Compilation options
    // -------------------------------------------------------------------------
    struct CompileOptions
    {
        Arch      arch       = Arch::X86_64;
        Format    fmt        = Format::ELF;
        OptLevel  opt        = OptLevel::O1;
        std::string symbolName = "main"; // exported function name
    };

    // -------------------------------------------------------------------------
    // Compile result
    // -------------------------------------------------------------------------
    struct CompileResult
    {
        bool                      ok      = false;
        std::string               error;
        std::vector<std::uint8_t> objectData; // raw bytes of the object file
    };

    // =========================================================================
    // llgo::compile()
    //
    // Main entry point.  Translates a frontend::Module through the full
    // pipeline (SoN build → optimise → lower → alloc → codegen) and returns
    // a ready-to-link object file in memory.
    //
    // Usage:
    //   llgo::frontend::Module mod = /* build your AST/IR here */;
    //   llgo::CompileOptions   opts;
    //   opts.arch       = llgo::Arch::RISCV64;
    //   opts.fmt        = llgo::Format::ELF;
    //   opts.opt        = llgo::OptLevel::O2;
    //   opts.symbolName = "my_func";
    //   llgo::CompileResult res = llgo::compile(mod, opts);
    //   if (!res.ok) { /* handle res.error */ }
    //   // write res.objectData to a .o file or pass to a linker
    // =========================================================================
    inline CompileResult compile(const frontend::Module& module,
                                 const CompileOptions& opts = {})
    {
        CompileResult result;

        try
        {
            // ------------------------------------------------------------------
            // Stage 1: Build Sea-of-Nodes graph from frontend IR
            // ------------------------------------------------------------------
            SONBuilder builder;
            builder.build(module);
            Graph graph = builder.getGraph(); // value-copy so we can mutate

            // ------------------------------------------------------------------
            // Stage 2: Optimise
            // ------------------------------------------------------------------
            if (opts.opt != OptLevel::O0)
            {
                SONOptimizer optimizer;
                optimizer.run(graph);
            }

            // ------------------------------------------------------------------
            // Stage 3: Arena allocation (all subsequent work uses the arena)
            // ------------------------------------------------------------------
            Arena arena;
            (void)arena; // used implicitly by future arena-aware passes

            // ------------------------------------------------------------------
            // Stage 4: Lower SoN graph → LinearIR
            // ------------------------------------------------------------------
            LinearIR linearIR;
            Lowering lowering;
            lowering.run(graph, linearIR);

            // ------------------------------------------------------------------
            // Stage 5: Re-allocate / compact arena after lowering
            // ------------------------------------------------------------------
            Reallocator realloc(arena);
            (void)realloc;

            // ------------------------------------------------------------------
            // Stage 6: Code generation → ObjectFile
            // ------------------------------------------------------------------
            codegen::ObjectFile objFile;

            switch (opts.arch)
            {
                // --- x86-64 ---
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
                            // x86-64 Mach-O: reuse the ELF codegen path with
                            // a Mach-O wrapper (not yet a separate file, so we
                            // emit a PE stub here and flag it)
                            result.error =
                                "x86-64 Mach-O not yet implemented; "
                                "use ELF or PE for x86-64";
                            return result;
                        }
                    }
                    break;
                }

                // --- ARM64 ---
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

                // --- RISC-V 64 ---
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

                // --- RISC-V 32 ---
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

    // =========================================================================
    // llgo::compileToFile()
    //
    // Convenience wrapper: compile and write the object file to disk.
    // Returns true on success, false + error message on failure.
    // =========================================================================
    inline bool compileToFile(const frontend::Module& module,
                              const std::string& path,
                              const CompileOptions& opts,
                              std::string& errorOut)
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

    // =========================================================================
    // llgo::archFromString() / llgo::formatFromString()
    //
    // Parse CLI-style strings into enum values.
    // Returns false if the string is unrecognised.
    // =========================================================================
    inline bool archFromString(const std::string& s, Arch& out)
    {
        if (s == "x86_64"  || s == "x86-64")   { out = Arch::X86_64;  return true; }
        if (s == "arm64"   || s == "aarch64")   { out = Arch::ARM64;   return true; }
        if (s == "riscv64" || s == "riscv-64")  { out = Arch::RISCV64; return true; }
        if (s == "riscv32" || s == "riscv-32")  { out = Arch::RISCV32; return true; }
        return false;
    }

    inline bool formatFromString(const std::string& s, Format& out)
    {
        if (s == "elf")   { out = Format::ELF;   return true; }
        if (s == "pe"   || s == "coff") { out = Format::PE;    return true; }
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

    // =========================================================================
    // llgo::version()
    //
    // Returns a human-readable version string.
    // =========================================================================
    inline const char* version()
    {
        return "LLGO 1.0.0";
    }

} // namespace llgo
