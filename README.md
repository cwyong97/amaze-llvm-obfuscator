# AmazeLLVM Obfuscator

An LLVM 15-based compiler plugin that applies multiple obfuscation transformations to C/C++ programs at the IR (Intermediate Representation) level, making reverse engineering significantly harder while preserving program correctness.

---

## Features

| Pass | Flag | Description |
|------|------|-------------|
| String Obfuscation | `--obf-string` | Encrypts string literals; decrypts at runtime via XOR |
| Constant Obfuscation | `--obf-const` | Replaces integer constants with MBA (Mixed Boolean-Arithmetic) expressions |
| Instruction Substitution | `--obf-sub` | Replaces standard arithmetic/bitwise ops with equivalent but complex expressions |
| Basic Block Splitting | `--obf-split` | Fragments basic blocks into smaller chunks to obscure control flow |
| Bogus Control Flow (BCF) | `--obf-bcf` | Inserts opaque predicates and fake branches that never execute |
| Enable All | `--obf-all` | Activates all passes in the correct pipeline order |

---

## Prerequisites

| Tool | Version | Purpose |
|------|---------|---------|
| LLVM / Clang | 15 | Compiler infrastructure and IR toolchain |
| `opt-15` | 15 | LLVM optimizer — loads the plugin and runs passes |
| `lli-15` | 15 | LLVM JIT executor for testing `.ll` files |
| CMake | ≥ 3.13 | Build system |
| Python 3 | ≥ 3.8 | Real-world test automation scripts |
| `bc` | any | Used by `run.sh stats` for ratio calculation |

Install LLVM 15 on Ubuntu/Debian:
```bash
sudo apt install llvm-15 clang-15 lld-15
```

---

## Build

```bash
# First-time setup
mkdir build && cd build
cmake -DLLVM_DIR=/usr/lib/llvm-15/cmake ..
cd ..

# Compile the plugin
cd build && make -j$(nproc) && cd ..
```

The compiled plugin will be at `build/libObfPass.so`.

Or use the helper script:
```bash
./run.sh build
```

---

## Quick Start

### Using `run.sh` (Recommended)

```bash
./run.sh run    # Build and produce obfuscated output.ll
./run.sh exec   # Build, obfuscate, and run the result
./run.sh elf    # Build obfuscated + baseline ELF binaries and execute both
./run.sh diff   # Show IR-level instruction diff (original vs obfuscated)
./run.sh stats  # Print instruction count and binary size bloat ratios
./run.sh clean  # Delete the build directory
./run.sh reset  # Clean rebuild from scratch
./run.sh help   # Show all commands
```

The target source file is `test/input.c`. Outputs land in `test/`.

### Manual Pipeline

```bash
# 1. Compile C source to LLVM IR
clang-15 -S -emit-llvm -O1 test/input.c -o test/input.ll

# 2. Run all obfuscation passes (O3 pipeline)
opt-15 -load-pass-plugin=build/libObfPass.so -O3 --obf-all -S test/input.ll -o test/output.ll

# 3. Compile obfuscated IR to a native binary
clang-15 test/output.ll -o test/bin/output_bin

# 4. Run it
./test/bin/output_bin
```

### Selective Passes

Run only specific passes by naming them individually:

```bash
# String obfuscation only
opt-15 -load-pass-plugin=build/libObfPass.so --obf-string -S test/input.ll -o test/output.ll

# Specific pass combination
opt-15 -load-pass-plugin=build/libObfPass.so --obf-split --obf-bcf -S test/input.ll -o test/output.ll

# Manual pipeline string (fine-grained control)
opt-15 -load-pass-plugin=build/libObfPass.so \
    -passes="stringobfuscation,function(split,bcf,constantobfuscation,substitution)" \
    -S test/input.ll -o test/output.ll
```

---

## Pass Name Reference

When using `-passes="..."` manually:

| Pass Class | Pipeline Name |
|-----------|--------------|
| `StringObfuscation` | `stringobfuscation` |
| `ConstantTaggingPass` | `constanttagger` |
| `ConstantObfuscation` | `constantobfuscation` |
| `Substitution` | `substitution` |
| `SplitBasicBlocks` | `split` |
| `BogusControlFlow` | `bcf` |

---

## Real-World Tests

Automated correctness tests against real open-source projects:

```bash
# Test against tiny-AES-c (clones repo automatically)
python3 real_world_tests/test_tiny_aes.py

# Test against SQLite 3 (downloads amalgamation automatically)
python3 real_world_tests/test_sqlite.py
```

These scripts try all flag combinations and verify that obfuscated binaries produce identical output to the baseline.

---

## Project Layout

```
obfuscator/
├── lib/
│   ├── PipelineRegistration.cpp   # Plugin entry point; registers all passes
│   ├── StringObfuscation.{h,cpp}  # String literal encryption
│   ├── ConstantObfuscation.{h,cpp}# MBA-based constant hiding
│   ├── Substitution.{h,cpp}       # Arithmetic instruction substitution
│   ├── SplitBasicBlocks.{h,cpp}   # Basic block fragmentation
│   ├── BogusControlFlow.{h,cpp}   # Opaque predicate insertion
│   └── Utils/                     # MBA engine, solver, constant analysis
├── test/
│   ├── input.c                    # Default test target
│   ├── input.ll / output.ll       # Generated IR files
│   └── bin/                       # Compiled binaries
├── real_world_tests/              # Automated integration tests
├── build/                         # CMake build artifacts (gitignored)
├── CMakeLists.txt
└── run.sh                         # Developer convenience script
```

---

## Acknowledgments

- **[LLVM Project](https://llvm.org/)** — The entire infrastructure this project is built on. LLVM 15's New Pass Manager API drives the plugin system.
- **[obfuscator-llvm / Hikari](https://github.com/HikariObfuscator/Hikari)** — Pioneering open-source LLVM obfuscator; the BCF and substitution concepts here are inspired by its design.
- **[tiny-AES-c by kokke](https://github.com/kokke/tiny-AES-c)** — Lightweight AES implementation used as a real-world obfuscation correctness test.
- **[SQLite](https://www.sqlite.org/)** — The SQLite amalgamation (3.41.2) is used as a stress test for large-scale IR obfuscation.
- **[MBA (Mixed Boolean-Arithmetic) research](https://link.springer.com/chapter/10.1007/978-3-540-77535-5_5)** — Theoretical foundation for the constant and instruction obfuscation passes.
