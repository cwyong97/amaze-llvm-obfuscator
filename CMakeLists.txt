cmake_minimum_required(VERSION 3.16)
# 專案名稱更新
project(HelloLLVM)

find_package(LLVM 15 REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

set(CMAKE_CXX_STANDARD 17)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

# 告訴 LLVM 把 HelloLLVM.cpp 編譯成 HelloLLVM.so
#add_llvm_pass_plugin(Substitution substitution.cpp)
#add_llvm_pass_plugin(SplitBasicBlocks split.cpp)
