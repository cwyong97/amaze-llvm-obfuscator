#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"

// 引入所有 Pass 的標頭檔
#include "StringObfuscation.h" 
#include "ConstantObfuscation.h"
#include "Substitution.h"
#include "Utils/ConstantAnalyzer.h"

using namespace llvm;

// --- 在這裡定義開關 (CommandLine Flags) ---
static cl::opt<bool> EnableAllObf(
    "obf-all", cl::init(false), cl::Hidden,
    cl::desc("Enable all obfuscation passes"));

static cl::opt<bool> EnableStringObf(
    "obf-string", cl::init(false), cl::Hidden,
    cl::desc("Enable String Obfuscation"));

static cl::opt<bool> EnableConstantObf(
    "obf-const", cl::init(false), cl::Hidden,
    cl::desc("Enable Constant Obfuscation"));

static cl::opt<bool> EnableSubstitution(
    "obf-sub", cl::init(false), cl::Hidden,
    cl::desc("Enable Instruction Substitution"));

static cl::opt<bool> EnableConstantTagging(
    "obf-tag", cl::init(false), cl::Hidden,
    cl::desc("Enable Constant Tagging (Scans and tags duplicate constants for targeted obfuscation)"));

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
                    if (Name.equals("constantobfuscation")) { 
                        FPM.addPass(ConstantObfuscation());
                        return true;
                    }
                    if (Name.equals("substitution")) { 
                        FPM.addPass(Substitution());
                        return true;
                    }
                    return false;
                });
                
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name.equals("stringobfuscation")) { 
                        MPM.addPass(StringObfuscation());
                        return true;
                    }
                    if (Name.equals("constanttagger")) { 
                        MPM.addPass(ConstantTaggingPass());
                        return true;
                    }
                    return false;
                });
                
            // 2. 註冊自動擴充點
            PB.registerOptimizerLastEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {
                    if (EnableAllObf || EnableStringObf) {
                        MPM.addPass(StringObfuscation());
                    }
                    if (EnableAllObf || EnableConstantTagging || EnableConstantObf) {
                        MPM.addPass(ConstantTaggingPass());
                    }
                    if (EnableAllObf || EnableConstantObf) {
                        MPM.addPass(createModuleToFunctionPassAdaptor(ConstantObfuscation()));
                    }
                    if (EnableAllObf || EnableSubstitution) {
                        MPM.addPass(createModuleToFunctionPassAdaptor(Substitution()));
                    }
                });
        }
    };
}