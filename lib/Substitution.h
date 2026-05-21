#ifndef SUBSTITUTION_H
#define SUBSTITUTION_H

#include "llvm/IR/PassManager.h"

using namespace llvm;

struct Substitution : public PassInfoMixin<Substitution> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
};

#endif