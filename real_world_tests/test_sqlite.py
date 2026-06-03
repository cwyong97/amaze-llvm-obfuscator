#!/usr/bin/env python3
import os
import sys
import subprocess
import argparse
import shutil
import urllib.request
import zipfile
import io

# Text colors for beautiful output
GREEN = "\033[1;32m"
RED = "\033[1;31m"
YELLOW = "\033[1;33m"
BLUE = "\033[1;34m"
CYAN = "\033[1;36m"
BOLD = "\033[1m"
RESET = "\033[0m"

# Official SQLite Amalgamation Download URL (3.41.2 Stable Release)
SQLITE_URL = "https://www.sqlite.org/2023/sqlite-amalgamation-3410200.zip"
TESTS_DIR = os.path.abspath("real_world_tests")
SQLITE_DIR = os.path.abspath(os.path.join(TESTS_DIR, "sqlite-amalgamation"))

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
            ["string", "const", "sub"], # Data Obf
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

def download_sqlite():
    """Download and extract official SQLite amalgamation."""
    if not os.path.exists(SQLITE_DIR):
        print(f"{BLUE}[1/4] Downloading official SQLite amalgamation...{RESET}")
        req = urllib.request.Request(
            SQLITE_URL,
            headers={'User-Agent': 'Mozilla/5.0'}
        )
        with urllib.request.urlopen(req) as response:
            zip_data = response.read()
        
        with zipfile.ZipFile(io.BytesIO(zip_data)) as zip_ref:
            # The zip contains a directory like 'sqlite-amalgamation-3410200'
            # We will extract it and rename it to 'sqlite-amalgamation'
            zip_ref.extractall(TESTS_DIR)
            
            # Find the extracted folder
            extracted_folder = None
            for name in os.listdir(TESTS_DIR):
                if name.startswith("sqlite-amalgamation-") and os.path.isdir(os.path.join(TESTS_DIR, name)):
                    extracted_folder = os.path.join(TESTS_DIR, name)
                    break
            
            if extracted_folder:
                os.rename(extracted_folder, SQLITE_DIR)
        print(f"{GREEN}[✔] SQLite downloaded and extracted to {SQLITE_DIR}!{RESET}\n")
    else:
        print(f"{GREEN}[✔] SQLite amalgamation already downloaded.{RESET}\n")

