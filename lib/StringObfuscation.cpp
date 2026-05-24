// header file
#include "StringObfuscation.h"

// LLVM 工具
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"

// C++ 標準庫
#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>

using namespace llvm;

static cl::opt<int> StrChance(
    "str-chance", cl::init(100), cl::Hidden,
    cl::desc("The probability (0-100) of applying string obfuscation to an instruction"));

static cl::opt<int> StrRound(
    "str-round", cl::init(1), cl::Hidden,
    cl::desc("The number of rounds of string obfuscation to apply"));

namespace {
using RNG = std::mt19937;

bool applyOneRound(Function &F, RNG &gen, std::uniform_int_distribution<> &dist) {
    bool modified = false;
    LLVMContext &Ctx = F.getContext();
    IRBuilder<> Builder(Ctx);

    std::vector<std::pair<Instruction*, Use*>> ToReplace;

    for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
            for (Use &Op : I.operands()) {
                auto *GV = dyn_cast<GlobalVariable>(Op.get());
                if (!GV || !GV->isConstant() || !GV->hasInitializer()) continue;

                auto *CDA = dyn_cast<ConstantDataArray>(GV->getInitializer());
                if (!CDA || !CDA->isString()) continue;

                if (dist(gen) > StrChance) continue;

                ToReplace.push_back({&I, &Op});
            }
        }
    }

    if (ToReplace.empty()) return false;

    IRBuilder<> EntryBuilder(&*F.getEntryBlock().getFirstInsertionPt());
    std::unordered_map<GlobalVariable*, AllocaInst*> GlobalToAlloca;

    for (auto &Pair : ToReplace) {
        Use *Op = Pair.second;
        auto *GV = cast<GlobalVariable>(Op->get());
        if (GlobalToAlloca.count(GV)) continue;

        StringRef StrVal = cast<ConstantDataArray>(GV->getInitializer())->getAsString();
        size_t len = StrVal.size() + 1; 
        size_t chunkCount = (len + 7) / 8; 

        ArrayType *ArrTy = ArrayType::get(EntryBuilder.getInt64Ty(), chunkCount);
        AllocaInst *StrAlloc = EntryBuilder.CreateAlloca(ArrTy, nullptr, "str_alloc_long");
        StrAlloc->setAlignment(Align(16));
        GlobalToAlloca[GV] = StrAlloc;
    }

    std::unordered_map<GlobalVariable*, bool> Initialized;

    for (size_t idx = 0; idx < ToReplace.size(); ++idx) {
        Instruction *Inst = ToReplace[idx].first;
        Use *Op = ToReplace[idx].second;
        auto *GV = cast<GlobalVariable>(Op->get());
        AllocaInst *StrAlloc = GlobalToAlloca[GV];

        Builder.SetInsertPoint(Inst);

        if (!Initialized[GV]) {
            StringRef StrVal = cast<ConstantDataArray>(GV->getInitializer())->getAsString();
            size_t len = StrVal.size() + 1;
            size_t chunkCount = (len + 7) / 8;

            for (size_t c = 0; c < chunkCount; ++c) {
                uint64_t chunkInt = 0;
                for (size_t i = 0; i < 8; ++i) {
                    size_t charIdx = c * 8 + i;
                    uint8_t charVal = (charIdx < len - 1) ? StrVal[charIdx] : 0;
                    chunkInt |= (static_cast<uint64_t>(charVal) << (i * 8));
                }

                Constant *ConstChunkInt = Builder.getInt64(chunkInt);
                Value *Indices[] = {Builder.getInt32(0), Builder.getInt32(c)};
                Value *ChunkPtr = Builder.CreateInBoundsGEP(StrAlloc->getAllocatedType(), StrAlloc, Indices);

                StoreInst *Store = Builder.CreateStore(ConstChunkInt, ChunkPtr);
                Store->setAlignment(Align(16));
            }
            Initialized[GV] = true;
        }

        if (auto *CI = dyn_cast<CallInst>(Inst)) {
            CI->setTailCall(false);
        }

        Op->set(StrAlloc);
        modified = true;
    }

    std::vector<GlobalVariable*> DeadGlobals;
    for (GlobalVariable &GV : F.getParent()->globals()) {
        if (GV.use_empty() && GV.hasInitializer() && isa<ConstantDataArray>(GV.getInitializer())) {
            DeadGlobals.push_back(&GV);
        }
    }
    for (GlobalVariable *GV : DeadGlobals) {
        GV->eraseFromParent();
    }

    return modified;
}
} // end anonymous namespace

PreservedAnalyses StringObfuscation::run(Function &F, FunctionAnalysisManager &FAM) {
    bool modified = false;
    int rounds = StrRound;
    errs() << "Current string obfuscation rounds is set to: " << rounds << "\n";
    errs() << "<testmessage> Running StringObfuscation on function: " << F.getName() << "\n";

    std::random_device rd;
    RNG gen(rd());
    std::uniform_int_distribution<> dist(0, 100);
    for (int i = 0; i < rounds; ++i) {
        modified |= applyOneRound(F, gen, dist);
    }
    return modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
