#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<EOF
Usage: $0 [command]

Commands:
  run       Build if needed and generate output.ll (default)
  exec      Build and execute the pass with lli-15 (JIT)
  elf       Build, compile output.ll to native ELF binary, and execute
  diff      Show instruction-level diff (Input vs Obfuscated)
  stats     Display compilation, instruction, and size bloat statistics
  build     Configure and build the project
  clean     Clean the build directory
  reset     Clean and rebuild from scratch
  help      Show this usage message
EOF
}

build_project() {
  mkdir -p build
  pushd build > /dev/null
  # 強制指定 LLVM 15 的 CMake 設定檔路徑
  cmake -DLLVM_DIR=/usr/lib/llvm-15/cmake ..
  popd > /dev/null
}
clean_build() {
  rm -rf build
}

reset_project() {
  clean_build
  build_project
}

run_pass() {
  if [[ ! -d build ]]; then
    echo "build/ not found. Configuring project..."
    build_project
  fi

  pushd build > /dev/null
  make -j"$(nproc)"
  popd > /dev/null

  clang-15 -S -emit-llvm -Wall -O1 test/input.c -o test/input.ll
  
  # 1. 產生經過 AmaZe Pass 混淆後的 LLVM IR
  # opt-15 -load-pass-plugin=build/libObfPass.so -passes="stringobfuscation,constanttagger,function(split,bcf,constantobfuscation,substitution)" -S test/input.ll -o test/output.ll
  # opt-15 -load-pass-plugin=build/libObfPass.so -passes="function(split,bcf,constantobfuscation)" -S test/input.ll -o test/output.ll
  opt-15 -load-pass-plugin=build/libObfPass.so -O3 -obf-all -S test/input.ll -o test/output.ll
  
  # 2. 針對混淆後的 output.ll 再進行標準的 opt-15 -O3 優化，驗證抗摺疊與編譯正確性
  opt-15 -O3 -S test/output.ll -o test/opt_output.ll
}

exec_pass() {
  run_pass
  mkdir -p test/bin
  clang-15 test/output.ll -o test/bin/exec_output
  ./test/bin/exec_output
}

exec_elf() {
  run_pass
  mkdir -p test/bin
  
  clang-15 test/output.ll -o test/bin/output_bin
  clang-15 -Os -g -o test/bin/opt_Os_output_bin test/output.ll
  clang-15 -O3 -g -o test/bin/opt_O3_output_bin test/opt_output.ll
  clang-15 -O3 -g -o test/bin/baseline_bin test/input.ll

  echo "========================================="
  echo "Running Obfuscated ELF Binary (output_bin):"
  echo "========================================="
  ./test/bin/output_bin
  
  echo ""
  echo "========================================="
  echo "Running Standard O3 ELF Binary (opt_O3_output_bin):"
  echo "========================================="
  ./test/bin/opt_O3_output_bin
}

diff_ir() {
  run_pass
  echo "========================================="
  echo "IR Instruction Diff (Input vs Obfuscated):"
  echo "========================================="
  # Compare instructions, filtering out comments, metadata, and labels for a precise diff of replacements
  diff -u <(grep -E '^[[:space:]]+[a-z]+' test/input.ll) <(grep -E '^[[:space:]]+[a-z]+' test/output.ll) || true
}

print_stats() {
  run_pass > /dev/null 2>&1
  exec_elf > /dev/null 2>&1 || true
  echo "========================================="
  echo "AmaZe Obfuscator Compiler Statistics:"
  echo "========================================="
  local inst_in
  local inst_out
  inst_in=$(grep -E -c '^[[:space:]]+[a-z]+' test/input.ll || echo 1)
  inst_out=$(grep -E -c '^[[:space:]]+[a-z]+' test/output.ll || echo 1)
  echo "Input IR Instruction Count:  $inst_in"
  echo "Output IR Instruction Count: $inst_out"
  echo "Instruction Bloat Ratio:     $(echo "scale=2; $inst_out / $inst_in" | bc)x"
  
  if [[ -f test/bin/baseline_bin && -f test/bin/output_bin ]]; then
    local size_in
    local size_out
    size_in=$(stat -c%s test/bin/baseline_bin)
    size_out=$(stat -c%s test/bin/output_bin)
    echo "Baseline Binary Size:        $size_in bytes"
    echo "Obfuscated Binary Size:      $size_out bytes"
    echo "Binary Size Bloat Ratio:     $(echo "scale=2; $size_out / $size_in" | bc)x"
  fi
}

cmd=${1:-run}
case "$cmd" in
  run)
    run_pass
    ;;
  exec)
    exec_pass
    ;;
  elf)
    exec_elf
    ;;
  diff)
    diff_ir
    ;;
  stats)
    print_stats
    ;;
  build)
    build_project
    ;;
  clean)
    clean_build
    ;;
  reset)
    reset_project
    ;;
  help|-h|--help)
    usage
    ;;
  *)
    echo "Unknown command: $cmd"
    usage
    exit 1
    ;;
esac 