def main():
    parser = argparse.ArgumentParser(description="AmaZe LLVM Obfuscator - SQLite3 Verification & Benchmarking")
    parser.add_argument("--full", action="store_true", help="Test all 32 combinations of obfuscation passes")
    parser.add_argument("--benchmark", action="store_true", help="Run hyperfine performance benchmarks")
    args = parser.parse_args()

    # 1. Setup
    if not os.path.exists(TESTS_DIR):
        os.makedirs(TESTS_DIR)
    download_sqlite()

    # Create temporary binary directory
    bin_temp = os.path.abspath(os.path.join(TESTS_DIR, "bin_sqlite_temp"))
    if os.path.exists(bin_temp):
        shutil.rmtree(bin_temp)
    os.makedirs(bin_temp)

    # Copy sqlite_test.c into the sqlite amalgamation directory for easy building
    shutil.copy("real_world_tests/sqlite_test.c", os.path.join(SQLITE_DIR, "sqlite_test.c"))

    # 2. Compile baseline IR from sqlite3.c
    # SQLite compiles with -pthread and -ldl. We also pass -DSQLITE_THREADSAFE=0 to simplify testing
    print(f"{BLUE}[2/4] Generating LLVM IR for sqlite3.c (this may take a few seconds)...{RESET}")
    run_cmd([
        "clang-15", "-S", "-emit-llvm", "-O1", 
        "-DSQLITE_THREADSAFE=0", "-DSQLITE_OMIT_LOAD_EXTENSION", 
        "sqlite3.c", "-o", "sqlite3.ll"
    ], cwd=SQLITE_DIR)
    print(f"{GREEN}[✔] LLVM IR (sqlite3.ll) generated successfully!{RESET}\n")

    # 3. Establish Baseline Binary
    print(f"{BLUE}[3/4] Establishing baseline SQLite3 binary...{RESET}")
    baseline_bin = os.path.join(bin_temp, "baseline_bin")
    run_cmd([
        "clang-15", "-O1", "-pthread", "-ldl", 
        "-DSQLITE_THREADSAFE=0", "-DSQLITE_OMIT_LOAD_EXTENSION", 
        "sqlite3.c", "sqlite_test.c", "-o", baseline_bin
    ], cwd=SQLITE_DIR)
    
    # Run baseline to ensure it works
    stdout, _ = run_cmd([baseline_bin])
    print(f"{GREEN}[✔] Baseline compiled & executed successfully!{RESET}")
    print(f"{CYAN}Baseline Output Snippet:{RESET}\n{stdout.strip()}\n")

    # 4. Compile Obfuscated Configurations
    combinations = get_combinations(args.full)
    if args.full:
        print(f"{RED}{BOLD}[Warning] Running all 32 combinations on SQLite3 (240k+ lines) will take a very long time!{RESET}")
    print(f"{BLUE}[4/4] Obfuscating & Verifying SQLite3 ({len(combinations)} combinations)...{RESET}")
    
    results_table = []
    benchmark_cmds = []

    # Get absolute path to plugin
    plugin_path = os.path.abspath("build/libObfPass.so")

    for idx, comb in enumerate(combinations, 1):
        name = "+".join(comb) if comb else "None (Baseline)"
        flags = get_flags_for_combination(comb)
        
        comb_id = "_".join(comb) if comb else "baseline"
        
        sqlite_obf_ll = os.path.join(SQLITE_DIR, f"sqlite3_{comb_id}.ll")
        sqlite_opt_obf_ll = os.path.join(SQLITE_DIR, f"sqlite3_opt_{comb_id}.ll")
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
            if flags:
                print(f"  Obfuscating with {YELLOW}{name}{RESET} (this takes ~10-15 seconds)...")
                cmd_opt = ["opt-15", "-load-pass-plugin=" + plugin_path] + flags + ["-O3", "-S", "sqlite3.ll", "-o", sqlite_obf_ll]
                run_cmd(cmd_opt, cwd=SQLITE_DIR)
            else:
                # For baseline, just copy the un-obfuscated IR
                shutil.copy(os.path.join(SQLITE_DIR, "sqlite3.ll"), sqlite_obf_ll)

            ir_size = os.path.getsize(sqlite_obf_ll)

            # B. Compile to ELF
            print(f"  Compiling {name} to ELF...")
            run_cmd(["clang-15", "-pthread", "-ldl", sqlite_obf_ll, "sqlite_test.c", "-o", obf_bin], cwd=SQLITE_DIR)
            elf_size = os.path.getsize(obf_bin)
            comp_ok = True

            # C. Verify correctness
            stdout_obf, _ = run_cmd([obf_bin])
            if stdout_obf.strip() == stdout.strip():
                correct_ok = True
                benchmark_cmds.append((name, obf_bin))
            else:
                print(f"{RED}  [Warning] {name} output mismatch!{RESET}")

            # D. Compile optimized obfuscated binary (Obf + standard O3)
            print(f"  Optimizing and compiling {name} (+O3) to ELF...")
            run_cmd(["opt-15", "-O3", "-S", sqlite_obf_ll, "-o", sqlite_opt_obf_ll], cwd=SQLITE_DIR)
            run_cmd(["clang-15", "-pthread", "-ldl", sqlite_opt_obf_ll, "sqlite_test.c", "-o", opt_obf_bin], cwd=SQLITE_DIR)
            opt_elf_size = os.path.getsize(opt_obf_bin)

            stdout_opt, _ = run_cmd([opt_obf_bin])
            if stdout_opt.strip() == stdout.strip():
                opt_correct_ok = True
                benchmark_cmds.append((f"{name} (+O3)", opt_obf_bin))
            else:
                print(f"{RED}  [Warning] {name} (+O3) output mismatch!{RESET}")

        except Exception as e:
            # 強制將受污染的編譯狀態歸零，避免殘留數據寫入 Table
            comp_ok, correct_ok, opt_correct_ok = False, False, False
            print(f"{RED}Error compiling/verifying combination {name}: {e}{RESET}")

        # Cleanup intermediate .ll files
        for f in [sqlite_obf_ll, sqlite_opt_obf_ll]:
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
    print(f"{BOLD} AmaZe LLVM Obfuscator - SQLite3 Verification & Size Bloat{RESET}")
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
        
        hyperfine_cmd = ["hyperfine", "--warmup", "10", "--runs", "50"]
        selected_benchmarks = [bin_path for _, bin_path in benchmark_cmds]
        
        hyperfine_cmd.extend(selected_benchmarks)
        print(f"  Running: {CYAN}{' '.join(hyperfine_cmd)}{RESET}\n")
        try:
            subprocess.run(hyperfine_cmd, check=True)
        except FileNotFoundError:
            print(f"{RED}[Error] 'hyperfine' is not installed or not found in PATH.{RESET}")

    # Cleanup temporary files
    for f in ["sqlite3.ll", "sqlite_test.c"]:
        p = os.path.join(SQLITE_DIR, f)
        if os.path.exists(p):
            os.remove(p)

    # Cleanup temp directory
    shutil.rmtree(bin_temp)
    print(f"{GREEN}{BOLD}[✔] SQLite3 testing completed successfully.{RESET}")

if __name__ == "__main__":
    main()
