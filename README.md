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

A module contains functions.  
A function contains blocks.  
A block contains instructions.

### Example: Creating a module

```C++
LLGOModule* m = llgo_module_create();
```

---

## 2. Creating a Function

Functions have a return type and a name.

```C++
LLGOFunction* f = llgo_function_create(m, LLGO_TYPE_I64, "main");  
llgo_function_add_param(f, LLGO_TYPE_I64, "x");  
```

---

## 3. Creating Blocks

Blocks represent basic blocks in SSA form.

```C++
LLGOBlock* entry = llgo_block_create(f, "entry");  
```

---

## 4. Emitting Instructions

Instructions are appended to blocks.  
Each instruction has:

• result name  
• result type  
• instruction kind  
• operand names  
• control dependency  
• memory dependency  
• branch targets (if applicable)

### Example: x + 1

```C++
const char* ops_add[] = { "x", "1" };  
llgo_block_append_instr(entry,  
    "tmp",  
    LLGO_TYPE_I64,  
    LLGO_INSTR_ADD,  
    ops_add, 2,  
    "", "", "", ""  
);  
```

### Example: return tmp

```C++
const char* ops_ret[] = { "tmp" };  
llgo_block_append_instr(entry,  
    "",  
    LLGO_TYPE_I64,  
    LLGO_INSTR_RET,  
    ops_ret, 1,  
    "", "", "", ""  
);  
```

---

## 5. Compiling a Module

### C API

```C
LLGOCompileOptions opt = {  
    LLGO_ARCH_X86_64,  
    LLGO_FMT_ELF,  
    LLGO_OPT_O2,  
    "main"  
};  

LLGOResult* r = llgo_compile(m, &opt);  
```

### C++ API

```C++
llgo::CompileOptions opt;  
opt.arch = llgo::Arch::RISCV64;  
opt.fmt  = llgo::Format::ELF;  
opt.opt  = llgo::OptLevel::O2;  
opt.symbolName = "main";  

auto res = llgo::compile(mod, opt);  
```

---

## 6. Writing the Object File

```C++
FILE* fp = fopen("out.o", "wb");  
fwrite(llgo_result_data(r), 1, llgo_result_size(r), fp);  
fclose(fp);  
```

---

## 7. Debugging Tips

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

## 8. Integration Patterns

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
