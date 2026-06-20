# AmazeLLVM Reproducible Evaluation Framework

This directory contains the scripts and assets required to reproduce the security evaluation metrics described in the main documentation. The framework is split into two primary components to isolate the effects of the obfuscator on different layers of automated analysis.

## Environment Setup

To ensure reproducible results, we provide a `requirements.txt` file for the Python dependencies used in the evaluation. *(Note: If you have `uv` installed, you can skip this step as the scripts will automatically fetch the exact dependencies.)*

```bash
cd evaluation/
pip install -r requirements.txt
```

## 1. `angr/` - Symbolic Execution Benchmark
Tests the obfuscated binary against the `angr` symbolic execution engine. 
This test measures the combined effect of Bogus Control Flow (BCF) and Mixed Boolean-Arithmetic (MBA) under different compiler optimization levels (`-O0` vs `-O3`).

**Usage:**
We provide an automated script to build all required configurations and run the benchmark suite.

```bash
cd evaluation/angr/
./run_benchmark.sh all
```

**Commands:**
- `./run_benchmark.sh build`: Compiles `evaluation_demo.c` into original, O3 obfuscated, and O0 obfuscated binaries.
- `./run_benchmark.sh clean`: Removes all generated IR and binary files.
- `./run_benchmark.sh run-orig`: Tests the original clean binary.
- `./run_benchmark.sh run-obf-o3`: Tests Scenario A (Constraint Hardness).
- `./run_benchmark.sh run-obf-o0`: Tests Scenario B (State Growth).

## 2. `z3/` - SMT Solver Benchmark
Directly tests the mathematical hardness of the generated Mixed Boolean-Arithmetic (MBA) formulas against `z3`, the underlying SMT solver used by most symbolic engines. By isolating the math from the control flow, this benchmark quantifies the pure constraint-solving cost of the MBA transformations at different bit-widths.

**Usage:**

Using standard Python (ensure dependencies from `requirements.txt` are installed):
```bash
cd z3/
python3 mba_z3_benchmark.py
```

Or using `uv` (will automatically fetch `z3-solver`):
```bash
cd z3/
uv run --with "z3-solver==4.13.0.0" python3 mba_z3_benchmark.py
```

*Outputs the formula simplification and the time required to prove equivalence (`unsat`).*
