#include "MBABuilder.h"
#include "MBABasis.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Metadata.h"
#include <numeric>
#include <iterator>

using namespace llvm;

Value *MBABuilder::buildLinearMBA(IRBuilder<>& builder, 
                                 Value *lhs, 
                                 Value *rhs, 
                                 const std::vector<int64_t> &x, 
                                 const std::vector<size_t> &perm) {
    Instruction *prevInst = nullptr;
    BasicBlock::iterator insertPt = builder.GetInsertPoint();
    if (insertPt != builder.GetInsertBlock()->begin() && !builder.GetInsertBlock()->empty()) {
        prevInst = &*std::prev(insertPt);
    }

    if (!lhs || !rhs) return nullptr;
    if (x.empty()) return nullptr;

    Value *obfuscatedExpr = nullptr;
    for (size_t j = 0; j < x.size(); ++j) {
        if (x[j] == 0)
            continue;

        // 呼叫對應基底的 genIR 生成對應的 IR 子樹
        Value *basisExpr = basis_pool[perm[j]].genIR(builder, lhs, rhs);
        if (!basisExpr) return nullptr;
        // 取得係數的常數物件
        Value *coef = getIntOrSplat(lhs->getType(), x[j]);
        if (!coef) return nullptr;
        // 構造 term: coef * basisExpr
        Value *term = builder.CreateMul(coef, basisExpr);
        if (!term) return nullptr;
        
        // 將每個 term 相加
        if (!obfuscatedExpr)
            obfuscatedExpr = term;
        else
            obfuscatedExpr = builder.CreateAdd(obfuscatedExpr, term);
    }

    BasicBlock::iterator it;
    if (prevInst) {
        it = std::next(prevInst->getIterator());
    } else {
        it = builder.GetInsertBlock()->begin();
    }
    while (it != insertPt) {
        Instruction &I = *it;
        I.setMetadata("amaze.obfuscated", MDNode::get(I.getContext(), {}));
        ++it;
    }

    return obfuscatedExpr;
}

Value *MBABuilder::buildLinearMBA(IRBuilder<>& builder, 
                                 Value *lhs, 
                                 Value *rhs, 
                                 const std::vector<int64_t> &x) {
    std::vector<size_t> perm(x.size());
    std::iota(perm.begin(), perm.end(), 0);
    return buildLinearMBA(builder, lhs, rhs, x, perm);
}
