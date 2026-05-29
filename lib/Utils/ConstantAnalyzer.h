#ifndef AMAZE_CONSTANT_ANALYZER_H
#define AMAZE_CONSTANT_ANALYZER_H

#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/PassManager.h"
#include <map>
#include <vector>

namespace llvm {

class ConstantAnalyzer {
public:
    struct ConstantUse {
        Instruction *Inst;
        unsigned OpIdx;
    };

    using ConstantKey = std::pair<int64_t, Type*>;

    static std::map<ConstantKey, std::vector<ConstantUse>> analyzeConstants(Module &M);
    static void dumpTopDuplicates(Module &M, unsigned TopN = 10);
    static unsigned tagConstantOperands(Module &M, unsigned Threshold = 3);
};

class ConstantTaggingPass : public PassInfoMixin<ConstantTaggingPass> {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

} // namespace llvm

#endif // AMAZE_CONSTANT_ANALYZER_H
