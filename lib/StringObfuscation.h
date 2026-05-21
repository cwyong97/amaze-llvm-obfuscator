#ifndef STRING_OBFUSCATION_H
#define STRING_OBFUSCATION_H

#include "llvm/IR/PassManager.h"

using namespace llvm;

struct StringObfuscation : public PassInfoMixin<StringObfuscation> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
};

#endif