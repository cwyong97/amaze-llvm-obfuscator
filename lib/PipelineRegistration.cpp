#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"

// 引入所有 Pass 的標頭檔
#include "StringObfuscation.h" 
#include "Substitution.h"
// #include "ControlFlowFlattening.h" // 未來新增的

using namespace llvm;

// --- 在這裡定義開關 (CommandLine Flags) ---
// 讓 opt 多出 -obf-string 和 -obf-sub 這些參數可以用
static cl::opt<bool> EnableAllObf(
    "obf-all", cl::init(false), cl::Hidden,
    cl::desc("Enable all obfuscation passes"));
static cl::opt<bool> EnableStringObf(
    "obf-string", cl::init(false), cl::Hidden,
    cl::desc("Enable String Obfuscation"));

static cl::opt<bool> EnableSubstitution(
    "obf-sub", cl::init(false), cl::Hidden,
    cl::desc("Enable Instruction Substitution"));

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION, 
        "MyObfuscator", 
        LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            
            // 1. 註冊手動管線解析名稱（-passes=...）
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name.equals("stringobfuscation")) { 
                        FPM.addPass(StringObfuscation());
                        return true;
                    }
                    if (Name.equals("substitution")) { 
                        FPM.addPass(Substitution());
                        return true;
                    }
                    return false;
                });
                
            // 2. 註冊自動擴充點（例如在管線最後自動根據開關執行）
            PB.registerOptimizerLastEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {
                    // 如果使用者在指令下了 -obf-string，就把這個 Pass 塞進管線
                    if (EnableAllObf || EnableStringObf) {
                        MPM.addPass(createModuleToFunctionPassAdaptor(StringObfuscation()));
                    }
                    // 如果使用者下了 -obf-sub，就塞這個
                    if (EnableAllObf || EnableSubstitution) {
                        MPM.addPass(createModuleToFunctionPassAdaptor(Substitution()));
                    }
                });
        }
    };
}