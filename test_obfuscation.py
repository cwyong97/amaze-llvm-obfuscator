#!/usr/bin/env python3
import os
import sys
import subprocess
import argparse
import tempfile
import shutil

# Text colors for beautiful output
GREEN = "\033[1;32m"
RED = "\033[1;31m"
YELLOW = "\033[1;33m"
BLUE = "\033[1;34m"
CYAN = "\033[1;36m"
BOLD = "\033[1m"
RESET = "\033[0m"

# Default arithmetic expressions to verify correctness
DEFAULT_EXPRESSIONS = [
    "3 * 5 + 8 / 2",
    "(12 + 8) * (20 - 15)",
    "100 / (4 + 6) * 3",
    "45 - 6 * 7 + 12",
    "99 % 10 + 2",
    "((3 + 5) * 2 - 6) / 2"
]

# Obfuscation flag mapping
FLAG_MAP = {
    "string": ["-obf-string"],
    "const": ["-obf-const", "-obf-tag"],
    "sub": ["-obf-sub"],
    "split": ["-obf-split"],
    "bcf": ["-obf-bcf"]
}

def run_cmd(cmd, shell=False, stdin_data=None):
    """Run a shell command, capturing stdout, stderr, and exit status."""
    try:
        proc = subprocess.run(
            cmd,
            shell=shell,
            input=stdin_data,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True
        )
        return proc.stdout, proc.stderr
    except subprocess.CalledProcessError as e:
        print(f"{RED}{BOLD}[Error] Command failed:{RESET} {' '.join(cmd) if isinstance(cmd, list) else cmd}")
        print(f"{RED}Exit Code:{RESET} {e.returncode}")
        print(f"{RED}Stdout:{RESET}\n{e.stdout}")
        print(f"{RED}Stderr:{RESET}\n{e.stderr}")
        raise e

def build_obfuscator():
    """Build the ObfPass plugin."""
    print(f"{BLUE}{BOLD}[1/4] Building AmaZe Obfuscator Pass...{RESET}")
    if not os.path.exists("build"):
        os.makedirs("build")
    cwd = os.getcwd()
    try:
        os.chdir("build")
        run_cmd(["cmake", "-DLLVM_DIR=/usr/lib/llvm-15/cmake", ".."], shell=False)
        run_cmd(["make", "-j", str(os.cpu_count())])
    finally:
        os.chdir(cwd)
    print(f"{GREEN}[✔] Build successful!{RESET}\n")

def generate_base_ir():
    """Generate the baseline input.ll from input.c."""
    print(f"{BLUE}{BOLD}[2/4] Generating baseline IR (input.ll) from input.c...{RESET}")
    run_cmd(["clang-15", "-S", "-emit-llvm", "-Wall", "-O1", "test/input.c", "-o", "test/input.ll"])
    print(f"{GREEN}[✔] input.ll generated successfully!{RESET}\n")

def get_combinations(full=False):
    """Return the list of combinations to test."""
    keys = list(FLAG_MAP.keys())
    
    if full:
        # Full 2^5 = 32 combinations
        combinations = []
        for i in range(1 << len(keys)):
            comb = []
            for j in range(len(keys)):
                if (i >> j) & 1:
                    comb.append(keys[j])
            combinations.append(comb)
        return combinations
    else:
        # Curated representative combinations
        return [
            [], # Baseline
            ["string"],
            ["const"],
            ["sub"],
            ["split"],
            ["bcf"],
            ["split", "bcf"], # Control flow heavy
            ["string", "const", "sub"], # Data flow heavy
            ["string", "const", "sub", "split", "bcf"] # All combined
        ]

def get_flags_for_combination(comb):
    """Convert combination keys to actual LLVM opt flags."""
    if len(comb) == len(FLAG_MAP):
        return ["-obf-all"]
    
    flags = []
    for key in comb:
        flags.extend(FLAG_MAP[key])
    return flags

def parse_calculator_result(stdout):
    """Extract result number from output string '[=] Calculation Success! Output Result: X'"""
    for line in stdout.splitlines():
        if "Calculation Success!" in line:
            parts = line.split(":")
            if len(parts) >= 2:
                return int(parts[-1].strip())
    return None

def verify_correctness(bin_path, expressions, baseline_results):
    """Verify calculation outputs for a list of expressions against baseline results."""
    for expr, expected in zip(expressions, baseline_results):
        try:
            stdout, _ = run_cmd([bin_path], stdin_data=expr + "\n")
            result = parse_calculator_result(stdout)
            if result != expected:
                print(f"    {RED}[✗] Expression '{expr}': expected {expected}, got {result}{RESET}")
                return False
        except Exception:
            return False
    return True

