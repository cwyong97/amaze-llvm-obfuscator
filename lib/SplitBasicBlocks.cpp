#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "SplitBasicBlocks.h"
#include <vector>
#include <random>
#include <algorithm>

using namespace llvm;

// Configuration constants for basic block splitting
constexpr int MIN_SPLITS = 1;
constexpr int MAX_SPLITS = 3;
constexpr int MIN_BLOCK_SIZE = 4;

namespace {
struct SplitBasicBlocks_impl {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        if (F.empty()) return PreservedAnalyses::all();
        
        bool modified = false;
        int totalSplits = 0;

        // Setup random number generator
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> splitDist(MIN_SPLITS, MAX_SPLITS);

        // Store original blocks to avoid iterator invalidation during modification
        std::vector<BasicBlock*> origBlocks;
        for (BasicBlock &BB : F) {
            origBlocks.push_back(&BB);
        }

        for (BasicBlock *BB : origBlocks) {
            int splitCount = splitDist(gen);
            BasicBlock *currBB = BB;

            for (int i = 0; i < splitCount; ++i) {
                // Skip blocks that are too small to split safely
                if (currBB->size() < MIN_BLOCK_SIZE) break;

                // Pick a random instruction in the middle of the block
                int size = currBB->size();
                std::uniform_int_distribution<> instDist(1, size - 2);
                int splitIndex = instDist(gen);

                // Use iterator arithmetic for efficient instruction lookup
                auto it = currBB->begin();
                std::advance(it, splitIndex);
                Instruction *splitInst = &*it;

                // Safety checks: Never split at PHI nodes, Exception pads, or Terminators
                // These instruction types cannot be legally placed at block boundaries
                if (isa<PHINode>(splitInst) || splitInst->isEHPad() || splitInst->isTerminator()) {
                    continue;
                }

                // Split the block at the chosen instruction
                // 'currBB' becomes the first half, and the return value is the second half
                currBB = currBB->splitBasicBlock(splitInst, "split_block");
                totalSplits++;
                modified = true;
            }
        }

        // Log results for debugging
        if (modified) {
            errs() << "SplitBasicBlocks: Split " << totalSplits << " locations in function " 
                   << F.getName() << "\n";
        }

        return modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};
} // end anonymous namespace

PreservedAnalyses SplitBasicBlocks::run(Function &F, FunctionAnalysisManager &FAM) {
    return SplitBasicBlocks_impl().run(F, FAM);
}
