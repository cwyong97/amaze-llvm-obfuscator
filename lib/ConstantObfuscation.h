#pragma once
#include "llvm/IR/PassManager.h"

namespace llvm {

class ConstantObfuscation : public PassInfoMixin<ConstantObfuscation> {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm
