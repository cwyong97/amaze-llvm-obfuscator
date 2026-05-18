#pragma once

#include <vector>
#include <cstdint>
#include <random>

namespace MBASolver {
    // 傳入矩陣 A, 向量 b, 以及亂數產生器 gen
    // 回傳一組長度與 A 的行數 (變數數量) 相同的係數解 x
    // 如果無解，則回傳空的 vector
    std::vector<int64_t> solve(
        const std::vector<std::vector<int64_t>>& A, 
        const std::vector<int64_t>& b, 
        std::mt19937& gen
    );
}