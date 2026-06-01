#!/usr/bin/env python3
import os
import sys
import subprocess
import argparse
import shutil

# Text colors for beautiful output
GREEN = "\033[1;32m"
RED = "\033[1;31m"
YELLOW = "\033[1;33m"
BLUE = "\033[1;34m"
CYAN = "\033[1;36m"
BOLD = "\033[1m"
RESET = "\033[0m"

REPO_URL = "https://github.com/kokke/tiny-AES-c.git"
TESTS_DIR = os.path.abspath("real_world_tests")
AES_DIR = os.path.abspath(os.path.join(TESTS_DIR, "tiny-AES-c"))

FLAG_MAP = {
    "string": ["-obf-string"],
    "const": ["-obf-const", "-obf-tag"],
    "sub": ["-obf-sub"],
    "split": ["-obf-split"],
    "bcf": ["-obf-bcf"]
}

def get_combinations(full=False):
    keys = list(FLAG_MAP.keys())
    if full:
        combinations = []
        for i in range(1 << len(keys)):
            comb = []
            for j in range(len(keys)):
                if (i >> j) & 1:
                    comb.append(keys[j])
            combinations.append(comb)
        return combinations
    else:
        return [
            [], # Baseline
            ["sub"],
            ["split", "bcf"],
            ["string", "const", "sub"],
            ["string", "const", "sub", "split", "bcf"]
        ]

def get_flags_for_combination(comb):
    if len(comb) == len(FLAG_MAP):
        return ["-obf-all"]
    flags = []
    for key in comb:
        flags.extend(FLAG_MAP[key])
    return flags

