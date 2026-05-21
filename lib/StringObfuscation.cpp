// header file
#include "StringObfuscation.h"

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
    return modified;
}
} // end anonymous namespace
// StringObfuscation pass implementation
PreservedAnalyses StringObfuscation::run(Function &F, FunctionAnalysisManager &FAM) {
    bool modified = false;
    int rounds = StrRound;  // string obfuscation 的迴圈次數
    // rounds = 1; //要測試的話先把這行打開
    // int current_chance = StrChance;  //還沒用到
    // errs() << "Current string obfuscation chance is set to: " << current_chance << "%\n";
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