def main():
    parser = argparse.ArgumentParser(description="AmaZe LLVM Obfuscator Combination Tester & Benchmarker")
    parser.add_argument("--full", action="store_true", help="Test all 32 combinations of obfuscation passes")
    parser.add_argument("--benchmark", action="store_true", help="Run hyperfine performance benchmarks")
    parser.add_argument("--expr", type=str, help="Add a custom mathematical expression to test (e.g. '(3+5)*4')")
    args = parser.parse_args()

    # Prepare expressions list
    expressions = list(DEFAULT_EXPRESSIONS)
    if args.expr:
        expressions.append(args.expr)

    # 1. Build project and base IR
    build_obfuscator()
    generate_base_ir()

    # Create temporary directories for binaries
    bin_dir = "test/bin_temp"
    if os.path.exists(bin_dir):
        shutil.rmtree(bin_dir)
    os.makedirs(bin_dir)

    # 2. Build and run Baseline
    print(f"{BLUE}{BOLD}[3/4] Establishing baseline results...{RESET}")
    baseline_bin = os.path.join(bin_dir, "baseline_bin")
    run_cmd(["clang-15", "test/input.ll", "-o", baseline_bin])
    
    baseline_results = []
    for expr in expressions:
        stdout, _ = run_cmd([baseline_bin], stdin_data=expr + "\n")
        res = parse_calculator_result(stdout)
        if res is None:
            print(f"{RED}[Error] Baseline failed to compute result for expression: '{expr}'{RESET}")
            sys.exit(1)
        baseline_results.append(res)
        print(f"  Expr: {CYAN}{expr:<25}{RESET} -> Output: {GREEN}{res}{RESET}")
    print(f"{GREEN}[✔] Baseline results established successfully!{RESET}\n")

    # 3. Test Combinations
    combinations = get_combinations(args.full)
    print(f"{BLUE}{BOLD}[4/4] Testing {len(combinations)} combinations...{RESET}")
    
    # We will store the results for comparison
    results_table = []
    
    # Track paths for hyperfine benchmarking
    benchmark_targets = [("Baseline", baseline_bin)]

    for idx, comb in enumerate(combinations, 1):
        comb_name = "+".join(comb) if comb else "None (Baseline)"
        comb_id = "_".join(comb) if comb else "baseline"
        
        # Flags
        opt_flags = get_flags_for_combination(comb)
        
        # Paths
        obf_ll = f"test/output_{comb_id}.ll"
        opt_obf_ll = f"test/opt_output_{comb_id}.ll"
        obf_bin = os.path.join(bin_dir, f"obf_{comb_id}")
        opt_obf_bin = os.path.join(bin_dir, f"obf_opt_{comb_id}")
        
        # Size variables
        ir_size = 0
        elf_size = 0
        opt_elf_size = 0
        
        # Status variables
        comp_ok = False
        correct_ok = False
        opt_correct_ok = False
        
        try:
            # 1. Run opt-15 with obfuscation flags
            # Include -O3 so OptimizerLastEPCallback is triggered
            cmd_opt = ["opt-15", "-load-pass-plugin=build/libObfPass.so"] + opt_flags + ["-O3", "-S", "test/input.ll", "-o", obf_ll]
            run_cmd(cmd_opt)
            ir_size = os.path.getsize(obf_ll)
            
            # 2. Compile obfuscated IR to ELF
            run_cmd(["clang-15", obf_ll, "-o", obf_bin])
            elf_size = os.path.getsize(obf_bin)
            comp_ok = True
            
            # 3. Verify correctness of obfuscated binary
            correct_ok = verify_correctness(obf_bin, expressions, baseline_results)
            
            # 4. Generate optimized obfuscated IR (Obf + standard O3)
            run_cmd(["opt-15", "-O3", "-S", obf_ll, "-o", opt_obf_ll])
            
            # 5. Compile optimized IR to ELF
            run_cmd(["clang-15", opt_obf_ll, "-o", opt_obf_bin])
            opt_elf_size = os.path.getsize(opt_obf_bin)
            
            # 6. Verify correctness of optimized binary
            opt_correct_ok = verify_correctness(opt_obf_bin, expressions, baseline_results)
            
            # Add to benchmarking list if correct
            if correct_ok:
                benchmark_targets.append((comb_name, obf_bin))
            if opt_correct_ok:
                benchmark_targets.append((f"{comb_name} (+O3)", opt_obf_bin))
                
        except Exception as e:
            comp_ok = False
            
        # Cleanup temporary .ll files to prevent cluttering the test directory
        for f in [obf_ll, opt_obf_ll]:
            if os.path.exists(f):
                os.remove(f)
                
        # Status summary
        status_str = ""
        if not comp_ok:
            status_str = f"{RED}FAIL (Comp){RESET}"
        elif not correct_ok:
            status_str = f"{RED}FAIL (Obf Result){RESET}"
        elif not opt_correct_ok:
            status_str = f"{RED}FAIL (O3 Result){RESET}"
        else:
            status_str = f"{GREEN}PASS{RESET}"
            
        # Bloat ratios
        baseline_ir_size = os.path.getsize("test/input.ll")
        baseline_elf_size = os.path.getsize(baseline_bin)
        
        ir_bloat = ir_size / baseline_ir_size if baseline_ir_size > 0 else 0
        elf_bloat = elf_size / baseline_elf_size if baseline_elf_size > 0 else 0
        opt_elf_bloat = opt_elf_size / baseline_elf_size if baseline_elf_size > 0 else 0
        
        print(f"  [{idx}/{len(combinations)}] Combination: {YELLOW}{comb_name:<45}{RESET} -> Status: {status_str}")
        if comp_ok and correct_ok and opt_correct_ok:
            print(f"      IR Bloat: {ir_bloat:.2f}x | ELF Bloat: {elf_bloat:.2f}x | ELF+O3 Bloat: {opt_elf_bloat:.2f}x")
            
        results_table.append({
            "name": comb_name,
            "status": "PASS" if (comp_ok and correct_ok and opt_correct_ok) else "FAIL",
            "ir_size": ir_size,
            "ir_bloat": ir_bloat,
            "elf_size": elf_size,
            "elf_bloat": elf_bloat,
            "opt_elf_size": opt_elf_size,
            "opt_elf_bloat": opt_elf_bloat
        })

    # Print beautiful markdown summary table
    print("\n" + "="*80)
    print(f"{BOLD} AmaZe LLVM Obfuscator - Verification & Size Bloat Analysis{RESET}")
    print("="*80)
    print(f"| {'Combination':<35} | {'Status':<6} | {'IR Size':<10} | {'IR Bloat':<8} | {'ELF Size':<10} | {'ELF Bloat':<9} | {'ELF+O3':<10} | {'ELF+O3 Bloat':<12} |")
    print(f"|{'-'*37}|{'-'*8}|{'-'*12}|{'-'*10}|{'-'*12}|{'-'*11}|{'-'*12}|{'-'*14}|")
    for r in results_table:
        status_color = GREEN if r["status"] == "PASS" else RED
        status_text = f"{status_color}{r['status']}{RESET}"
        
        print(f"| {r['name']:<35} | {status_text:<6} | {r['ir_size']:10,} | {r['ir_bloat']:7.2f}x | {r['elf_size']:10,} | {r['elf_bloat']:8.2f}x | {r['opt_elf_size']:10,} | {r['opt_elf_bloat']:11.2f}x |")
    print("="*80 + "\n")

    # 4. Hyperfine Benchmarking
    if args.benchmark:
        print(f"{BLUE}{BOLD}[Benchmark] Running performance benchmarks using hyperfine...{RESET}")
        
        # We will benchmark with the first math expression
        benchmark_expr = expressions[0]
        print(f"  Benchmark math expression: {CYAN}'{benchmark_expr}'{RESET}")
        
        # Build hyperfine command
        # E.g. hyperfine -N "echo '3 * 5 + 8 / 2' | ./baseline_bin" ...
        hyperfine_cmd = ["hyperfine", "--warmup", "10", "--runs", "100"]
        
        # We will benchmark the Baseline, "All Combined", and "All Combined (+O3)" for a high-impact clean report
        # To avoid making hyperfine extremely slow, we benchmark: Baseline, obf_all, and obf_opt_all
        selected_benchmarks = []
        for name, bin_path in benchmark_targets:
            if name in [
                "Baseline",
                "None (Baseline)",
                "None (Baseline) (+O3)",
                "string+const+sub+split+bcf",
                "string+const+sub+split+bcf (+O3)"
            ]:
                selected_benchmarks.append((name, bin_path))
                
        if len(selected_benchmarks) > 1:
            for name, bin_path in selected_benchmarks:
                hyperfine_cmd.append(f"echo '{benchmark_expr}' | {bin_path}")
            
            # Print command for user clarity
            print(f"  Running: {CYAN}{' '.join(hyperfine_cmd)}{RESET}\n")
            
            try:
                subprocess.run(hyperfine_cmd, check=True)
            except FileNotFoundError:
                print(f"{RED}[Error] 'hyperfine' is not installed or not found in PATH.{RESET}")
                print("Please install hyperfine to run the benchmark: 'sudo apt install hyperfine'")
        else:
            print(f"{YELLOW}[Warning] Not enough successful targets to benchmark.{RESET}")

    # Cleanup temporary bin dir
    shutil.rmtree(bin_dir)
    print(f"\n{GREEN}{BOLD}[✔] All done! Verification and analyses complete.{RESET}")

if __name__ == "__main__":
    main()
