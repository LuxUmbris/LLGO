/* LLGO Public API */

#pragma once
#include "frontend.hpp"

#include <cstring>
#include <string>

// Internal POD used to store result memory
struct LLGOResult_ {
    int ok;
    char* error;
    uint8_t* data;
    std::size_t size;
};

static char* dup_cstr(const std::string& s) {
    char* p = (char*)std::malloc(s.size() + 1);
    if (!p) return nullptr;
    std::memcpy(p, s.c_str(), s.size() + 1);
    return p;
}

static uint8_t* dup_bytes(const std::vector<uint8_t>& v) {
    if (v.empty()) return nullptr;
    uint8_t* p = (uint8_t*)std::malloc(v.size());
    if (!p) return nullptr;
    std::memcpy(p, v.data(), v.size());
    return p;
}

// -----------------------------------------------------------------------------
// C API: compile
// -----------------------------------------------------------------------------
LLGOResult llgo_compile_c(const void* modulePtr,
                          const LLGOCompileOptions* opts)
{
    LLGOResult out{};
    out.ok = 0;
    out.error = nullptr;
    out.data = nullptr;
    out.size = 0;

    if (!modulePtr || !opts) {
        out.error = dup_cstr("invalid arguments");
        return out;
    }

    const llgo::frontend::Module* mod =
        reinterpret_cast<const llgo::frontend::Module*>(modulePtr);

    llgo::CompileOptions cppOpts;
    cppOpts.symbolName = opts->symbolName ? opts->symbolName : "main";

    switch (opts->arch) {
        case LLGO_ARCH_X86_64: cppOpts.arch = llgo::Arch::X86_64; break;
        case LLGO_ARCH_ARM64:  cppOpts.arch = llgo::Arch::ARM64;  break;
        case LLGO_ARCH_RISCV64:cppOpts.arch = llgo::Arch::RISCV64;break;
        case LLGO_ARCH_RISCV32:cppOpts.arch = llgo::Arch::RISCV32;break;
    }

    switch (opts->format) {
        case LLGO_FMT_ELF:   cppOpts.fmt = llgo::Format::ELF;   break;
        case LLGO_FMT_PE:    cppOpts.fmt = llgo::Format::PE;    break;
        case LLGO_FMT_MACHO: cppOpts.fmt = llgo::Format::MachO; break;
    }

    switch (opts->optLevel) {
        case LLGO_OPT_O0: cppOpts.opt = llgo::OptLevel::O0; break;
        case LLGO_OPT_O1: cppOpts.opt = llgo::OptLevel::O1; break;
        case LLGO_OPT_O2: cppOpts.opt = llgo::OptLevel::O2; break;
    }

    llgo::CompileResult res = llgo::compile(*mod, cppOpts);

    if (!res.ok) {
        out.error = dup_cstr(res.error);
        return out;
    }

    out.ok = 1;
    out.size = res.objectData.size();
    out.data = dup_bytes(res.objectData);
    return out;
}

// -----------------------------------------------------------------------------
// C API: compile to file
// -----------------------------------------------------------------------------
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

    switch (opts->arch) {
        case LLGO_ARCH_X86_64: cppOpts.arch = llgo::Arch::X86_64; break;
        case LLGO_ARCH_ARM64:  cppOpts.arch = llgo::Arch::ARM64;  break;
        case LLGO_ARCH_RISCV64:cppOpts.arch = llgo::Arch::RISCV64;break;
        case LLGO_ARCH_RISCV32:cppOpts.arch = llgo::Arch::RISCV32;break;
    }

    switch (opts->format) {
        case LLGO_FMT_ELF:   cppOpts.fmt = llgo::Format::ELF;   break;
        case LLGO_FMT_PE:    cppOpts.fmt = llgo::Format::PE;    break;
        case LLGO_FMT_MACHO: cppOpts.fmt = llgo::Format::MachO; break;
    }

    switch (opts->optLevel) {
        case LLGO_OPT_O0: cppOpts.opt = llgo::OptLevel::O0; break;
        case LLGO_OPT_O1: cppOpts.opt = llgo::OptLevel::O1; break;
        case LLGO_OPT_O2: cppOpts.opt = llgo::OptLevel::O2; break;
    }

    std::string err;
    bool ok = llgo::compileToFile(*mod, path, cppOpts, err);

    if (!ok) {
        if (errorOut) *errorOut = dup_cstr(err);
        return 0;
    }

    return 1;
}

// -----------------------------------------------------------------------------
// C API: free result
// -----------------------------------------------------------------------------
void llgo_result_free_c(LLGOResult* r)
{
    if (!r) return;
    if (r->error) std::free((void*)r->error);
    if (r->data)  std::free((void*)r->data);
    r->error = nullptr;
    r->data  = nullptr;
    r->size  = 0;
    r->ok    = 0;
}

// -----------------------------------------------------------------------------
// C API: enum parsers
// -----------------------------------------------------------------------------
int llgo_arch_from_string_c(const char* s, LLGOArch* out)
{
    if (!s || !out) return 0;
    std::string str(s);
    llgo::Arch a;
    if (!llgo::archFromString(str, a)) return 0;

    switch (a) {
        case llgo::Arch::X86_64: *out = LLGO_ARCH_X86_64; break;
        case llgo::Arch::ARM64:  *out = LLGO_ARCH_ARM64;  break;
        case llgo::Arch::RISCV64:*out = LLGO_ARCH_RISCV64;break;
        case llgo::Arch::RISCV32:*out = LLGO_ARCH_RISCV32;break;
    }
    return 1;
}

int llgo_format_from_string_c(const char* s, LLGOFormat* out)
{
    if (!s || !out) return 0;
    std::string str(s);
    llgo::Format f;
    if (!llgo::formatFromString(str, f)) return 0;

    switch (f) {
        case llgo::Format::ELF:   *out = LLGO_FMT_ELF;   break;
        case llgo::Format::PE:    *out = LLGO_FMT_PE;    break;
        case llgo::Format::MachO: *out = LLGO_FMT_MACHO; break;
    }
    return 1;
}

int llgo_opt_level_from_int_c(int n, LLGOOptLevel* out)
{
    if (!out) return 0;
    llgo::OptLevel o;
    if (!llgo::optLevelFromInt(n, o)) return 0;

    switch (o) {
        case llgo::OptLevel::O0: *out = LLGO_OPT_O0; break;
        case llgo::OptLevel::O1: *out = LLGO_OPT_O1; break;
        case llgo::OptLevel::O2: *out = LLGO_OPT_O2; break;
    }
    return 1;
}

// -----------------------------------------------------------------------------
// C API: version
// -----------------------------------------------------------------------------
const char* llgo_version_c(void)
{
    return llgo::version();
}
