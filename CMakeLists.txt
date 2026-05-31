cmake_minimum_required(VERSION 3.13)
project(ObfLLVMPass)

# LLVM 15 是目前的目標版本，請根據實際安裝的 LLVM 版本調整
find_package(LLVM 15 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()
include_directories(include) # 如果以後要整理header，可以移動到include/
include_directories(lib)
include_directories(lib/Utils)
add_library(ObfPass MODULE 
    lib/PipelineRegistration.cpp  # 核心管理入口
    lib/StringObfuscation.cpp
    lib/ConstantObfuscation.cpp
    lib/Substitution.cpp
    lib/SplitBasicBlocks.cpp      # [+] 加入你的碎紙機 Pass
    lib/BogusControlFlow.cpp      # [+] 加入你的迷宮 Pass
    lib/Utils/MBAEngine.cpp
    lib/Utils/MBABuilder.cpp
    lib/Utils/MBASolver.cpp
    lib/Utils/ConstantAnalyzer.cpp
    # 未來只要在這裡橫向增加新的 Pass 檔案即可
)

# target_link_libraries(ObfPass LLVM)
#set_target_properties(ObfPass PROPERTIES
#    CXX_VISIBILITY_PRESET "hidden"
#    VISIBILITY_INLINES_HIDDEN 1
#)

if(APPLE)
    set_property(TARGET ObfPass APPEND_STRING PROPERTY LINK_FLAGS " -undefined dynamic_lookup")
endif()