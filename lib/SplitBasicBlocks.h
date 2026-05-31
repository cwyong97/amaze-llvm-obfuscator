#ifndef SPLIT_BASIC_BLOCKS_H
#define SPLIT_BASIC_BLOCKS_H

#include "llvm/IR/PassManager.h"

using namespace llvm;

struct SplitBasicBlocks : public PassInfoMixin<SplitBasicBlocks> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
};

#endif
