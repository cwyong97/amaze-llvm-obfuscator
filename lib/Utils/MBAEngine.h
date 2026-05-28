#pragma once

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"
#include <vector>
#include <cstdint>
#include <random>

using namespace llvm;

class MBAEngine {
public:

    static std::vector<std::vector<int64_t>> computeBasisMatrix();

    static Value* obfuscate(
        IRBuilder<>& builder,
        Value* x, Value* y,
        const std::vector<int64_t>& truthTable,
        const std::vector<std::vector<int64_t>>& A,
        std::mt19937& gen,
        int intensity
    );
};
