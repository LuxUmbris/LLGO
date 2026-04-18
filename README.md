# LLGO – Low Level Graph Optimizer

LLGO is a compact, self‑contained compiler pipeline designed for embedding, JIT‑style execution, ahead‑of‑time code generation, and language‑agnostic IR experimentation.  
It transforms a simple SSA‑based frontend IR into an optimized Sea‑of‑Nodes graph, lowers it into a linear instruction stream, allocates registers, and finally emits real object files for multiple architectures and formats.

LLGO is implemented as a unity build: all implementation headers are included into a single translation unit (llgo.cpp).  
This makes LLGO extremely easy to integrate — one .cpp file compiles into a static library.

---

# Why LLGO?

Most compiler backends are either:

• **too large** (LLVM, GCC)  
• **too limited** (toy compilers, educational projects)  
• **too opinionated** (language‑specific JITs)  
• **too heavy to embed** (runtime dependencies, dynamic linking, platform quirks)

LLGO exists to fill the gap:

### 1. A real backend without the weight of LLVM  
LLGO emits **real object files** (ELF, PE/COFF, Mach‑O) for multiple architectures.  
No IR bitcode, no JIT‑only mode, no runtime dependencies.

### 2. Fully self‑contained  
Everything is header‑only except the unity build entry point.  
No external libraries. No platform‑specific hacks.

### 3. Language‑agnostic  
You can feed LLGO any IR that maps to its minimal SSA‑style frontend.  
It does not assume C, C++, Rust, Go, or any language semantics.

### 4. Designed for embedding  
LLGO is ideal for:

• custom languages  
• DSLs  
• shader compilers  
• JIT engines  
• build systems  
• code generation tools  
• educational compilers  
• static analysis tools that need object output

### 5. Predictable, explicit, transparent  
LLGO avoids magic.  
Every stage is explicit:

• Sea‑of‑Nodes graph  
• Optimizer  
• Lowering  
• Register allocation  
• Codegen

You can inspect or modify any stage.

### 6. Stable C API  
The C API is ABI‑safe and works from any language.  
No STL types cross the boundary.

---

# Developer Guide

This section explains how to use LLGO as a developer:  
how to build IR, how to compile, how to debug, and how to integrate LLGO into your toolchain.

---

## 1. Building a Module

A module contains functions. A function contains blocks. A block contains instructions.  
LLGO has no builder API — you construct `frontend::Module` structs directly in C++.

```C++
#include "frontend.hpp"

llgo::frontend::Module mod;

llgo::frontend::Function fn;
fn.name       = "add_one";
fn.returnType = llgo::frontend::Type::i64;
fn.params.push_back({ llgo::frontend::Type::i64, "x" });

llgo::frontend::Block entry;
entry.name = "entry";
fn.blocks.push_back(entry);

mod.functions.push_back(fn);
```

---

## 2. Adding Instructions

Instructions are appended to a block's `instructions` vector.  
Each `Instr` carries: a kind, a result type, a result name, a list of `Value` operands, optional control/memory dependency names, and optional branch target names.

### Example: x + 1

```C++
llgo::frontend::Instr add;
add.kind        = llgo::frontend::InstrKind::Add;
add.resultType  = llgo::frontend::Type::i64;
add.resultName  = "tmp";
add.operands    = { { llgo::frontend::Type::i64, "x" },
                    { llgo::frontend::Type::i64, "1" } };
// control / memory / branch fields default to ""
mod.functions[0].blocks[0].instructions.push_back(add);
```

### Example: return tmp

```C++
llgo::frontend::Instr ret;
ret.kind       = llgo::frontend::InstrKind::Ret;
ret.resultType = llgo::frontend::Type::i64;
ret.operands   = { { llgo::frontend::Type::i64, "tmp" } };
mod.functions[0].blocks[0].instructions.push_back(ret);
```

Integer literals are written as plain decimal strings in operand names (`"1"`, `"-42"`).  
The SONBuilder recognises them automatically and creates `ConstInt` nodes.

---

## 3. Compiling a Module

### C++ API

```C++
#include "llgo_api.hpp"

llgo::CompileOptions opt;
opt.arch       = llgo::Arch::X86_64;
opt.fmt        = llgo::Format::ELF;
opt.opt        = llgo::OptLevel::O2;
opt.symbolName = "add_one";

llgo::CompileResult res = llgo::compile(mod, opt);
if (!res.ok) {
    fprintf(stderr, "compile error: %s\n", res.error.c_str());
}
```

### C API

The C API receives a pointer to a `llgo::frontend::Module` cast to `void*`.

```C
#include "llgo_api.hpp"

llgo::frontend::Module mod = /* ... build as above ... */;

LLGOCompileOptions opt;
opt.arch       = LLGO_ARCH_X86_64;
opt.format     = LLGO_FMT_ELF;      // field is "format", not "fmt"
opt.optLevel   = LLGO_OPT_O2;
opt.symbolName = "add_one";

LLGOResult r = llgo_compile_c(&mod, &opt);
if (!r.ok) {
    fprintf(stderr, "compile error: %s\n", r.error);
    llgo_result_free_c(&r);
}
```

---

## 4. Writing the Object File

### C++ API

```C++
// Option A: write manually
FILE* fp = fopen("out.o", "wb");
fwrite(res.objectData.data(), 1, res.objectData.size(), fp);
fclose(fp);

// Option B: convenience helper
std::string err;
llgo::compileToFile(mod, "out.o", opt, err);
```

### C API

```C
// res.data and res.size hold the raw object bytes
FILE* fp = fopen("out.o", "wb");
fwrite(r.data, 1, r.size, fp);
fclose(fp);
llgo_result_free_c(&r);   // always free after use

// Or use the file helper directly:
const char* err = nullptr;
int ok = llgo_compile_to_file_c(&mod, "out.o", &opt, &err);
```

---

## 5. Debugging Tips

### Inspect the Sea‑of‑Nodes graph  
Insert debug prints in SONBuilder or SONOptimizer to visualize nodes.

### Inspect Linear IR  
Dump LinearIR before codegen to verify lowering.

### Inspect generated machine code  
Use:

• objdump  
• llvm-objdump  
• radare2  
• Hopper  
• IDA  
• Binary Ninja

### Check alignment and relocations  
Especially important for RISC‑V and ARM64.

### Validate object files  
Use:

```
readelf -a out.o  
objdump -d out.o  
llvm-readobj -all out.o  
```

---

## 6. Integration Patterns

### Embedding in a language compiler  
Map your AST → LLGO frontend::Module → llgo_compile → object file → system linker.

### Embedding in a JIT  
Generate IR on the fly, compile to object bytes, load into memory, link, execute.

### Using LLGO as a backend for DSLs  
Perfect for shader languages, math DSLs, robotics DSLs, etc.

### Using LLGO in build systems  
Generate small object files programmatically.

---

# Supported Architectures & Formats

### x86‑64  
• ELF64  
• PE/COFF  
• Mach‑O (not implemented)

### ARM64  
• ELF64  
• PE/COFF  
• Mach‑O

### RISC‑V 64  
• ELF64  
• PE/COFF  
• Mach‑O

### RISC‑V 32  
• ELF32  
• PE/COFF  
• Mach‑O

---

# License

LLGO is licensed under the **Mozilla Public License 2.0**.

---
