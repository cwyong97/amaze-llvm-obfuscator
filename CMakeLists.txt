cmake_minimum_required(VERSION 3.13)
project(ObfLLVMPass)

find_package(LLVM REQUIRED CONFIG)
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

add_library(ObfPass MODULE 
    Pass/Substitution.cpp
    Pass/MBASolver.cpp
)

# target_link_libraries(ObfPass LLVM)
#set_target_properties(ObfPass PROPERTIES
#    CXX_VISIBILITY_PRESET "hidden"
#    VISIBILITY_INLINES_HIDDEN 1
#)

if(APPLE)
    set_property(TARGET ObfPass APPEND_STRING PROPERTY LINK_FLAGS " -undefined dynamic_lookup")
endif()