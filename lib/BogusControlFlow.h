#ifndef BOGUS_CONTROL_FLOW_H
#define BOGUS_CONTROL_FLOW_H

#include "llvm/IR/PassManager.h"

using namespace llvm;

struct BogusControlFlow : public PassInfoMixin<BogusControlFlow> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
};

#endif
