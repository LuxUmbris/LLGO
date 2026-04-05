# LLGO

A compact, dependency-free compiler backend library — everything LLVM offers for generating native object files, packaged as a single **static library** built from one `.cpp` file.

You translate your AST or IR into **LLGO Frontend IR**, hand it to the library, and get back raw object-file bytes ready to write to disk and link.

---

## Features

| | |
|---|---|
| **Architectures** | x86-64 · ARM64 · RISC-V 64 · RISC-V 32 |
| **Object formats** | ELF · PE/COFF · Mach-O |
| **Optimiser** | Constant folding · Strength reduction · Algebraic identities · CSE · DCE |
| **Build** | Unity build — one `.cpp`, zero dependencies beyond a C++17 compiler |
| **API** | C API (`llgo_compile`) + C++ convenience overload (`llgo::compile`) |

### Supported target matrix

|              | ELF | PE/COFF | Mach-O |
|:-------------|:---:|:-------:|:------:|
| **x86-64**   | ✓   | ✓       |        |
| **ARM64**    | ✓   | ✓       | ✓      |
| **RISC-V 64**| ✓   | ✓       | ✓      |
| **RISC-V 32**| ✓   | ✓       | ✓      |

---

## Repository layout

```
src/
├── llgo.cpp                  # Unity build entry point + C/C++ API implementation
├── llgo_api.hpp              # Public API (C + C++)
│
├── frontend.hpp              # Frontend IR: Module / Function / Block / Instr
├── son_builder.hpp           # Frontend IR → Sea-of-Nodes graph
├── optimizer.hpp             # SoN optimiser (fold · strength-reduce · CSE · DCE)
├── alloc.hpp                 # Arch-agnostic arena allocator
├── realloc.hpp               # In-place arena reallocator
├── lowering.hpp              # SoN graph → linear IR
│
├── codegen_x86_64_def.hpp    # x86-64 assembler + register allocator + IR lowering
├── codegen_arm64_def.hpp     # ARM64  assembler + register allocator + IR lowering
├── codegen_riscv64_def.hpp   # RISC-V 64 assembler + register allocator + IR lowering
├── codegen_riscv32_def.hpp   # RISC-V 32 assembler + register allocator + IR lowering
│
├── codegen_elf_x86_64.hpp    # ELF-64  x86-64  object file emitter
├── codegen_pe_x86_64.hpp     # PE/COFF x86-64  object file emitter
├── codegen_elf_arm64.hpp     # ELF-64  ARM64   object file emitter
├── codegen_pe_arm64.hpp      # PE/COFF ARM64   object file emitter
├── codegen_macho_arm64.hpp   # Mach-O  ARM64   object file emitter
├── codegen_elf_risc64.hpp    # ELF-64  RISC-V 64 object file emitter
├── codegen_pe_riscv64.hpp    # PE/COFF RISC-V 64 object file emitter
├── codegen_macho_riscv64.hpp # Mach-O  RISC-V 64 object file emitter
├── codegen_elf_riscv32.hpp   # ELF-32  RISC-V 32 object file emitter
├── codegen_pe_riscv32.hpp    # PE/COFF RISC-V 32 object file emitter
├── codegen_macho_riscv32.hpp # Mach-O  RISC-V 32 object file emitter
│
└── test_llgo.cpp             # Test suite (31 cases)
```

---

## Build

### CMake (recommended)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
# run tests
cd build && ctest --output-on-failure
```

### Manual

```bash
g++ -std=c++17 -O2 -Isrc -c src/llgo.cpp -o llgo.o
ar rcs libllgo.a llgo.o
```

Requirements: any C++17 compiler (GCC ≥ 7, Clang ≥ 5, MSVC 19.14+). No third-party dependencies.

---

## Compile pipeline

```
Your AST / IR
     │
     ▼
Frontend IR          (frontend.hpp)
  Module → Function → Block → Instr
     │
     ▼
Sea-of-Nodes graph   (son_builder.hpp)
     │
     ▼
Optimiser            (optimizer.hpp)
  • Constant folding          (3 + 4  →  7)
  • Strength reduction        (x * 8  →  x << 3)
  • Algebraic identities      (x - x  →  0)
  • Common subexpression elim
  • Dead code elimination
     │
     ▼
Arena alloc / realloc  (alloc.hpp, realloc.hpp)
     │
     ▼
Linear IR lowering   (lowering.hpp)
  topological sort → block assignment → linear instruction list
     │
     ▼
Code generation      (codegen_<arch>_def.hpp)
  greedy register allocation → instruction selection → branch patching
     │
     ▼
Object file emitter  (codegen_<fmt>_<arch>.hpp)
  ELF / PE / Mach-O  →  raw bytes
```

---

## Usage

### C++ API

Include `llgo_api.hpp` and link against `libllgo.a`.

```cpp
#include "llgo_api.hpp"
#include "frontend.hpp"

// 1. Build frontend IR
llgo::frontend::Module mod;
llgo::frontend::Function fn;
fn.name       = "add";
fn.returnType = llgo::frontend::Type::i64;

llgo::frontend::Value a, b;
a.type = b.type = llgo::frontend::Type::i64;
a.name = "a"; b.name = "b";
fn.params = { a, b };

llgo::frontend::Block blk;
blk.name = "entry";

