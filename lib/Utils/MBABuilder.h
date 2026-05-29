#ifndef MBA_BUILDER_H
#define MBA_BUILDER_H

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"
#include <vector>
#include <cstdint>

using namespace llvm;

class MBABuilder {
public:
    // 根據傳入的解向量 x 與對應的基底置換 perm，生成對應的 Linear MBA 表示式 IR 樹：
    // sum_{j} (x[j] * basis_pool[perm[j]](lhs, rhs))
    static Value *buildLinearMBA(IRBuilder<>& builder, 
                                 Value *lhs, 
                                 Value *rhs, 
                                 const std::vector<int64_t> &x, 
                                 const std::vector<size_t> &perm);

    // 重載版本：若無進行 column 打亂，則預設採用順序 [0, 1, 2, ...] 的 basis_pool
    static Value *buildLinearMBA(IRBuilder<>& builder, 
                                 Value *lhs, 
                                 Value *rhs, 
                                 const std::vector<int64_t> &x);
};

#endif // MBA_BUILDER_H
