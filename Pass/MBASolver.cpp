#include "MBASolver.h"
#include <algorithm>
#include <cstdint>
#include <iostream>

namespace MBASolver {

// 在 uint64_t 快速求奇數反元素的黑魔法 (Newton-Raphson Method)
uint64_t modInverse_odd(uint64_t c) {
    // c 必須是奇數！
    uint64_t inv = c; 
    inv *= 2 - c * inv; // 4-bit precision
    inv *= 2 - c * inv; // 8-bit precision
    inv *= 2 - c * inv; // 16-bit precision
    inv *= 2 - c * inv; // 32-bit precision
    inv *= 2 - c * inv; // 64-bit precision
    return inv;
}

std::vector<int64_t> solve(
    const std::vector<std::vector<int64_t>>& A, 
    const std::vector<int64_t>& b, 
    std::mt19937& gen) 
{
    if (A.empty() || A[0].empty()) return {};
    
    int rows = A.size();
    int cols = A[0].size();
    
    // 初始化 D = A
    std::vector<std::vector<int64_t>> D = A;
    
    // 初始化 S (rows x rows) 為單位矩陣
    std::vector<std::vector<int64_t>> S(rows, std::vector<int64_t>(rows, 0));
    for (int i = 0; i < rows; ++i) S[i][i] = 1;
    
    // 初始化 T (cols x cols) 為單位矩陣
    std::vector<std::vector<int64_t>> T(cols, std::vector<int64_t>(cols, 0));
    for (int i = 0; i < cols; ++i) T[i][i] = 1;
    
    // 輔助函式：安全減法 R1 = R1 - q * R2
    auto sub_row = [](std::vector<int64_t>& r1, const std::vector<int64_t>& r2, int64_t q) {
        for (size_t i = 0; i < r1.size(); ++i) {
            r1[i] = static_cast<int64_t>(static_cast<uint64_t>(r1[i]) - static_cast<uint64_t>(q) * static_cast<uint64_t>(r2[i]));
        }
    };
    
    // 輔助函式：安全減法 C1 = C1 - q * C2
    auto sub_col = [](std::vector<std::vector<int64_t>>& M, int c1, int c2, int64_t q) {
        for (size_t i = 0; i < M.size(); ++i) {
            M[i][c1] = static_cast<int64_t>(static_cast<uint64_t>(M[i][c1]) - static_cast<uint64_t>(q) * static_cast<uint64_t>(M[i][c2]));
        }
    };
    
    // D = SAT 分解 (類似 Smith Normal Form 的運算)
    for (int i = 0; i < std::min(rows, cols); ++i) {
        // 1. 尋找 Pivot，確保 D[i][i] != 0
        if (D[i][i] == 0) {
            bool found = false;
            for (int r = i; r < rows && !found; ++r) {
                for (int c = i; c < cols && !found; ++c) {
                    if (D[r][c] != 0) {
                        std::swap(D[i], D[r]);
                        std::swap(S[i], S[r]);
                        for (int k = 0; k < rows; ++k) std::swap(D[k][i], D[k][c]);
                        for (int k = 0; k < cols; ++k) std::swap(T[k][i], T[k][c]);
                        found = true;
                    }
                }
            }
            if (!found) continue; // 剩下的全為 0
        }
        
        // 2. 透過行列運算把非對角線元素消成 0
        bool changed;
        int loop_count = 0;
        do {
            loop_count++;
            if (loop_count > 100) {
                std::cout << "Infinite loop detected at i=" << i << "! Breaking out.\n";
                break;
            }
            changed = false;
            
            // 列運算消除同行的其他元素 (D[j][i], j > i)
            for (int j = i + 1; j < rows; ++j) {
                while (D[j][i] != 0) {
                    if (D[i][i] == 0) {
                        std::swap(D[i], D[j]);
                        std::swap(S[i], S[j]);
                    } else {
                        int64_t q = D[j][i] / D[i][i];
                        sub_row(D[j], D[i], q);
                        sub_row(S[j], S[i], q);
                        if (D[j][i] != 0) {
                            std::swap(D[i], D[j]);
                            std::swap(S[i], S[j]);
                        }
                    }
                    changed = true;
                }
            }
            
            // 行運算消除同列的其他元素 (D[i][j], j > i)
            for (int j = i + 1; j < cols; ++j) {
                while (D[i][j] != 0) {
                    if (D[i][i] == 0) {
                        for (int k = 0; k < rows; ++k) std::swap(D[k][i], D[k][j]);
                        for (int k = 0; k < cols; ++k) std::swap(T[k][i], T[k][j]);
                    } else {
                        int64_t q = D[i][j] / D[i][i];
                        sub_col(D, j, i, q);
                        sub_col(T, j, i, q);
                        if (D[i][j] != 0) {
                            for (int k = 0; k < rows; ++k) std::swap(D[k][i], D[k][j]);
                            for (int k = 0; k < cols; ++k) std::swap(T[k][i], T[k][j]);
                        }
                    }
                    changed = true;
                }
            }
        } while (changed);
    }
    
    // 計算 b' = S * b
    std::vector<int64_t> b_prime(rows, 0);
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < rows; ++j) {
            b_prime[i] = static_cast<int64_t>(static_cast<uint64_t>(b_prime[i]) + static_cast<uint64_t>(S[i][j]) * static_cast<uint64_t>(b[j]));
        }
    }
    
    // 初始化 x' 向量
    std::vector<int64_t> x_prime(cols, 0);
    
    // 求解 d_i * x'_i = b'_i mod 2^64
    for (int i = 0; i < std::min(rows, cols); ++i) {
        int64_t d = D[i][i];
        int64_t bp = b_prime[i];
        
        if (d == 0) {
            if (bp != 0) return {}; // 無解
            else {
                std::uniform_int_distribution<int64_t> free_var_dist(-8480526731661512249LL, 8480526731661512249LL);
                x_prime[i] = free_var_dist(gen);
            }
            continue;
        }
        
        // 提取 2 的次方 (Trailing Zeros)
        int k = __builtin_ctzll(static_cast<uint64_t>(d));
        uint64_t c = static_cast<uint64_t>(d) >> k;
        
        uint64_t b_uint = static_cast<uint64_t>(bp);
        if (k > 0 && (b_uint & ((1ULL << k) - 1)) != 0) {
            return {}; // 無解 (低位無法整除)
        }
        
        uint64_t b_new = b_uint >> k;
        uint64_t c_inv = modInverse_odd(c);
        
        x_prime[i] = static_cast<int64_t>(b_new * c_inv);
    }
    
    // 自由變數 (index std::min(rows, cols) to cols-1)
    std::uniform_int_distribution<int64_t> free_var_dist(-8480526731661512249LL, 8480526731661512249LL);
    for (int i = std::min(rows, cols); i < cols; ++i) {
        x_prime[i] = free_var_dist(gen);
    }
    
    // 轉回真實的解 x = T * x'
    std::vector<int64_t> x(cols, 0);
    for (int i = 0; i < cols; ++i) {
        for (int j = 0; j < cols; ++j) {
            x[i] = static_cast<int64_t>(static_cast<uint64_t>(x[i]) + static_cast<uint64_t>(T[i][j]) * static_cast<uint64_t>(x_prime[j]));
        }
    }
    
    // 最後驗證機制: A * x == b
    bool valid = true;
    for (int i = 0; i < rows; ++i) {
        uint64_t sum = 0;
        for (int j = 0; j < cols; ++j) {
            sum += static_cast<uint64_t>(A[i][j]) * static_cast<uint64_t>(x[j]);
        }
        if (sum != static_cast<uint64_t>(b[i])) {
            valid = false;
            break;
        }
    }
    
    if (valid) return x;
    return {};
}

} // namespace MBASolver
