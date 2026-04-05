/* LLGO Public API */

#pragma once
#include <cstddef>
#include <cstdint>

// -----------------------------------------------------------------
// C API – usable from any language with a C FFI.
// All pointers returned are owned by LLGO and freed via the
// corresponding llgo_*_free() function.
// -----------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// -------------------------------------------------------
// Opaque handles
// -------------------------------------------------------
typedef struct LLGOModule_   LLGOModule;
typedef struct LLGOFunction_ LLGOFunction;
typedef struct LLGOBlock_    LLGOBlock;
typedef struct LLGOResult_   LLGOResult;

// -------------------------------------------------------
// Target architecture
// -------------------------------------------------------
typedef enum LLGOArch
{
    LLGO_ARCH_X86_64   = 0,
    LLGO_ARCH_ARM64    = 1,
    LLGO_ARCH_RISCV64  = 2,
    LLGO_ARCH_RISCV32  = 3
} LLGOArch;

// -------------------------------------------------------
// Object-file format
// -------------------------------------------------------
typedef enum LLGOFormat
{
    LLGO_FMT_ELF   = 0,
    LLGO_FMT_PE    = 1,
    LLGO_FMT_MACHO = 2
} LLGOFormat;

// -------------------------------------------------------
// Optimisation level
// -------------------------------------------------------
typedef enum LLGOOptLevel
{
    LLGO_OPT_NONE = 0,
    LLGO_OPT_O1   = 1,
    LLGO_OPT_O2   = 2
} LLGOOptLevel;

// -------------------------------------------------------
// Value types (mirrors frontend::Type)
// -------------------------------------------------------
typedef enum LLGOType
{
    LLGO_TYPE_I2   = 0,
    LLGO_TYPE_I4,
    LLGO_TYPE_I8,
    LLGO_TYPE_I16,
    LLGO_TYPE_I32,
    LLGO_TYPE_I64,
    LLGO_TYPE_I128,
    LLGO_TYPE_I256,
    LLGO_TYPE_U1,
    LLGO_TYPE_U2,
    LLGO_TYPE_U4,
    LLGO_TYPE_U8,
    LLGO_TYPE_U16,
    LLGO_TYPE_U32,
    LLGO_TYPE_U64,
    LLGO_TYPE_U128,
    LLGO_TYPE_U256,
    LLGO_TYPE_CONST_U8,
    LLGO_TYPE_BOOLEAN,
    LLGO_TYPE_ARR,
    LLGO_TYPE_PTR
} LLGOType;

// -------------------------------------------------------
// Instruction kinds (mirrors frontend::InstrKind)
// -------------------------------------------------------
typedef enum LLGOInstrKind
{
    LLGO_INSTR_ADD = 0,
    LLGO_INSTR_SUB,
    LLGO_INSTR_MUL,
    LLGO_INSTR_DIV,
    LLGO_INSTR_MOD,
    LLGO_INSTR_ICMP,
    LLGO_INSTR_FCMP,
    LLGO_INSTR_LOAD,
    LLGO_INSTR_STORE,
    LLGO_INSTR_GEP,
    LLGO_INSTR_PHI,
    LLGO_INSTR_BR,
    LLGO_INSTR_CONDBR,
    LLGO_INSTR_RET,
    LLGO_INSTR_CALL,
    LLGO_INSTR_CONST_INT,
    LLGO_INSTR_CONST_FLOAT,
    LLGO_INSTR_CONST_STRING,
    LLGO_INSTR_UNDEF
} LLGOInstrKind;

// -------------------------------------------------------
// Compile options
// -------------------------------------------------------
typedef struct LLGOCompileOptions
{
    LLGOArch      arch;
    LLGOFormat    format;
    LLGOOptLevel  optLevel;
    const char*   symbolName; // exported function name; NULL → "main"
} LLGOCompileOptions;

// -------------------------------------------------------
// Result (compiled object file bytes)
// -------------------------------------------------------

// Returns the number of bytes in the compiled object file.
std::size_t llgo_result_size(const LLGOResult* result);

// Returns a pointer to the object file bytes (valid until llgo_result_free).
const std::uint8_t* llgo_result_data(const LLGOResult* result);

// Frees a compile result.
void llgo_result_free(LLGOResult* result);

// -------------------------------------------------------
// Module construction
// -------------------------------------------------------

// Creates a new, empty module.
LLGOModule* llgo_module_create(void);

// Frees a module and all its contents.
void llgo_module_free(LLGOModule* module);

// -------------------------------------------------------
// Function construction
// -------------------------------------------------------

// Appends a new function to the module.
// returnType:  LLGO_TYPE_* constant
// name:        null-terminated symbol name
LLGOFunction* llgo_function_create(LLGOModule*  module,
                                   LLGOType     returnType,
                                   const char*  name);

// Appends a parameter (SSA value) to the function.
void llgo_function_add_param(LLGOFunction* function,
                             LLGOType      type,
                             const char*   name);

// -------------------------------------------------------
// Block construction
// -------------------------------------------------------

// Appends a new basic block to the function.
LLGOBlock* llgo_block_create(LLGOFunction* function,
                             const char*   name);

// -------------------------------------------------------
// Instruction construction
// -------------------------------------------------------

// Appends an instruction to the block.
//
// resultName:   SSA name for the produced value ("" if none)
// resultType:   type of the produced value
// kind:         LLGO_INSTR_* constant
// operandNames: array of SSA operand names
// numOperands:  length of operandNames
// control:      control dependency name ("" for implicit)
// memory:       memory dependency name  ("" for implicit)
// targetTrue:   branch target (CondBr true  / Br target)
// targetFalse:  branch target (CondBr false)
void llgo_block_append_instr(LLGOBlock*    block,
                             const char*   resultName,
                             LLGOType      resultType,
                             LLGOInstrKind kind,
                             const char**  operandNames,
                             std::size_t   numOperands,
                             const char*   control,
                             const char*   memory,
                             const char*   targetTrue,
                             const char*   targetFalse);

// -------------------------------------------------------
// Compilation
// -------------------------------------------------------

// Compiles the module to a native object file.
// Returns NULL on failure (e.g. unsupported arch/format combination).
LLGOResult* llgo_compile(const LLGOModule*          module,
                         const LLGOCompileOptions*  options);

// -------------------------------------------------------
// Convenience: single-shot compile from a pre-built
// frontend::Module (C++ only, not exposed as C)
// -------------------------------------------------------
#ifdef __cplusplus
} // extern "C"

#include "frontend.hpp"

namespace llgo
{
    // Returns object-file bytes or empty vector on failure.
    std::vector<std::uint8_t> compile(const frontend::Module& module,
                                      LLGOArch     arch,
                                      LLGOFormat   format,
                                      LLGOOptLevel optLevel,
                                      const std::string& symbolName = "main");
}

#endif // __cplusplus
