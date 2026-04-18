/* LLGO Public API */

#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "frontend.hpp"

// -------------------------
// C++ PUBLIC API (DECLARATIONS ONLY)
// -------------------------
namespace llgo {

enum class Arch { X86_64, ARM64, RISCV64, RISCV32 };
enum class Format { ELF, PE, MachO };
enum class OptLevel { O0, O1, O2 };

struct CompileOptions {
    Arch arch = Arch::X86_64;
    Format fmt = Format::ELF;
    OptLevel opt = OptLevel::O1;
    std::string symbolName = "main";
};

struct CompileResult {
    bool ok = false;
    std::string error;
    std::vector<std::uint8_t> objectData;
};

CompileResult compile(const frontend::Module&, const CompileOptions&);
bool compileToFile(const frontend::Module&, const std::string&, const CompileOptions&, std::string&);

bool archFromString(const std::string&, Arch&);
bool formatFromString(const std::string&, Format&);
bool optLevelFromInt(int, OptLevel&);
const char* version();

} // namespace llgo

// -------------------------
// C API DECLARATIONS
// -------------------------
extern "C" {

typedef enum {
    LLGO_ARCH_X86_64,
    LLGO_ARCH_ARM64,
    LLGO_ARCH_RISCV64,
    LLGO_ARCH_RISCV32
} LLGOArch;

typedef enum {
    LLGO_FMT_ELF,
    LLGO_FMT_PE,
    LLGO_FMT_MACHO
} LLGOFormat;

typedef enum {
    LLGO_OPT_O0,
    LLGO_OPT_O1,
    LLGO_OPT_O2
} LLGOOptLevel;

typedef struct {
    LLGOArch arch;
    LLGOFormat format;
    LLGOOptLevel optLevel;
    const char* symbolName;
} LLGOCompileOptions;

typedef struct LLGOResult_ {
    int ok;
    char* error;
    uint8_t* data;
    std::size_t size;
} LLGOResult;

LLGOResult llgo_compile_c(const void*, const LLGOCompileOptions*);
int llgo_compile_to_file_c(const void*, const char*, const LLGOCompileOptions*, const char**);
void llgo_result_free_c(LLGOResult*);

int llgo_arch_from_string_c(const char*, LLGOArch*);
int llgo_format_from_string_c(const char*, LLGOFormat*);
int llgo_opt_level_from_int_c(int, LLGOOptLevel*);

const char* llgo_version_c(void);

} // extern "C"
