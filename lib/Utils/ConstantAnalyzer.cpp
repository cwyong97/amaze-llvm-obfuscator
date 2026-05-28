#include "ConstantAnalyzer.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Support/CommandLine.h"
#include <algorithm>

using namespace llvm;

static cl::opt<unsigned> ConstThreshold(
    "const-threshold", cl::init(3), cl::Hidden,
    cl::desc("The minimum frequency of a constant value required to tag it for obfuscation"));

namespace {
bool isTaggingAllowed(Instruction *Inst, unsigned OpIdx, ConstantInt *CI) {
    if (isa<GetElementPtrInst>(Inst)) return false;
    if (isa<PHINode>(Inst)) return false;
    if (isa<AllocaInst>(Inst)) return false;
    if (isa<SwitchInst>(Inst)) return false;

    if (auto *CI_call = dyn_cast<CallInst>(Inst)) {
        if (Function *Callee = CI_call->getCalledFunction()) {
            if (Callee->isIntrinsic()) return false;
        }
    }

    int64_t Val = CI->getSExtValue();
    uint64_t AbsVal = (Val < 0) ? -static_cast<uint64_t>(Val) : static_cast<uint64_t>(Val);
    if (AbsVal <= 10) {
        return false;
    }

    return true;
}
} // namespace

std::map<ConstantAnalyzer::ConstantKey, std::vector<ConstantAnalyzer::ConstantUse>> 
ConstantAnalyzer::analyzeConstants(Module &M) {
    std::map<ConstantKey, std::vector<ConstantUse>> Results;

    for (Function &F : M) {
        if (F.isDeclaration()) continue;
        if (F.getName().startswith("__amaze_")) continue;

        for (BasicBlock &BB : F) {
            for (Instruction &Inst : BB) {
                unsigned numOperands = Inst.getNumOperands();
                for (unsigned i = 0; i < numOperands; ++i) {
                    Value *Op = Inst.getOperand(i);
                    if (auto *CI = dyn_cast<ConstantInt>(Op)) {
                        if (CI->getType()->isIntegerTy(1)) continue;
                        if (CI->getType()->getIntegerBitWidth() > 64) continue;

                        if (!isTaggingAllowed(&Inst, i, CI)) continue;

                        int64_t constVal = CI->getSExtValue();
                        Results[{constVal, CI->getType()}].push_back({&Inst, i});
                    }
                }
            }
        }
    }
    return Results;
}

void ConstantAnalyzer::dumpTopDuplicates(Module &M, unsigned TopN) {
    auto Results = analyzeConstants(M);

    std::vector<std::pair<ConstantKey, size_t>> SortedFreq;
    for (auto &Pair : Results) {
        SortedFreq.push_back({Pair.first, Pair.second.size()});
    }

    std::sort(SortedFreq.begin(), SortedFreq.end(), 
              [](const std::pair<ConstantKey, size_t> &a, const std::pair<ConstantKey, size_t> &b) {
                  return a.second > b.second;
              });

    errs() << "\n==================================================\n";
    errs() << "Constant Duplication Statistics (Top " << TopN << ")\n";
    errs() << "==================================================\n";

    unsigned Count = 0;
    for (auto &Pair : SortedFreq) {
        if (Count >= TopN) break;
        ConstantKey Key = Pair.first;
        int64_t Val = Key.first;
        Type *Ty = Key.second;
        size_t Freq = Pair.second;

        std::string TypeStr;
        raw_string_ostream rso(TypeStr);
        Ty->print(rso);

        errs() << " 常數數字: " << Val << " (" << rso.str() << ") | 重複出現次數: " << Freq << "\n";
        
        auto &Uses = Results[Key];
        unsigned UseCount = 0;
        for (auto &U : Uses) {
            if (UseCount >= 3) {
                errs() << "    ... 以及其餘 " << (Uses.size() - 3) << " 處引用。\n";
                break;
            }
            errs() << "    [位置] 函數: " << U.Inst->getFunction()->getName() 
                   << " | 指令: " << *U.Inst << "\n";
            UseCount++;
        }
        errs() << "--------------------------------------------------\n";
        Count++;
    }
}

unsigned ConstantAnalyzer::tagConstantOperands(Module &M, unsigned Threshold) {
    auto Results = analyzeConstants(M);
    unsigned TaggedCount = 0;

    LLVMContext &Ctx = M.getContext();
    MDNode *MD = MDNode::get(Ctx, {});

    for (auto &Pair : Results) {
        auto &Uses = Pair.second;
        if (Uses.size() < Threshold) continue;

        for (auto &UseInfo : Uses) {
            UseInfo.Inst->setMetadata("amaze.target.constant", MD);
        }
        TaggedCount += Uses.size();
    }
    return TaggedCount;
}

PreservedAnalyses ConstantTaggingPass::run(Module &M, ModuleAnalysisManager &AM) {
    errs() << "[+] ConstantTaggingPass: Scanning and tagging high-frequency targeted constants...\n";

    ConstantAnalyzer::dumpTopDuplicates(M, 10);

    unsigned taggedInsts = ConstantAnalyzer::tagConstantOperands(M, ConstThreshold);
    errs() << "[+] ConstantTaggingPass: Successfully tagged " << taggedInsts 
           << " constant operands with !amaze.target.constant.\n";

    return taggedInsts > 0 ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