def run_cmd(cmd, shell=False, cwd=None):
    """Run a shell command, capturing stdout, stderr, and exit status."""
    try:
        proc = subprocess.run(
            cmd,
            shell=shell,
            cwd=cwd,
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

def clone_repo():
    """Clone the tiny-AES-c repository if it does not exist."""
    if not os.path.exists(AES_DIR):
        print(f"{BLUE}[1/4] Cloning tiny-AES-c repository...{RESET}")
        run_cmd(["git", "clone", REPO_URL, AES_DIR])
        print(f"{GREEN}[✔] Cloned successfully!{RESET}\n")
    else:
        print(f"{GREEN}[✔] tiny-AES-c repository already cloned.{RESET}\n")

def main():
    parser = argparse.ArgumentParser(description="AmaZe LLVM Obfuscator - tiny-AES-c Verification & Benchmarking")
    parser.add_argument("--full", action="store_true", help="Test all 32 combinations of obfuscation passes")
    parser.add_argument("--benchmark", action="store_true", help="Run hyperfine performance benchmarks")
    args = parser.parse_args()

    # 1. Setup
    if not os.path.exists(TESTS_DIR):
        os.makedirs(TESTS_DIR)
    clone_repo()

    # Create temporary binary directory
    bin_temp = os.path.abspath(os.path.join(TESTS_DIR, "bin_aes_temp"))
    if os.path.exists(bin_temp):
        shutil.rmtree(bin_temp)
    os.makedirs(bin_temp)

    # 2. Compile baseline IR from aes.c
    print(f"{BLUE}[2/4] Generating LLVM IR for aes.c...{RESET}")
    # Compiling aes.c to LLVM IR
    run_cmd(["clang-15", "-S", "-emit-llvm", "-O1", "aes.c", "-o", "aes.ll"], cwd=AES_DIR)
    print(f"{GREEN}[✔] LLVM IR (aes.ll) generated successfully!{RESET}\n")

    # 3. Establish Baseline Binary
    print(f"{BLUE}[3/4] Establishing baseline tiny-AES-c...{RESET}")
    baseline_bin = os.path.join(bin_temp, "baseline_bin")
    run_cmd(["clang-15", "aes.c", "test.c", "-o", baseline_bin], cwd=AES_DIR)
    
    # Run baseline to ensure it works
    stdout, _ = run_cmd([baseline_bin])
    print(f"{GREEN}[✔] Baseline compiled & executed successfully!{RESET}")
    print(f"{CYAN}Baseline Output Snippet:{RESET}\n{stdout.strip()}\n")

    # 4. Compile Obfuscated Configurations
    combinations = get_combinations(args.full)
    print(f"{BLUE}[4/4] Obfuscating & Verifying tiny-AES-c ({len(combinations)} combinations)...{RESET}")
    
    results_table = []
    benchmark_cmds = []

    # Get absolute path to plugin
    plugin_path = os.path.abspath("build/libObfPass.so")

    for idx, comb in enumerate(combinations, 1):
        name = "+".join(comb) if comb else "None (Baseline)"
        flags = get_flags_for_combination(comb)
        
        comb_id = "_".join(comb) if comb else "baseline"
        
        aes_obf_ll = os.path.join(AES_DIR, f"aes_{comb_id}.ll")
        aes_opt_obf_ll = os.path.join(AES_DIR, f"aes_opt_{comb_id}.ll")
        obf_bin = os.path.join(bin_temp, f"obf_{comb_id}")
        opt_obf_bin = os.path.join(bin_temp, f"obf_opt_{comb_id}")
        
        ir_size = 0
        elf_size = 0
        opt_elf_size = 0
        comp_ok = False
        correct_ok = False
        opt_correct_ok = False

        try:
            # A. Run opt-15 with obfuscation flags (need -O3 to trigger extension callback)
            cmd_opt = ["opt-15", "-load-pass-plugin=" + plugin_path] + flags + ["-O3", "-S", "aes.ll", "-o", aes_obf_ll]
            run_cmd(cmd_opt, cwd=AES_DIR)
            ir_size = os.path.getsize(aes_obf_ll)

            # B. Compile to ELF
            run_cmd(["clang-15", aes_obf_ll, "test.c", "-o", obf_bin], cwd=AES_DIR)
            elf_size = os.path.getsize(obf_bin)
            comp_ok = True

            # C. Verify correctness
            stdout_obf, _ = run_cmd([obf_bin])
            # If it runs to completion (exit code 0), correctness passes
            correct_ok = True

            # D. Compile optimized obfuscated binary (Obf + standard O3)
            run_cmd(["opt-15", "-O3", "-S", aes_obf_ll, "-o", aes_opt_obf_ll], cwd=AES_DIR)
            run_cmd(["clang-15", aes_opt_obf_ll, "test.c", "-o", opt_obf_bin], cwd=AES_DIR)
            opt_elf_size = os.path.getsize(opt_obf_bin)

            stdout_opt, _ = run_cmd([opt_obf_bin])
            opt_correct_ok = True

            if correct_ok:
                benchmark_cmds.append((name, obf_bin))
            if opt_correct_ok:
                benchmark_cmds.append((f"{name} (+O3)", opt_obf_bin))

        except Exception as e:
            comp_ok = False

        # Cleanup intermediate .ll files
        for f in [aes_obf_ll, aes_opt_obf_ll]:
            if os.path.exists(f):
                os.remove(f)

        status_str = f"{GREEN}PASS{RESET}" if (comp_ok and correct_ok and opt_correct_ok) else f"{RED}FAIL{RESET}"
        print(f"  Combination: {YELLOW}{name:<30}{RESET} -> Status: {status_str}")

        results_table.append({
            "name": name,
            "status": "PASS" if (comp_ok and correct_ok and opt_correct_ok) else "FAIL",
            "ir_size": ir_size,
            "elf_size": elf_size,
            "opt_elf_size": opt_elf_size
        })

    # Print beautiful table
    print("\n" + "="*80)
    print(f"{BOLD} AmaZe LLVM Obfuscator - tiny-AES-c Verification & Size Bloat{RESET}")
    print("="*80)
    print(f"| {'Combination':<28} | {'Status':<6} | {'IR Size':<10} | {'ELF Size':<10} | {'ELF+O3 Size':<12} |")
    print(f"|{'-'*30}|{'-'*8}|{'-'*12}|{'-'*12}|{'-'*14}|")
    for r in results_table:
        status_color = GREEN if r["status"] == "PASS" else RED
        status_text = f"{status_color}{r['status']}{RESET}"
        print(f"| {r['name']:<28} | {status_text:<6} | {r['ir_size']:10,} | {r['elf_size']:10,} | {r['opt_elf_size']:12,} |")
    print("="*80 + "\n")

    # Hyperfine speed benchmarking
    if args.benchmark and len(benchmark_cmds) > 1:
        print(f"{BLUE}{BOLD}[Benchmark] Running performance benchmarks using hyperfine...{RESET}")
        
        # We benchmark Baseline, "All combined (Five-in-One)", and "All combined (Five-in-One) (+O3)"
        hyperfine_cmd = ["hyperfine", "--warmup", "20", "--runs", "100"]
        
        selected_benchmarks = []
        for name, bin_path in benchmark_cmds:
            if name in [
                "Baseline",
                "None (Baseline)",
                "string+const+sub+split+bcf",
                "string+const+sub+split+bcf (+O3)"
            ]:
                selected_benchmarks.append(bin_path)

        if len(selected_benchmarks) > 1:
            hyperfine_cmd.extend(selected_benchmarks)
            print(f"  Running: {CYAN}{' '.join(hyperfine_cmd)}{RESET}\n")
            try:
                subprocess.run(hyperfine_cmd, check=True)
            except FileNotFoundError:
                print(f"{RED}[Error] 'hyperfine' is not installed or not found in PATH.{RESET}")
        else:
            print(f"{YELLOW}[Warning] Not enough successful targets to benchmark.{RESET}")

    # Cleanup temporary .ll and .ll_obf
    for f in ["aes.ll"]:
        p = os.path.join(AES_DIR, f)
        if os.path.exists(p):
            os.remove(p)

    # Keep binaries for potential manual inspection or hyperfine
    shutil.rmtree(bin_temp)
    print(f"{GREEN}{BOLD}[✔] tiny-AES-c testing completed successfully.{RESET}")

if __name__ == "__main__":
    main()
