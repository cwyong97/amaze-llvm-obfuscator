#ifndef STRING_OBFUSCATION_H
#define STRING_OBFUSCATION_H

#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"

using namespace llvm;

struct StringObfuscation : public PassInfoMixin<StringObfuscation> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

#endif