#!/usr/bin/env bash
set -euo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

BUILD_DIR="../../build"
PASS_PLUGIN="$BUILD_DIR/libObfPass.so"

if [ ! -f "$PASS_PLUGIN" ]; then
    echo "[-] Error: Obfuscator pass not found at $PASS_PLUGIN"
    echo "[-] Please build the project in the root directory first."
    exit 1
fi

clean() {
    echo "[*] Cleaning up temporary IR and binary files..."
    rm -f *.ll evaluation_demo_orig evaluation_demo_obf evaluation_demo_obf_noO3
    echo "[*] Cleaning up IDA database files..."
    rm -f *.id0 *.id1 *.id2 *.nam *.til *.i64
    echo "[+] Clean complete."
}

build() {
    echo "[*] Building benchmark binaries..."
    
    # 1. Build original reference binary (-O3)
    clang-15 -O3 evaluation_demo.c -o evaluation_demo_orig
    
    # 2. Generate base LLVM IR (-O1 to preserve loops but simplify basic stuff)
    clang-15 -S -emit-llvm -Wall -O1 evaluation_demo.c -o evaluation_demo.ll
    
    # 3. Scenario A: Obfuscation with -O3 optimization (Constraint Hardness)
    opt-15 -load-pass-plugin="$PASS_PLUGIN" -O3 -obf-all -S evaluation_demo.ll -o evaluation_demo_obf.ll
    clang-15 evaluation_demo_obf.ll -o evaluation_demo_obf
    
    # 4. Scenario B: Obfuscation with -O0 optimization (State Growth / Path Explosion)
    opt-15 -load-pass-plugin="$PASS_PLUGIN" -O0 -obf-all -S evaluation_demo.ll -o evaluation_demo_obf_noO3.ll
    clang-15 -O0 evaluation_demo_obf_noO3.ll -o evaluation_demo_obf_noO3
    
    echo "[+] Build complete."
}

run_test() {
    local target=$1
    if [ ! -f "$target" ]; then
        echo "[-] Error: Target $target not found. Please run 'build' first."
        exit 1
    fi
    echo "============================================================"
    echo "[*] Running angr evaluation on: $target"
    echo "============================================================"
    if command -v uv &> /dev/null; then
        uv run --with "angr==9.2.104" --with "pycparser==2.21" python3 -u angr_test.py "$target"
    else
        echo "[*] 'uv' not found, falling back to standard python3. Ensure 'angr' is installed via requirements.txt."
        python3 -u angr_test.py "$target"
    fi
}

cmd=${1:-all}
case "$cmd" in
    clean)
        clean
        ;;
    build)
        build
        ;;
    run-orig)
        run_test evaluation_demo_orig
        ;;
    run-obf-o3)
        run_test evaluation_demo_obf
        ;;
    run-obf-o0)
        run_test evaluation_demo_obf_noO3
        ;;
    all)
        clean
        build
        echo ""
        run_test evaluation_demo_orig
        echo ""
        run_test evaluation_demo_obf
        echo ""
        run_test evaluation_demo_obf_noO3
        ;;
    *)
        echo "Usage: ./run_benchmark.sh {build|clean|run-orig|run-obf-o3|run-obf-o0|all}"
        exit 1
        ;;
esac
