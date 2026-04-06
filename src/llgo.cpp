/* LLGO – Unity Build Entry Point */

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <exception>

// Public API header (C + C++ API declarations)
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
    // -------------------------------------------------------------------------
    // Internal C++ compile implementation (used by C API wrapper)
    // -------------------------------------------------------------------------
    CompileResult compile_internal(const frontend::Module& module,
                                   Arch                    arch,
                                   Format                  fmt,
                                   OptLevel                opt,
                                   const std::string&      symbolName)
    {
        CompileResult result;

        try
        {
            // Build SoN graph
            SONBuilder builder;
            builder.build(module);
            Graph graph = builder.getGraph();

            // Optimizer
            if (opt != OptLevel::O0)
            {
                SONOptimizer optimizer;
                optimizer.run(graph);
            }

            // Arena
            Arena arena;

            // Lowering
            LinearIR linearIR;
            Lowering lowering;
            lowering.run(graph, linearIR);

            // Reallocator
            Reallocator realloc(arena);

            // Codegen
            codegen::ObjectFile objFile;

            switch (arch)
            {
                case Arch::X86_64:
                {
                    switch (fmt)
                    {
                        case Format::ELF:
                        {
                            codegen::ELF64Codegen cg;
                            cg.generate(linearIR, symbolName, objFile);
                            break;
                        }
                        case Format::PE:
                        {
                            codegen::PE64Codegen cg;
                            cg.generate(linearIR, symbolName, objFile);
                            break;
                        }
                        case Format::MachO:
                        {
                            result.error =
                                "x86-64 Mach-O not implemented";
                            return result;
                        }
                    }
                    break;
                }

                case Arch::ARM64:
                {
                    switch (fmt)
                    {
                        case Format::ELF:
                        {
                            codegen::ELFARM64Codegen cg;
                            cg.generate(linearIR, symbolName, objFile);
                            break;
                        }
                        case Format::PE:
                        {
                            codegen::PEARM64Codegen cg;
                            cg.generate(linearIR, symbolName, objFile);
                            break;
                        }
                        case Format::MachO:
                        {
                            codegen::MachOARM64Codegen cg;
                            cg.generate(linearIR, symbolName, objFile);
                            break;
                        }
                    }
                    break;
                }

                case Arch::RISCV64:
                {
                    switch (fmt)
                    {
                        case Format::ELF:
                        {
                            codegen::ELFRISCV64Codegen cg;
                            cg.generate(linearIR, symbolName, objFile);
                            break;
                        }
                        case Format::PE:
                        {
                            codegen::PERISCV64Codegen cg;
                            cg.generate(linearIR, symbolName, objFile);
                            break;
                        }
                        case Format::MachO:
                        {
                            codegen::MachORISCV64Codegen cg;
                            cg.generate(linearIR, symbolName, objFile);
                            break;
                        }
                    }
                    break;
                }

                case Arch::RISCV32:
                {
                    switch (fmt)
                    {
                        case Format::ELF:
                        {
                            codegen::ELFRISCV32Codegen cg;
                            cg.generate(linearIR, symbolName, objFile);
                            break;
                        }
                        case Format::PE:
                        {
                            codegen::PERISCV32Codegen cg;
                            cg.generate(linearIR, symbolName, objFile);
                            break;
                        }
                        case Format::MachO:
                        {
                            codegen::MachORISCV32Codegen cg;
                            cg.generate(linearIR, symbolName, objFile);
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
            result.error = "unknown error";
        }

        return result;
    }

    // -------------------------------------------------------------------------
    // C++ API wrapper (thin, ABI-stable)
    // -------------------------------------------------------------------------
    std::vector<std::uint8_t> compile(const frontend::Module& module,
                                      LLGOArch     arch,
                                      LLGOFormat   fmt,
                                      LLGOOptLevel opt,
                                      const std::string& symbolName)
    {
        CompileResult r = compile_internal(
            module,
            static_cast<Arch>(arch),
            static_cast<Format>(fmt),
            static_cast<OptLevel>(opt),
            symbolName
        );

        if (!r.ok)
            return {};

        return r.objectData;
    }

} // namespace llgo

// -----------------------------------------------------------------------------
// C API implementation
// -----------------------------------------------------------------------------
extern "C"
{

// Opaque structs
struct LLGOModule_   { llgo::frontend::Module mod; };
struct LLGOFunction_ { llgo::frontend::Function fn; };
struct LLGOBlock_    { llgo::frontend::Block blk; };
struct LLGOResult_   { std::vector<std::uint8_t> data; };

// --- Module API --------------------------------------------------------------

LLGOModule* llgo_module_create(void)
{
    return new LLGOModule_();
}

void llgo_module_free(LLGOModule* m)
{
    delete m;
}

// --- Function API ------------------------------------------------------------

LLGOFunction* llgo_function_create(LLGOModule* m,
                                   LLGOType    ret,
                                   const char* name)
{
    auto* f = new LLGOFunction_();
    f->fn.name       = name;
    f->fn.returnType = static_cast<llgo::frontend::Type>(ret);
    m->mod.functions.push_back(f->fn);
    return f;
}

void llgo_function_add_param(LLGOFunction* f,
                             LLGOType      t,
                             const char*   name)
{
    llgo::frontend::Value v;
    v.type = static_cast<llgo::frontend::Type>(t);
    v.name = name;
    f->fn.params.push_back(v);
}

// --- Block API ---------------------------------------------------------------

LLGOBlock* llgo_block_create(LLGOFunction* f, const char* name)
{
    auto* b = new LLGOBlock_();
    b->blk.name = name;
    f->fn.blocks.push_back(b->blk);
    return b;
}

// --- Instruction API ---------------------------------------------------------

void llgo_block_append_instr(LLGOBlock*    b,
                             const char*   resultName,
                             LLGOType      resultType,
                             LLGOInstrKind kind,
                             const char**  operandNames,
                             std::size_t   numOperands,
                             const char*   control,
                             const char*   memory,
                             const char*   targetTrue,
                             const char*   targetFalse)
{
    llgo::frontend::Instr i;
    i.kind       = static_cast<llgo::frontend::InstrKind>(kind);
    i.resultType = static_cast<llgo::frontend::Type>(resultType);
    i.resultName = resultName ? resultName : "";

    for (std::size_t n = 0; n < numOperands; ++n)
    {
        llgo::frontend::Value v;
        v.name = operandNames[n];
        i.operands.push_back(v);
    }

    i.control     = control     ? control     : "";
    i.memory      = memory      ? memory      : "";
    i.targetTrue  = targetTrue  ? targetTrue  : "";
    i.targetFalse = targetFalse ? targetFalse : "";

    b->blk.instructions.push_back(i);
}

// --- Compilation API ---------------------------------------------------------

LLGOResult* llgo_compile(const LLGOModule* m,
                         const LLGOCompileOptions* opt)
{
    if (!m || !opt)
        return nullptr;

    llgo::CompileResult r = llgo::compile_internal(
        m->mod,
        static_cast<llgo::Arch>(opt->arch),
        static_cast<llgo::Format>(opt->format),
        static_cast<llgo::OptLevel>(opt->optLevel),
        opt->symbolName ? opt->symbolName : "main"
    );

    if (!r.ok)
        return nullptr;

    auto* res = new LLGOResult_();
    res->data = std::move(r.objectData);
    return res;
}

std::size_t llgo_result_size(const LLGOResult* r)
{
    return r ? r->data.size() : 0;
}

const std::uint8_t* llgo_result_data(const LLGOResult* r)
{
    return r ? r->data.data() : nullptr;
}

void llgo_result_free(LLGOResult* r)
{
    delete r;
}

} // extern "C"
