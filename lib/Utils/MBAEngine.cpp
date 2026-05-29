#include "MBAEngine.h"
#include "MBABasis.h"
#include "MBASolver.h"
#include "MBABuilder.h"

#include <algorithm>
#include <numeric>

std::vector<std::vector<int64_t>> MBAEngine::computeBasisMatrix() {
    std::vector<std::vector<int64_t>> A(4, std::vector<int64_t>(basis_pool.size()));
    for (int i = 0; i < 4; ++i) {
        int64_t vx = (i >> 1) & 1;
        int64_t vy = i & 1;
        for (size_t j = 0; j < basis_pool.size(); ++j) {
            A[i][j] = static_cast<int64_t>(basis_pool[j].op(vx, vy));
        }
    }
    return A;
}

Value* MBAEngine::obfuscate(
    IRBuilder<>& builder,
    Value* x, Value* y,
    const std::vector<int64_t>& truthTable,
    const std::vector<std::vector<int64_t>>& A,
    std::mt19937& gen,
    int intensity)
{
    size_t cols = basis_pool.size();

    // 1. 隨機打亂 column 排列，讓每次求解選到不同的 pivot
    std::vector<size_t> perm(cols);
    std::iota(perm.begin(), perm.end(), 0);
    std::shuffle(perm.begin(), perm.end(), gen);

    // 2. 按 perm 打亂 A 的 column
    std::vector<std::vector<int64_t>> A_shuffled(4, std::vector<int64_t>(cols));
    for (int row = 0; row < 4; ++row)
        for (size_t col = 0; col < cols; ++col)
            A_shuffled[row][col] = A[row][perm[col]];

    // 3. 求解線性系統 A_shuffled * x = truthTable (mod 2^64)
    std::vector<int64_t> x_sol = MBASolver::solve(A_shuffled, truthTable, gen, intensity);
    if (x_sol.empty()) return nullptr;

    // 4. 生成對應的 LLVM IR 表達式樹
    return MBABuilder::buildLinearMBA(builder, x, y, x_sol, perm);
}
