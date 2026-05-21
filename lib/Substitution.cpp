// header file
#include "Substitution.h"

// LLVM 工具
#include "llvm/IR/IRBuilder.h" // for IRBuilder
#include "llvm/Support/raw_ostream.h" // for errs()
#include "llvm/Support/CommandLine.h" // for command-line options

// 混淆演算法工具 (MBA)
#include "MBABasis.h"
#include "MBASolver.h"

// C++ 標準庫
#include <algorithm>
#include <cstdint>
#include <numeric>
#include <random>
using namespace llvm;

static cl::opt<int> SubChance(
    "sub-chance", cl::init(100), cl::Hidden,
    cl::desc("The probability (0-100) of applying substitution to an instruction"));

static cl::opt<int> SubRound(
    "sub-round", cl::init(1), cl::Hidden,
    cl::desc("The number of rounds of substitution to apply"));


namespace {
using RNG = std::mt19937;
// bool applyAddPolynomialObf(BinaryOperator *op, RNG &gen, std::uniform_int_distribution<> &dist) {
//     IRBuilder<> builder(op);
//     Value *op1 = op->getOperand(0);
//     Value *op2 = op->getOperand(1);
//     Type* ty = op->getType();
//     Value *newAdd = nullptr;
//     int choice = dist(gen);
//     choice = 0;
//     switch (choice) {
//     case 0: {
// // ------------------------------------------------------------
//     // 第一步：將 A + B 轉換為線性 MBA
//     // 公式: A + B = (A ^ B) + 2 * (A & B)
//     // ------------------------------------------------------------
//     Value* xorVal = builder.CreateXor(op1, op2, "mba.xor");
//     Value* andVal = builder.CreateAnd(op1, op2, "mba.and");
//     Value* shlVal = builder.CreateShl(andVal, ConstantInt::get(ty, 1), "mba.shl");
//     Value* linearMBA = builder.CreateAdd(xorVal, shlVal, "mba.linear.add");

//     // ------------------------------------------------------------
//     // 第二步：構造零值多項式 ZEP (Zero-Evaluating Polynomial)
//     // 公式: ZEP(x) = (x^2 + x) * 0x80000000 == 0
//     // 我們可以隨機挑選 op1 作為種子變數 x
//     // ------------------------------------------------------------
//     Value* x = op1; 
//     // x^2
//     Value* x_sq = builder.CreateMul(x, x, "zep.x_sq");
//     // x^2 + x
//     Value* x_sq_plus_x = builder.CreateAdd(x_sq, x, "zep.add");
//     // (x^2 + x) * 0x80000000
//     Value* zep = builder.CreateMul(x_sq_plus_x, ConstantInt::get(ty, 0x80000000), "zep.mul");

//     // ------------------------------------------------------------
//     // 第三步：構造複雜恆等式並注入乘法
//     // 公式: 1_complex = ZEP + 1 == 1
//     //       Result = linearMBA * 1_complex
//     // ------------------------------------------------------------
//     Value* complex_one = builder.CreateAdd(zep, ConstantInt::get(ty, 1), "complex.one");
//     newAdd = builder.CreateMul(linearMBA, complex_one, "poly.mba.result");
//     break;
//     }
//     default:
//         // Fallback: leave instruction unchanged
//         newAdd = op;
//         return false;
//     }
//     op->replaceAllUsesWith(newAdd);
//     op->eraseFromParent();
//     return true;
// }
// bool applyAndObf(BinaryOperator *op, RNG &gen, std::uniform_int_distribution<> &dist) {
//     IRBuilder<> builder(op);
//     Value *a = op->getOperand(0);
//     Value *b = op->getOperand(1);
//     Value *newAnd = nullptr;

//     int choice = dist(gen);
//     choice = 0;
//     switch (choice) {
//     case 0: {
//     // a & b -> (a ^ ~b) & a
//         Value *notB = builder.CreateNot(b, "notB");
//         Value *xorAB = builder.CreateXor(a, notB, "xorAB");
//         newAnd = builder.CreateAnd(xorAB, a, "and0Substitute");
//         break;
//     }
//     default:
//         // Fallback: leave instruction unchanged
//         newAnd = op;
//         return false;
//     }
//     op->replaceAllUsesWith(newAnd);
//     op->eraseFromParent();
//     return true;
// }

bool applyOneRound(Function &F, RNG &gen, std::uniform_int_distribution<> &dist) {
    bool modified = false;

    // 優化：矩陣 A 是固定的，拉到迴圈外部只計算一次
    std::vector<std::vector<int64_t>> A(4, std::vector<int64_t>(basis_pool.size()));
    for (int i = 0; i < 4; ++i) {
        int64_t vx = (i >> 1) & 1;
        int64_t vy = i & 1;
        for (size_t j = 0; j < basis_pool.size(); ++j) {
            // 確保 basis_pool 的 op 回傳值也能正確轉型為 int64_t
            A[i][j] = static_cast<int64_t>(basis_pool[j].op(vx, vy));
        }
    }
    // int crazynumber = dist(gen); //測試用的隨機數，決定 substitution 的機率

    for (auto &BB : F) {
        for (auto &Inst : make_early_inc_range(BB)) {
            auto *op = dyn_cast<BinaryOperator>(&Inst);
            if (!op) {
                continue;
            }
            // if (crazynumber % 2 == 0) { //測試降低 substitution 的機率，看看效果
            //     continue; // 約束值
            // }
            int64_t table_00 = 0, table_01 = 0, table_10 = 0, table_11 = 0;
            bool applied = true;
            // int choice = dist(gen); //testing for other substitutions method
            switch (op->getOpcode())
            {

            case Instruction::Add:
                table_00 = 0; table_01 = 1; table_10 = 1; table_11 = 2; // 真值表對應 Add 的輸出1+1=2
                // if (choice % 2 == 0) { //testing for other substitutions method
                //     applyAddPolynomialObf(op, gen, dist);
                // }
                // applied = false;        
                break;
            case Instruction::Sub:
                table_00 = 0; table_01 = -1; table_10 = 1; table_11 = 0; // 真值表對應 Sub 的輸出1-1=0
                break;
            case Instruction::And:
                table_00 = 0; table_01 = 0; table_10 = 0; table_11 = 1; // 真值表對應 And 的輸出1&1=1
                // applyAndObf(op, gen, dist); //testing for other substitutions method
                // applied = false;
                break;
            case Instruction::Or:
                table_00 = 0; table_01 = 1; table_10 = 1; table_11 = 1; // 真值表對應 Or 的輸出1|1=1
                break;
            case Instruction::Xor:
                table_00 = 0; table_01 = 1; table_10 = 1; table_11 = 0; // 真值表對應 Xor 的輸出1^1=0
                break;
            default:
                applied = false;
                break;
            }
            if (!applied) {
                continue;
            }
            std::vector<int64_t> b = {table_00, table_01, table_10, table_11};

            // 隨機打亂 A 的 column 與 basis_pool 的對應關係，讓每次求解選到不同的 pivot
            std::vector<size_t> perm(basis_pool.size());
            std::iota(perm.begin(), perm.end(), 0);
            std::shuffle(perm.begin(), perm.end(), gen);    //註解此行能停止隨機打亂
            std::vector<std::vector<int64_t>> A_shuffled(4, std::vector<int64_t>(basis_pool.size()));
            for (int row = 0; row < 4; ++row) {
                for (size_t col = 0; col < basis_pool.size(); ++col) {
                    A_shuffled[row][col] = A[row][perm[col]];
                }
            }

            std::vector<int64_t> x = MBASolver::solve(A_shuffled, b, gen);
            if (x.empty()) continue;
            IRBuilder<> builder(op);
            Value *valX = op->getOperand(0);
            Value *valY = op->getOperand(1);
            std::vector<size_t> selectedIndices;
            Value *obfuscatedExpr = nullptr;
            for (size_t j = 0; j < x.size(); ++j) {
                if (x[j] == 0) {
                    continue; // 如果算出的係數是 0，代表這個基底沒用到，直接跳過 (效能優化)
                }
                selectedIndices.push_back(j);
            }
            for (size_t j : selectedIndices) {
                Value *basisExpr = basis_pool[perm[j]].genIR(builder, valX, valY);
                Value *coef = ConstantInt::get(valX->getType(), x[j]);
                Value *term = builder.CreateMul(coef, basisExpr, "term");
                if (!obfuscatedExpr)
                    obfuscatedExpr = term;
                else
                    obfuscatedExpr = builder.CreateAdd(obfuscatedExpr, term, "agg");
            }
            // 將原本的加法/減法指令，替換成等價的混淆多項式
            if (obfuscatedExpr) {
                op->replaceAllUsesWith(obfuscatedExpr);
                op->eraseFromParent();
                modified = true;
            }
        }
    }
    return modified;
}
} // end anonymous namespace

// Substitution pass implementation
PreservedAnalyses Substitution::run(Function &F, FunctionAnalysisManager &FAM) {
    bool modified = false;
    int rounds = SubRound;  // substitution 的迴圈次數
    // rounds = 1; //要測試的話先把這行打開
    // int current_chance = SubChance;  //還沒用到
    // errs() << "Current substitution chance is set to: " << current_chance << "%\n";
    errs() << "Current substitution rounds is set to: " << rounds << "\n";
    errs() << "<testmessage> Running Substitution on function: " << F.getName() << "\n";

    std::random_device rd;
    RNG gen(rd());
    std::uniform_int_distribution<> dist(0, 100);
    for (int i = 0; i < rounds; ++i) {
        modified |= applyOneRound(F, gen, dist);
    }
    return modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
