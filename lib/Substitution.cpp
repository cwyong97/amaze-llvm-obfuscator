// header file
#include "Substitution.h"

// LLVM 工具
#include "llvm/IR/IRBuilder.h"        // for IRBuilder
#include "llvm/Support/raw_ostream.h" // for errs()
#include "llvm/Support/CommandLine.h" // for command-line options

// 混淆演算法工具 (MBA)
#include "MBAEngine.h"

// C++ 標準庫
#include <cstdint>
#include <random>

using namespace llvm;

static cl::opt<int> SubChance(
    "sub-chance", cl::init(100), cl::Hidden,
    cl::desc("The probability (0-100) of applying substitution to an instruction"));

static cl::opt<int> SubRound(
    "sub-round", cl::init(1), cl::Hidden,
    cl::desc("The number of rounds of substitution to apply"));

static cl::opt<int> SubIntensity(
    "sub-intensity", cl::init(100), cl::Hidden,
    cl::desc("The intensity (0-100) of substitution MBA. 0 means maximum minimization."));

namespace
{
    using RNG = std::mt19937;

    bool obfuscateBinaryOperators(Function &F, const std::vector<std::vector<int64_t>> &A, RNG &gen, std::uniform_int_distribution<> &dist)
    {
        bool modified = false;
        for (auto &BB : F)
        {
            for (auto &Inst : make_early_inc_range(BB))
            {
                auto *op = dyn_cast<BinaryOperator>(&Inst);
                if (!op)
                    continue;
                
                if (op->getMetadata("amaze.obfuscated")) {
                    // errs() << "Skipping obfuscated instruction: " << *op << "\n";
                    continue;
                }

                if (dist(gen) > SubChance)
                    continue;

                int64_t table_00 = 0, table_01 = 0, table_10 = 0, table_11 = 0;
                bool applied = true;
                switch (op->getOpcode())
                {
                case Instruction::Add:
                    table_00 = 0; table_01 = 1; table_10 = 1; table_11 = 2;
                    break;
                case Instruction::Sub:
                    table_00 = 0; table_01 = -1; table_10 = 1; table_11 = 0;
                    break;
                case Instruction::And:
                    table_00 = 0; table_01 = 0; table_10 = 0; table_11 = 1;
                    break;
                case Instruction::Or:
                    table_00 = 0; table_01 = 1; table_10 = 1; table_11 = 1;
                    break;
                case Instruction::Xor:
                    table_00 = 0; table_01 = 1; table_10 = 1; table_11 = 0;
                    break;
                default:
                    applied = false;
                    break;
                }
                if (!applied)
                    continue;

                std::vector<int64_t> b = {table_00, table_01, table_10, table_11};

                IRBuilder<> builder(op);
                Value *valX = op->getOperand(0);
                Value *valY = op->getOperand(1);
                if (!valX || !valY)
                    continue;
                if (!valX->getType()->isIntOrIntVectorTy() || !valY->getType()->isIntOrIntVectorTy())
                    continue;

                Value *obfuscatedExpr = MBAEngine::obfuscate(builder, valX, valY, b, A, gen, SubIntensity);

                if (obfuscatedExpr)
                {
                    op->replaceAllUsesWith(obfuscatedExpr);
                    op->eraseFromParent();
                    modified = true;
                }
            }
        }
        return modified;
    }

    bool applyOneRound(Function &F, RNG &gen, std::uniform_int_distribution<> &dist)
    {
        bool modified = false;
        auto A = MBAEngine::computeBasisMatrix();
        modified |= obfuscateBinaryOperators(F, A, gen, dist);
        return modified;
    }
} // end anonymous namespace

// Substitution pass implementation
PreservedAnalyses Substitution::run(Function &F, FunctionAnalysisManager &FAM)
{
    if (F.isDeclaration())
        return PreservedAnalyses::all();
    if (F.getName().startswith("__amaze_"))
        return PreservedAnalyses::all();

    bool modified = false;
    int rounds = SubRound; // substitution 的迴圈次數
    // rounds = 1; //要測試的話先把這行打開
    // int current_chance = SubChance;  //還沒用到
    // errs() << "Current substitution chance is set to: " << current_chance << "%\n";
    errs() << "Current substitution rounds is set to: " << rounds << "\n";
    errs() << "<testmessage> Running Substitution on function: " << F.getName() << "\n";

    std::random_device rd;
    RNG gen(rd());
    std::uniform_int_distribution<> dist(0, 100);
    for (int i = 0; i < rounds; ++i)
    {
        modified |= applyOneRound(F, gen, dist);
    }
    return modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
