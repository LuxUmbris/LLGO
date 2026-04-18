/* LLGO – Unity Build Entry Point */

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
#include "codegen_elf_riscv64.hpp"
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
// INTERNAL PIPELINE IMPLEMENTATION
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

        // Stage 4: Lowering
        LinearIR linearIR;
        Lowering lowering;
        lowering.run(graph, linearIR);

        // Stage 5: Reallocator
        Reallocator realloc(arena);

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
                        result.error = "x86-64 Mach-O not implemented";
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
        result.ok = true;
    }
    catch (const std::exception& e)
    {
        result.ok = false;
        result.error = e.what();
    }
    catch (...)
    {
        result.ok = false;
        result.error = "unknown error";
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

    std::size_t written =
        std::fwrite(res.objectData.data(), 1, res.objectData.size(), fp);

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
    if (s == "x86_64" || s == "x86-64") { out = Arch::X86_64; return true; }
    if (s == "arm64"  || s == "aarch64") { out = Arch::ARM64; return true; }
    if (s == "riscv64") { out = Arch::RISCV64; return true; }
    if (s == "riscv32") { out = Arch::RISCV32; return true; }
    return false;
}

bool formatFromString(const std::string& s, Format& out)
{
    if (s == "elf")   { out = Format::ELF; return true; }
    if (s == "pe")    { out = Format::PE; return true; }
    if (s == "macho") { out = Format::MachO; return true; }
    return false;
}

bool optLevelFromInt(int n, OptLevel& out)
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

} // namespace llgo

// ============================================================================
// C API IMPLEMENTATION
// ============================================================================

extern "C" {

LLGOResult llgo_compile_c(const void* modulePtr,
                          const LLGOCompileOptions* opts)
{
    LLGOResult out{};
    out.ok = 0;
    out.error = nullptr;
    out.data = nullptr;
    out.size = 0;

    if (!modulePtr || !opts) {
        out.error = strdup("invalid arguments");
        return out;
    }

    const llgo::frontend::Module* mod =
        reinterpret_cast<const llgo::frontend::Module*>(modulePtr);

    llgo::CompileOptions cppOpts;
    cppOpts.symbolName = opts->symbolName ? opts->symbolName : "main";

    cppOpts.arch =
        (opts->arch == LLGO_ARCH_X86_64) ? llgo::Arch::X86_64 :
        (opts->arch == LLGO_ARCH_ARM64)  ? llgo::Arch::ARM64 :
        (opts->arch == LLGO_ARCH_RISCV64)? llgo::Arch::RISCV64 :
                                           llgo::Arch::RISCV32;

    cppOpts.fmt =
        (opts->format == LLGO_FMT_ELF)   ? llgo::Format::ELF :
        (opts->format == LLGO_FMT_PE)    ? llgo::Format::PE :
                                           llgo::Format::MachO;

    cppOpts.opt =
        (opts->optLevel == LLGO_OPT_O0) ? llgo::OptLevel::O0 :
        (opts->optLevel == LLGO_OPT_O1) ? llgo::OptLevel::O1 :
                                          llgo::OptLevel::O2;

    llgo::CompileResult res = llgo::compile(*mod, cppOpts);

    if (!res.ok) {
        out.error = strdup(res.error.c_str());
        return out;
    }

    out.ok = 1;
    out.size = res.objectData.size();
    out.data = (uint8_t*)malloc(out.size);
    memcpy(out.data, res.objectData.data(), out.size);

    return out;
}

int llgo_compile_to_file_c(const void* modulePtr,
                           const char* path,
                           const LLGOCompileOptions* opts,
                           const char** errorOut)
{
    if (errorOut) *errorOut = nullptr;

    if (!modulePtr || !path || !opts) {
        if (errorOut) *errorOut = "invalid arguments";
        return 0;
    }

    const llgo::frontend::Module* mod =
        reinterpret_cast<const llgo::frontend::Module*>(modulePtr);

    llgo::CompileOptions cppOpts;
    cppOpts.symbolName = opts->symbolName ? opts->symbolName : "main";

    cppOpts.arch =
        (opts->arch == LLGO_ARCH_X86_64) ? llgo::Arch::X86_64 :
        (opts->arch == LLGO_ARCH_ARM64)  ? llgo::Arch::ARM64 :
        (opts->arch == LLGO_ARCH_RISCV64)? llgo::Arch::RISCV64 :
                                           llgo::Arch::RISCV32;

    cppOpts.fmt =
        (opts->format == LLGO_FMT_ELF)   ? llgo::Format::ELF :
        (opts->format == LLGO_FMT_PE)    ? llgo::Format::PE :
                                           llgo::Format::MachO;

    cppOpts.opt =
        (opts->optLevel == LLGO_OPT_O0) ? llgo::OptLevel::O0 :
        (opts->optLevel == LLGO_OPT_O1) ? llgo::OptLevel::O1 :
                                          llgo::OptLevel::O2;

    std::string err;
    bool ok = llgo::compileToFile(*mod, path, cppOpts, err);

    if (!ok) {
        if (errorOut) *errorOut = strdup(err.c_str());
        return 0;
    }

    return 1;
}

void llgo_result_free_c(LLGOResult* r)
{
    if (!r) return;
    if (r->error) free(r->error);
    if (r->data)  free(r->data);
    r->error = nullptr;
    r->data  = nullptr;
    r->size  = 0;
    r->ok    = 0;
}

int llgo_arch_from_string_c(const char* s, LLGOArch* out)
{
    if (!s || !out) return 0;
    std::string str(s);
    llgo::Arch a;
    if (!llgo::archFromString(str, a)) return 0;

    *out =
        (a == llgo::Arch::X86_64) ? LLGO_ARCH_X86_64 :
        (a == llgo::Arch::ARM64)  ? LLGO_ARCH_ARM64 :
        (a == llgo::Arch::RISCV64)? LLGO_ARCH_RISCV64 :
                                    LLGO_ARCH_RISCV32;

    return 1;
}

int llgo_format_from_string_c(const char* s, LLGOFormat* out)
{
    if (!s || !out) return 0;
    std::string str(s);
    llgo::Format f;
    if (!llgo::formatFromString(str, f)) return 0;

    *out =
        (f == llgo::Format::ELF)   ? LLGO_FMT_ELF :
        (f == llgo::Format::PE)    ? LLGO_FMT_PE :
                                     LLGO_FMT_MACHO;

    return 1;
}

int llgo_opt_level_from_int_c(int n, LLGOOptLevel* out)
{
    if (!out) return 0;
    llgo::OptLevel o;
    if (!llgo::optLevelFromInt(n, o)) return 0;

    *out =
        (o == llgo::OptLevel::O0) ? LLGO_OPT_O0 :
        (o == llgo::OptLevel::O1) ? LLGO_OPT_O1 :
                                    LLGO_OPT_O2;

    return 1;
}

const char* llgo_version_c(void)
{
    return llgo::version();
}

} // extern "C"
