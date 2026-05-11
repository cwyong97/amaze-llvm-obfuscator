#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<EOF
Usage: $0 [command]

Commands:
  run       Build if needed and generate output.ll (default)
  exec      Build and execute the pass with lli-15
  build     Configure and build the project
  clean     Clean the build directory
  reset     Clean and rebuild from scratch
  help      Show this usage message
EOF
}

build_project() {
  mkdir -p build
  pushd build > /dev/null
  cmake ..
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

  clang-15 -S -emit-llvm -O1 Test/input.c -o Test/input.ll
  opt-15 -load-pass-plugin=build/libObfPass.so -passes="substitution" -S Test/input.ll -o Test/output.ll
}

exec_pass() {
  run_pass
  lli-15 Test/output.ll
}

cmd=${1:-run}
case "$cmd" in
  run)
    run_pass
    ;;
  exec)
    exec_pass
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