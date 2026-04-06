/* LLGO Public API */

#pragma once
#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// C ABI enums
typedef enum LLGOArch {
    LLGO_ARCH_X86_64 = 0,
    LLGO_ARCH_ARM64  = 1,
    LLGO_ARCH_RISCV64 = 2,
    LLGO_ARCH_RISCV32 = 3
} LLGOArch;

typedef enum LLGOFormat {
    LLGO_FMT_ELF   = 0,
    LLGO_FMT_PE    = 1,
    LLGO_FMT_MACHO = 2
} LLGOFormat;

typedef enum LLGOOptLevel {
    LLGO_OPT_O0 = 0,
    LLGO_OPT_O1 = 1,
    LLGO_OPT_O2 = 2
} LLGOOptLevel;

// C ABI compile options (POD)
typedef struct LLGOCompileOptions {
    LLGOArch     arch;
    LLGOFormat   format;
    LLGOOptLevel optLevel;
    const char*  symbolName; // NULL → "main"
} LLGOCompileOptions;

// C ABI compile result (POD)
typedef struct LLGOResult {
    int            ok;     // nonzero = success
    const char*    error;  // NULL if ok != 0
    const uint8_t* data;   // object file bytes
    std::size_t    size;   // length of data
} LLGOResult;

// C ABI functions
LLGOResult llgo_compile_c(const void* module,
                          const LLGOCompileOptions* opts);

int llgo_compile_to_file_c(const void* module,
                           const char* path,
                           const LLGOCompileOptions* opts,
                           const char** errorOut);

void llgo_result_free_c(LLGOResult* result);

int llgo_arch_from_string_c(const char* s, LLGOArch* out);
int llgo_format_from_string_c(const char* s, LLGOFormat* out);
int llgo_opt_level_from_int_c(int n, LLGOOptLevel* out);

const char* llgo_version_c(void);

#ifdef __cplusplus
} // extern "C"
#endif


// ============================================================================
// C++ API (header‑inline, not part of shared ABI)
// ============================================================================

#ifdef __cplusplus

#include <string>
#include <vector>
#include "frontend.hpp"

namespace llgo
{
    enum class Arch { X86_64, ARM64, RISCV64, RISCV32 };
    enum class Format { ELF, PE, MachO };
    enum class OptLevel { O0, O1, O2 };

    struct CompileOptions {
        Arch        arch = Arch::X86_64;
        Format      fmt  = Format::ELF;
        OptLevel    opt  = OptLevel::O1;
        std::string symbolName = "main";
    };

    struct CompileResult {
        bool                      ok = false;
        std::string               error;
        std::vector<std::uint8_t> objectData;
    };

    // enum mapping helpers
    inline LLGOArch toC(Arch a) {
        switch (a) {
            case Arch::X86_64: return LLGO_ARCH_X86_64;
            case Arch::ARM64:  return LLGO_ARCH_ARM64;
            case Arch::RISCV64:return LLGO_ARCH_RISCV64;
            case Arch::RISCV32:return LLGO_ARCH_RISCV32;
        }
        return LLGO_ARCH_X86_64;
    }

    inline LLGOFormat toC(Format f) {
        switch (f) {
            case Format::ELF:   return LLGO_FMT_ELF;
            case Format::PE:    return LLGO_FMT_PE;
            case Format::MachO: return LLGO_FMT_MACHO;
        }
        return LLGO_FMT_ELF;
    }

    inline LLGOOptLevel toC(OptLevel o) {
        switch (o) {
            case OptLevel::O0: return LLGO_OPT_O0;
            case OptLevel::O1: return LLGO_OPT_O1;
            case OptLevel::O2: return LLGO_OPT_O2;
        }
        return LLGO_OPT_O1;
    }

    // C++ wrapper around C ABI
    inline CompileResult compile(const frontend::Module& module,
                                 const CompileOptions& opts = {})
    {
        LLGOCompileOptions cOpts;
        cOpts.arch       = toC(opts.arch);
        cOpts.format     = toC(opts.fmt);
        cOpts.optLevel   = toC(opts.opt);
        cOpts.symbolName = opts.symbolName.empty()
                           ? nullptr
                           : opts.symbolName.c_str();

        LLGOResult cRes = llgo_compile_c(&module, &cOpts);

        CompileResult out;
        out.ok = (cRes.ok != 0);

        if (!out.ok) {
            out.error = cRes.error ? cRes.error : "unknown error";
        } else if (cRes.data && cRes.size > 0) {
            out.objectData.assign(cRes.data, cRes.data + cRes.size);
        }

        llgo_result_free_c(&cRes);
        return out;
    }

    inline bool compileToFile(const frontend::Module& module,
                              const std::string& path,
                              const CompileOptions& opts,
                              std::string& errorOut)
    {
        LLGOCompileOptions cOpts;
        cOpts.arch       = toC(opts.arch);
        cOpts.format     = toC(opts.fmt);
        cOpts.optLevel   = toC(opts.opt);
        cOpts.symbolName = opts.symbolName.empty()
                           ? nullptr
                           : opts.symbolName.c_str();

        const char* err = nullptr;
        int ok = llgo_compile_to_file_c(&module, path.c_str(), &cOpts, &err);

        if (!ok) {
            errorOut = err ? err : "unknown error";
            return false;
        }
        return true;
    }

    inline bool archFromString(const std::string& s, Arch& out) {
        LLGOArch a;
        if (!llgo_arch_from_string_c(s.c_str(), &a)) return false;
        switch (a) {
            case LLGO_ARCH_X86_64: out = Arch::X86_64; break;
            case LLGO_ARCH_ARM64:  out = Arch::ARM64;  break;
            case LLGO_ARCH_RISCV64:out = Arch::RISCV64;break;
            case LLGO_ARCH_RISCV32:out = Arch::RISCV32;break;
        }
        return true;
    }

    inline bool formatFromString(const std::string& s, Format& out) {
        LLGOFormat f;
        if (!llgo_format_from_string_c(s.c_str(), &f)) return false;
        switch (f) {
            case LLGO_FMT_ELF:   out = Format::ELF;   break;
            case LLGO_FMT_PE:    out = Format::PE;    break;
            case LLGO_FMT_MACHO: out = Format::MachO; break;
        }
        return true;
    }

    inline bool optLevelFromInt(int n, OptLevel& out) {
        LLGOOptLevel o;
        if (!llgo_opt_level_from_int_c(n, &o)) return false;
        switch (o) {
            case LLGO_OPT_O0: out = OptLevel::O0; break;
            case LLGO_OPT_O1: out = OptLevel::O1; break;
            case LLGO_OPT_O2: out = OptLevel::O2; break;
        }
        return true;
    }

    inline const char* version() {
        return llgo_version_c();
    }

    [[maybe_unused]] void prevent_dce_full_pipeline();

} // namespace llgo

#endif // __cplusplus