// %sum = add a, b
llgo::frontend::Instr addInstr;
addInstr.kind       = llgo::frontend::InstrKind::Add;
addInstr.resultType = llgo::frontend::Type::i64;
addInstr.resultName = "%sum";
addInstr.operands   = { a, b };
blk.instructions.push_back(addInstr);

// ret %sum
llgo::frontend::Instr retInstr;
retInstr.kind       = llgo::frontend::InstrKind::Ret;
retInstr.resultType = llgo::frontend::Type::i64;
llgo::frontend::Value rv; rv.name = "%sum"; rv.type = llgo::frontend::Type::i64;
retInstr.operands   = { rv };
blk.instructions.push_back(retInstr);

fn.blocks.push_back(blk);
mod.functions.push_back(fn);

// 2. Compile → ELF x86-64 at O1
std::vector<std::uint8_t> obj =
    llgo::compile(mod,
                  LLGO_ARCH_X86_64,
                  LLGO_FMT_ELF,
                  LLGO_OPT_O1,
                  "add");

// 3. Write to disk
std::ofstream f("add.o", std::ios::binary);
f.write(reinterpret_cast<const char*>(obj.data()), obj.size());
```

### C API

```c
#include "llgo_api.hpp"

LLGOModule*   mod = llgo_module_create();
LLGOFunction* fn  = llgo_function_create(mod, LLGO_TYPE_I64, "my_func");
LLGOBlock*    blk = llgo_block_create(fn, "entry");

// ConstInt 42
llgo_block_append_instr(blk, "42", LLGO_TYPE_I64, LLGO_INSTR_CONST_INT,
                        NULL, 0, "", "", "", "");

// ret 42
const char* ops[] = { "42" };
llgo_block_append_instr(blk, "", LLGO_TYPE_I64, LLGO_INSTR_RET,
                        ops, 1, "", "", "", "");

LLGOCompileOptions opts = {
    .arch       = LLGO_ARCH_RISCV64,
    .format     = LLGO_FMT_ELF,
    .optLevel   = LLGO_OPT_O1,
    .symbolName = "my_func"
};

LLGOResult* result = llgo_compile(mod, &opts);
if (result) {
    // use llgo_result_data(result) / llgo_result_size(result)
    llgo_result_free(result);
}
llgo_module_free(mod);  // also frees fn and blk
```

---

## Frontend IR reference

### Types

| Enum | Width | Description |
|------|-------|-------------|
| `i2` … `i256` | 2–256 bit | Signed integers |
| `u1` … `u256` | 1–256 bit | Unsigned integers |
| `const_u8` | — | String literal (`const u8[]`) |
| `boolean` | — | Logical value |
| `arr` | metadata | Array (size in `TypeInfo`) |
| `ptr` | metadata | Pointer (element kind in `TypeInfo`) |

### Instructions

| Kind | Operands | Description |
|------|----------|-------------|
| `Add` `Sub` `Mul` `Div` `Mod` | lhs, rhs | Arithmetic |
| `Icmp` `Fcmp` | lhs, rhs | Integer / float compare |
| `Load` | ptr | Load from memory |
| `Store` | val, ptr | Store to memory |
| `Gep` | base, index… | Get element pointer |
| `Phi` | val₀, val₁, … | SSA Φ-node |
| `Br` | — | Unconditional branch (`target` field) |
| `CondBr` | cond | Conditional branch (`targetTrue`/`targetFalse`) |
| `Ret` | val? | Return |
| `Call` | fn, arg… | Function call |
| `ConstInt` | — | Integer constant (value in `resultName`) |
| `ConstFloat` | — | Float constant |
| `ConstString` | — | String constant |
| `Undef` | — | Undefined value |

---

## Optimiser passes

| Pass | Level | Description |
|------|-------|-------------|
| Constant folding | O1+ | Evaluates pure arithmetic on constant inputs at compile time |
| Strength reduction | O1+ | Replaces `x * 2ⁿ` with `x << n`, removes `x ± 0`, `x * 1` |
| Algebraic identities | O1+ | `x - x → 0` and similar |
| CSE | O1+ | Deduplicates identical pure subexpressions |
| DCE | O1+ | Removes nodes not contributing to any side-effectful root |
| Unreachable removal | O1+ | Prunes nodes unreachable from the start node |
| Trivial merge compression | O1+ | Collapses single-predecessor merge nodes |
| Double pass | O2 | All O1 passes run twice to catch cascading opportunities |

---

## Design notes

**Unity build.** All source files are headers (for the include graph); `llgo.cpp` is the single compilation unit. This gives fast builds, optimal inlining, and trivial integration — just add one `.cpp` to your project.

**Sea-of-Nodes IR.** Values are nodes in a directed graph; control and memory dependencies are explicit edges. This representation makes optimisation passes simple and correct by construction — no def-use chains to maintain separately.

**Arch-agnostic arena.** `alloc.hpp` uses 16-byte alignment universally, so the same allocator works on x86-64, ARM64, and both RISC-V widths without `#ifdef`.

**Greedy register allocator.** Each codegen backend uses a simple linear-scan greedy allocator over the caller-save register set. It is intentionally straightforward — good enough for a backend library, trivially replaceable with a full linear-scan or graph-colouring allocator if needed.

---

## License

See [LICENSE](LICENSE).
