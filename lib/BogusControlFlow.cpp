#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Support/raw_ostream.h"
#include "BogusControlFlow.h"
#include <vector>
#include <random>

using namespace llvm;

// 混淆觸發機率設定 (100% 的合法區塊會被選中插入虛假控制流)
constexpr int BCF_PROBABILITY = 100; 

namespace {
struct BogusControlFlow_impl {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        if (F.empty()) return PreservedAnalyses::all();
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> probDist(0, 100);

        // 1. 先收集所有合法的 Basic Block，避免後續動態新增區塊導致迭代器失效崩潰
        std::vector<BasicBlock*> blocks;
        for (BasicBlock &BB : F) {
            blocks.push_back(&BB);
        }

        int insertCount = 0;
        for (BasicBlock *BB : blocks) {
            if (BB->empty()) continue;
            
            // 隨機機率過濾
            if (probDist(gen) > BCF_PROBABILITY) continue;
            
            // 2. 嚴格篩選過濾：只針對「無條件跳躍」的區塊進行混淆
            Instruction *term = BB->getTerminator();
            if (!term) continue;
            
            if (!isa<BranchInst>(term)) continue;
            BranchInst *br = cast<BranchInst>(term);
            if (br->isConditional()) continue; // 跳過已有條件判斷的區塊
            if (br->getNumSuccessors() != 1) continue;
            
            BasicBlock *successor = br->getSuccessor(0);
            
            // 🔥 【重要安全檢查】：如果下一站區塊的開頭是 PHI Node，絕對不要動它！
            if (isa<PHINode>(successor->begin())) {
                continue;
            }
            
            // 3. 建立「永遠走不到的」虛假垃圾區塊 (bcf_dead)
            BasicBlock *deadBB = BasicBlock::Create(
                F.getContext(),
                "bcf_dead",
                &F,
                successor
            );
            
            // 在垃圾區塊內塞入隨機垃圾運算，增加逆向工具的分析厚度
            IRBuilder<> deadBuilder(deadBB);
            Value *junkA = deadBuilder.getInt32(probDist(gen));
            Value *junkB = deadBuilder.getInt32(probDist(gen));
            Value *junkResult = deadBuilder.CreateMul(junkA, junkB, "junk_mul");
            // 垃圾區塊最後一樣要導回正確的下一站，維持程式原始語意正確
            deadBuilder.CreateBr(successor);
            
            // 4. 安全地建立對抗 IDA 靜態優化的條件分支 (先建新分支，後拔舊分支)
            IRBuilder<> builder(term);
            Value *cond = createGlobalVarBogusCondition(builder);
            
            if (cond) {
                // True 走正確路徑，False 走垃圾區塊 (雖然動態執行時恆為 True)
                builder.CreateCondBr(cond, successor, deadBB);
                term->eraseFromParent();
                insertCount++;
            } else {
                deadBB->eraseFromParent();
            }
        }

        // 5. 輸出統計 Log
        if (insertCount > 0) {
            errs() << "[AmazeLLVM-BCF] Successfully inserted " << insertCount 
                   << " bogus branches into function: " << F.getName() << "\n";
            return PreservedAnalyses::none(); // 告訴 LLVM 我們改變了 CFG 結構
        }
        
        return PreservedAnalyses::all();
    }

private:
    /**
     * 建立抗靜態分析優化的不透明謂詞 (Opaque Predicate)
     * 【改進重點】：引入隨機 Store 指令，破壞 IDA 的 Global dead-store 數據流推導
     */
    Value *createGlobalVarBogusCondition(IRBuilder<> &builder) {
        // 取得目前施工中的 Module 參考
        Module *M = builder.GetInsertBlock()->getModule();
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 1000);
        
        // 尋找 Module 內是否已經宣告過這個全域種子變數
        GlobalVariable *seedVar = M->getGlobalVariable("AmazeLLVM_OpaqueSeed");
        if (!seedVar) {
            // 如果還沒有定義過，就動態動工建立它
            seedVar = new GlobalVariable(
                *M,
                builder.getInt32Ty(),              // 32 位元整數型態
                false,                             // isConstant = false (非常數，故意欺騙優化器)
                GlobalValue::InternalLinkage,      // 內部連接，防止符號污染
                builder.getInt32(0),               // 初始值設為 0
                "AmazeLLVM_OpaqueSeed"             // 變數名稱
            );
        }

        // 🚨 【核心改進：補上這行大絕招】
        // 在讀取之前，先用程式碼去「Store（寫入）」一個隨機數字到這個全域變數中
        // 只要 IDA 靜態掃描發現這個檔案「有 Store 行為」，它就再也無法武斷認定 seedVar 永遠為 0！
        // 我們將其設定為 volatile，以完全防止 LLVM 優化器消除該寫入或進行跨基本塊的常數傳播
        StoreInst *storeInst = builder.CreateStore(builder.getInt32(dis(gen)), seedVar);
        storeInst->setVolatile(true);
        storeInst->setMetadata("amaze.target.constant", MDNode::get(M->getContext(), {}));

        // 建立讀取指令 (Load)：從全域變數中把值撈上來，並設定為 volatile 徹底封鎖編譯器的優化與折疊
        LoadInst *loadInst = builder.CreateLoad(builder.getInt32Ty(), seedVar, "seed_load");
        loadInst->setVolatile(true);
        Value *seedLoad = loadInst;
        
        // 【數學恆等式升級】：因為變數會變，我們不用 (seed_load == 0) 了，因為那樣假分支真的會被執行到！
        // 我們改用絕對安全的數學特性： (seed_load * seed_load) >= 0
        // 有號整數的平方在不考慮極端溢位下（且我們只生成 1~1000）絕對 >= 0，動態執行時百分之百恆為 True！
        Value *squared = builder.CreateMul(seedLoad, seedLoad, "opaque_mul");
        Value *cond = builder.CreateICmpSGE(squared, builder.getInt32(0), "opaque_cond");
        if (auto *condInst = dyn_cast<Instruction>(cond)) {
            condInst->setMetadata("amaze.target.constant", MDNode::get(M->getContext(), {}));
        }
        
        return cond;
    }
};
} // end anonymous namespace

PreservedAnalyses BogusControlFlow::run(Function &F, FunctionAnalysisManager &FAM) {
    return BogusControlFlow_impl().run(F, FAM);
}
