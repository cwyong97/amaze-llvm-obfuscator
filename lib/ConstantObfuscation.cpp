#include "ConstantObfuscation.h"

// LLVM 工具
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/Intrinsics.h"

// 混淆演算法工具 (MBA)
#include "MBAEngine.h"

// C++ 標準庫
#include <cstdint>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace llvm;

static cl::opt<int> ConstChance(
    "const-chance", cl::init(100), cl::Hidden,
    cl::desc("The probability (0-100) of applying constant integer obfuscation to an instruction"));


static cl::opt<int> ConstRound(
    "const-round", cl::init(1), cl::Hidden,
    cl::desc("The number of rounds of constant integer obfuscation to apply"));

static cl::opt<int> ConstIntensity(
    "const-intensity", cl::init(100), cl::Hidden,
    cl::desc("The intensity (0-100) of constant integer MBA. 0 means maximum minimization."));

namespace {
using RNG = std::mt19937;

bool isObfuscationAllowed(Instruction *Inst, unsigned OpIdx, ConstantInt *CI) {
    if (isa<GetElementPtrInst>(Inst)) {
        return false;
    }

    if (isa<PHINode>(Inst)) {
        return false;
    }

    if (isa<AllocaInst>(Inst)) {
        return false;
    }

    if (isa<SwitchInst>(Inst)) {
        return false;
    }

    if (auto *CI_call = dyn_cast<CallInst>(Inst)) {
        if (Function *Callee = CI_call->getCalledFunction()) {
            if (Callee->isIntrinsic()) {
                return false;
            }
        }
    }

    if (!Inst->hasMetadata("amaze.target.constant")) {
        return false;
    }

    /* 範例 1：不混淆數值為 0 或 1 的常數
    int64_t val = CI->getSExtValue();
    if (val == 0 || val == 1) {
        return false;
    }
    */

    /* 範例 2：跳過特定 opcode 指令的常數 (例如跳過 Shl 移位指令，防止移位數過大引發未定義行為)
    if (Inst->getOpcode() == Instruction::Shl || Inst->getOpcode() == Instruction::LShr || Inst->getOpcode() == Instruction::AShr) {
        return false;
    }
    */

    return true;
}

void prefillCache(Function &F, std::unordered_map<Type*, Value*> &dummyCacheX, std::unordered_map<Type*, Value*> &dummyCacheY) {
    std::vector<Argument*> intArgs;
    for (Argument &Arg : F.args()) {
        if (Arg.getType()->isIntegerTy()) {
            intArgs.push_back(&Arg);
        }
    }

    if (!intArgs.empty()) {
        IRBuilder<> EntryBuilder(&F.getEntryBlock(), F.getEntryBlock().getFirstInsertionPt());

        if (intArgs.size() >= 2) {
            AllocaInst *AllocaX = EntryBuilder.CreateAlloca(intArgs[0]->getType(), nullptr, "arg_obf_x");
            StoreInst *StoreX = EntryBuilder.CreateStore(intArgs[0], AllocaX);
            StoreX->setVolatile(true);
            LoadInst *LoadX = EntryBuilder.CreateLoad(intArgs[0]->getType(), AllocaX, "arg_obf_x_val");
            LoadX->setVolatile(true);

            AllocaInst *AllocaY = EntryBuilder.CreateAlloca(intArgs[1]->getType(), nullptr, "arg_obf_y");
            StoreInst *StoreY = EntryBuilder.CreateStore(intArgs[1], AllocaY);
            StoreY->setVolatile(true);
            LoadInst *LoadY = EntryBuilder.CreateLoad(intArgs[1]->getType(), AllocaY, "arg_obf_y_val");
            LoadY->setVolatile(true);

            dummyCacheX[intArgs[0]->getType()] = LoadX;
            dummyCacheY[intArgs[1]->getType()] = LoadY;
        } else {
            AllocaInst *AllocaX = EntryBuilder.CreateAlloca(intArgs[0]->getType(), nullptr, "arg_obf_x");
            StoreInst *StoreX = EntryBuilder.CreateStore(intArgs[0], AllocaX);
            StoreX->setVolatile(true);
            LoadInst *LoadX = EntryBuilder.CreateLoad(intArgs[0]->getType(), AllocaX, "arg_obf_x_val");
            LoadX->setVolatile(true);

            dummyCacheX[intArgs[0]->getType()] = LoadX;
            dummyCacheY[intArgs[0]->getType()] = LoadX;
        }
    }
}

bool applyOneRound(Function &F, RNG &gen, std::uniform_int_distribution<>& dist) {
    bool modified = false;

    auto A = MBAEngine::computeBasisMatrix();

    std::unordered_map<Type*, Value*> dummyCacheX;
    std::unordered_map<Type*, Value*> dummyCacheY;
    prefillCache(F, dummyCacheX, dummyCacheY);

    struct ConstantTarget {
        Instruction *Inst;
        unsigned OpIdx;
        ConstantInt *CI;
    };
    std::vector<ConstantTarget> EligibleConstants;
    std::unordered_set<Type*> NeededTypes;

    for (auto &BB : F) {
        for (auto &Inst : BB) {
            unsigned numOperands = Inst.getNumOperands();
            for (unsigned i = 0; i < numOperands; ++i) {
                Value *Op = Inst.getOperand(i);
                if (auto *CI = dyn_cast<ConstantInt>(Op)) {
                    if (!isObfuscationAllowed(&Inst, i, CI)) continue;
                    if (CI->getType()->isIntegerTy(1)) continue;
                    if (CI->getType()->getIntegerBitWidth() > 64) continue;
                    if (dist(gen) > ConstChance) continue;

                    EligibleConstants.push_back({&Inst, i, CI});
                    NeededTypes.insert(CI->getType());
                }
            }
        }
    }

    if (EligibleConstants.empty()) {
        return false;
    }

    IRBuilder<> EntryBuilder(&F.getEntryBlock(), F.getEntryBlock().getFirstInsertionPt());
    Value *BaseFrameInt = nullptr;

    for (Type *Ty : NeededTypes) {
        if (!dummyCacheX.count(Ty) || !dummyCacheY.count(Ty)) {
            if (!BaseFrameInt) {
                Function *FrameAddrIntrinsic = Intrinsic::getDeclaration(F.getParent(), Intrinsic::frameaddress, {EntryBuilder.getPtrTy()});
                Value *FramePtr = EntryBuilder.CreateCall(FrameAddrIntrinsic, {EntryBuilder.getInt32(0)});
                BaseFrameInt = EntryBuilder.CreatePtrToInt(FramePtr, EntryBuilder.getInt64Ty(), "base_frame_int");
            }
            if (!dummyCacheX.count(Ty)) {
                Value *dummyX = EntryBuilder.CreateZExtOrTrunc(BaseFrameInt, Ty, "dummy_x");
                dummyCacheX[Ty] = dummyX;
            }
            if (!dummyCacheY.count(Ty)) {
                std::uniform_int_distribution<int64_t> offsetDist(8, 4096);
                int64_t randomOffset = offsetDist(gen);
                Value *OffsetVal = ConstantInt::get(EntryBuilder.getInt64Ty(), randomOffset);
                Value *FrameIntOffset = EntryBuilder.CreateAdd(BaseFrameInt, OffsetVal, "base_frame_offset");
                Value *dummyY = EntryBuilder.CreateZExtOrTrunc(FrameIntOffset, Ty, "dummy_y");
                dummyCacheY[Ty] = dummyY;
            }
        }
    }

    for (auto &Target : EligibleConstants) {
        Instruction *Inst = Target.Inst;
        unsigned OpIdx = Target.OpIdx;
        ConstantInt *CI = Target.CI;

        int64_t constVal = CI->getSExtValue();
        std::vector<int64_t> b_const = {constVal, constVal, constVal, constVal};

        Value *dummyX = dummyCacheX[CI->getType()];
        Value *dummyY = dummyCacheY[CI->getType()];

        IRBuilder<> builder(Inst);
        Value *obfuscatedConst = MBAEngine::obfuscate(builder, dummyX, dummyY, b_const, A, gen, ConstIntensity);

        if (obfuscatedConst) {
            Inst->setOperand(OpIdx, obfuscatedConst);
            modified = true;
        }
    }
    return modified;
}
} // end anonymous namespace

PreservedAnalyses ConstantObfuscation::run(Function &F, FunctionAnalysisManager &AM) {
    if (F.isDeclaration()) {
        return PreservedAnalyses::all();
    }
    if (F.getName().startswith("__amaze_")) {
        return PreservedAnalyses::all();
    }

    errs() << "[+] Constants in " << F.getName() << "\n";

    std::random_device rd;
    RNG gen(rd());
    std::uniform_int_distribution<> dist(0, 100);

    bool modified = false;
    for (int i = 0; i < ConstRound; ++i) {
        modified |= applyOneRound(F, gen, dist);
    }

    errs() << "[+] Constants in " << F.getName() << " obfuscated: " << modified << "\n";
    return modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
