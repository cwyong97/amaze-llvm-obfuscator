// header file
#include "StringObfuscation.h"

// LLVM 工具
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/Intrinsics.h"

// 環境綁定工具 (MagicValue)
#include "EnvBinding.h"

// C++ 標準庫
#include <cstdint>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <numeric>
#include <algorithm>

using namespace llvm;

static cl::opt<int> StrChance(
    "str-chance", cl::init(100), cl::Hidden,
    cl::desc("The probability (0-100) of applying string obfuscation to an instruction"));

static cl::opt<int> StrRound(
    "str-round", cl::init(1), cl::Hidden,
    cl::desc("The number of rounds of string obfuscation to apply"));

static cl::opt<std::string> StrMagicSymbol(
    "str-magic-symbol", cl::init(""), cl::Hidden,
    cl::desc("Override the target linker magic symbol name (e.g. __mh_dylib_header)"));

static cl::opt<unsigned long long> StrMagicValue(
    "str-magic-value", cl::init(0), cl::Hidden,
    cl::desc("Override the target linker magic value (e.g. 0xFEEDFACF)"));

namespace {
using RNG = std::mt19937;

// XTEA Encryption (32 rounds)
void xtea_encrypt(uint32_t num_rounds, uint32_t v[2], uint32_t const key[4]) {
    uint32_t v0 = v[0], v1 = v[1];
    uint32_t sum = 0;
    uint32_t delta = 0x9E3779B9;
    for (uint32_t i = 0; i < num_rounds; i++) {
        v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
        sum += delta;
        v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
    }
    v[0] = v0; v[1] = v1;
}

bool isEscaping(Value *V, std::unordered_set<Value*> &visited) {
    if (visited.count(V)) return false;
    visited.insert(V);

    for (User *U : V->users()) {
        if (isa<ReturnInst>(U)) {
            return true;
        }
        if (auto *SI = dyn_cast<StoreInst>(U)) {
            if (SI->getValueOperand() == V) {
                return true;
            }
        }
        if (isa<GetElementPtrInst>(U) || isa<BitCastInst>(U)) {
            if (isEscaping(U, visited)) {
                return true;
            }
        }
    }
    return false;
}

bool isEscaped(GlobalVariable *GV) {
    std::unordered_set<Value*> visited;
    return isEscaping(GV, visited);
}

// Outlined XTEA decryption stub
Function *getOrCreateXteaDecryptStub(Module &M) {
    LLVMContext &Ctx = M.getContext();
    std::string StubName = "__amaze_xtea_decrypt_stub";
    Function *Stub = M.getFunction(StubName);
    if (Stub) return Stub;

    // Type signature: void(ptr noalias, ptr noalias readonly, ptr noalias readonly, i32)
    Type *ArgTys[] = {
        PointerType::get(Ctx, 0), // buf
        PointerType::get(Ctx, 0), // enc_str
        PointerType::get(Ctx, 0), // key
        Type::getInt32Ty(Ctx)     // len
    };
    FunctionType *FTy = FunctionType::get(Type::getVoidTy(Ctx), ArgTys, false);
    Stub = Function::Create(FTy, Function::InternalLinkage, StubName, &M);

    // Apply attributes: noinline, nounwind
    Stub->addFnAttr(Attribute::NoInline);
    Stub->addFnAttr(Attribute::NoUnwind);

    // Param 0: buf (noalias)
    Stub->addParamAttr(0, Attribute::NoAlias);
    // Param 1: enc_str (noalias, readonly)
    Stub->addParamAttr(1, Attribute::NoAlias);
    Stub->addParamAttr(1, Attribute::ReadOnly);
    // Param 2: key (noalias, readonly)
    Stub->addParamAttr(2, Attribute::NoAlias);
    Stub->addParamAttr(2, Attribute::ReadOnly);

    // Create entry block
    BasicBlock *Entry = BasicBlock::Create(Ctx, "entry", Stub);
    IRBuilder<> Builder(Entry);

    Argument *BufArg = Stub->getArg(0);
    Argument *EncArg = Stub->getArg(1);
    Argument *KeyArg = Stub->getArg(2);
    Argument *LenArg = Stub->getArg(3);

    BufArg->setName("buf");
    EncArg->setName("enc_str");
    KeyArg->setName("key");
    LenArg->setName("len");

    // Load key segments: k0, k1, k2, k3
    Value *K0Ptr = Builder.CreateInBoundsGEP(Type::getInt32Ty(Ctx), KeyArg, Builder.getInt32(0));
    Value *K1Ptr = Builder.CreateInBoundsGEP(Type::getInt32Ty(Ctx), KeyArg, Builder.getInt32(1));
    Value *K2Ptr = Builder.CreateInBoundsGEP(Type::getInt32Ty(Ctx), KeyArg, Builder.getInt32(2));
    Value *K3Ptr = Builder.CreateInBoundsGEP(Type::getInt32Ty(Ctx), KeyArg, Builder.getInt32(3));

    Value *K0 = Builder.CreateLoad(Type::getInt32Ty(Ctx), K0Ptr, "k0");
    Value *K1 = Builder.CreateLoad(Type::getInt32Ty(Ctx), K1Ptr, "k1");
    Value *K2 = Builder.CreateLoad(Type::getInt32Ty(Ctx), K2Ptr, "k2");
    Value *K3 = Builder.CreateLoad(Type::getInt32Ty(Ctx), K3Ptr, "k3");

    Value *keys[4] = {K0, K1, K2, K3};

    // Tag keys inside stub for Constant / MBA obfuscation downstream
    std::vector<Value*> keys_to_tag = {K0, K1, K2, K3};
    for (Value *val : keys_to_tag) {
        if (auto *Inst = dyn_cast<Instruction>(val)) {
            LLVMContext &Ctx = Inst->getContext();
            MDNode *MD = MDNode::get(Ctx, {});
            Inst->setMetadata("amaze.target.constant", MD);
        }
    }

    // Allocate loop counter on stack
    AllocaInst *CounterAlloc = Builder.CreateAlloca(Type::getInt32Ty(Ctx), nullptr, "loop_counter");
    Builder.CreateStore(Builder.getInt32(0), CounterAlloc);

    BasicBlock *LoopHeader = BasicBlock::Create(Ctx, "loop.header", Stub);
    BasicBlock *LoopBody   = BasicBlock::Create(Ctx, "loop.body", Stub);
    BasicBlock *LoopEnd    = BasicBlock::Create(Ctx, "loop.end", Stub);

    Builder.CreateBr(LoopHeader);

    // Loop Header
    IRBuilder<> HeaderBuilder(LoopHeader);
    Value *CurrIdx = HeaderBuilder.CreateLoad(Type::getInt32Ty(Ctx), CounterAlloc, "curr_idx");
    Value *LoopCond = HeaderBuilder.CreateICmpULT(CurrIdx, LenArg, "loop_cond");
    HeaderBuilder.CreateCondBr(LoopCond, LoopBody, LoopEnd);

    // Loop Body
    IRBuilder<> BodyBuilder(LoopBody);
    Value *Idx = BodyBuilder.CreateLoad(Type::getInt32Ty(Ctx), CounterAlloc, "idx");

    // Load encrypted chunk
    Value *EncPtr = BodyBuilder.CreateInBoundsGEP(Type::getInt64Ty(Ctx), EncArg, Idx);
    Value *CipherVal = BodyBuilder.CreateLoad(Type::getInt64Ty(Ctx), EncPtr, "cipher_val");

    // Split 64-bit CipherVal into V0 (low 32 bits) and V1 (high 32 bits)
    Value *V0 = BodyBuilder.CreateTrunc(CipherVal, BodyBuilder.getInt32Ty(), "v0");
    Value *CipherValHigh = BodyBuilder.CreateLShr(CipherVal, ConstantInt::get(Type::getInt64Ty(Ctx), 32));
    Value *V1 = BodyBuilder.CreateTrunc(CipherValHigh, BodyBuilder.getInt32Ty(), "v1");

    // Unrolled XTEA decryption rounds (32 rounds)
    uint32_t delta = 0x9E3779B9;
    uint32_t num_rounds = 32;
    uint32_t sum = delta * num_rounds;

    for (uint32_t r = 0; r < num_rounds; ++r) {
        // v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
        {
            Value *shift4 = BodyBuilder.CreateShl(V0, 4);
            Value *shift5 = BodyBuilder.CreateLShr(V0, 5);
            Value *xor_shift = BodyBuilder.CreateXor(shift4, shift5);
            Value *add_v0 = BodyBuilder.CreateAdd(xor_shift, V0);

            uint32_t key_idx = (sum >> 11) & 3;
            Value *key_val = keys[key_idx];
            Value *sum_plus_key = BodyBuilder.CreateAdd(ConstantInt::get(Type::getInt32Ty(Ctx), sum), key_val);

            Value *final_xor = BodyBuilder.CreateXor(add_v0, sum_plus_key);
            V1 = BodyBuilder.CreateSub(V1, final_xor, "v1_sub");
        }

        sum -= delta;

        // v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
        {
            Value *shift4 = BodyBuilder.CreateShl(V1, 4);
            Value *shift5 = BodyBuilder.CreateLShr(V1, 5);
            Value *xor_shift = BodyBuilder.CreateXor(shift4, shift5);
            Value *add_v1 = BodyBuilder.CreateAdd(xor_shift, V1);

            uint32_t key_idx = sum & 3;
            Value *key_val = keys[key_idx];
            Value *sum_plus_key = BodyBuilder.CreateAdd(ConstantInt::get(Type::getInt32Ty(Ctx), sum), key_val);

            Value *final_xor = BodyBuilder.CreateXor(add_v1, sum_plus_key);
            V0 = BodyBuilder.CreateSub(V0, final_xor, "v0_sub");
        }
    }

    // Recombine V0 and V1 back into a 64-bit DecryptedChunk
    Value *V0_64 = BodyBuilder.CreateZExt(V0, Type::getInt64Ty(Ctx));
    Value *V1_64 = BodyBuilder.CreateZExt(V1, Type::getInt64Ty(Ctx));
    Value *V1_shifted = BodyBuilder.CreateShl(V1_64, 32);
    Value *DecryptedChunk = BodyBuilder.CreateOr(V1_shifted, V0_64, "decrypted_chunk");

    // Store decrypted chunk back to buf[idx]
    Value *StrChunkPtr = BodyBuilder.CreateInBoundsGEP(Type::getInt64Ty(Ctx), BufArg, Idx);
    StoreInst *StoreDec = BodyBuilder.CreateStore(DecryptedChunk, StrChunkPtr);
    StoreDec->setVolatile(true);
    StoreDec->setAlignment(Align(8)); // i64 stride = 8 bytes; odd indices are only 8-byte aligned

    // Increment index counter
    Value *NextIdx = BodyBuilder.CreateAdd(Idx, BodyBuilder.getInt32(1), "next_idx");
    BodyBuilder.CreateStore(NextIdx, CounterAlloc);

    BodyBuilder.CreateBr(LoopHeader);

    // Loop End block
    IRBuilder<> EndBuilder(LoopEnd);
    EndBuilder.CreateRetVoid();

    return Stub;
}

AllocaInst *generateDecryptedString(
    Function &F,
    IRBuilder<> &Builder,
    GlobalVariable *GV,
    std::vector<std::pair<AllocaInst*, size_t>> &StrAllocSizes,
    BasicBlock *BlockC,
    RNG &gen) 
{
    LLVMContext &Ctx = F.getContext();

    if (!GV->hasInitializer()) return nullptr;
    ConstantDataArray *CDA = dyn_cast<ConstantDataArray>(GV->getInitializer());
    if (!CDA) return nullptr;

    StringRef StrVal = CDA->getAsString();
    size_t len = StrVal.size() + 1;
    size_t chunkCount = (len + 7) / 8;

    IRBuilder<> EntryBuilder(&F.getEntryBlock(), F.getEntryBlock().getFirstInsertionPt());
    ArrayType *ArrTy = ArrayType::get(EntryBuilder.getInt64Ty(), chunkCount);
    AllocaInst *StrAlloc = EntryBuilder.CreateAlloca(ArrTy, nullptr, "str_alloc_long");
    StrAlloc->setAlignment(Align(16));
    StrAllocSizes.push_back({StrAlloc, chunkCount});

    EnvConfig config = EnvBinding::getTargetConfig(*F.getParent());
    if (!StrMagicSymbol.empty()) {
        config.MagicSymbolName = StrMagicSymbol;
        config.isValid = true;
    }
    if (StrMagicValue != 0) {
        config.MagicValue = StrMagicValue;
        config.isValid = true;
    }

    if (!config.isValid) {
        static bool warned = false;
        if (!warned) {
            errs() << "\033[1;33m[AmaZe Warning]\033[0m Target platform is unsupported for environment binding.\n"
                   << "                Fallback to compile-time randomized security key.\n";
            warned = true;
        }
        std::uniform_int_distribution<uint64_t> fallbackKeyDist(0, ~0ULL);
        config.MagicValue = fallbackKeyDist(gen);
    }

    uint64_t M = config.MagicValue;
    std::uniform_int_distribution<uint64_t> keyDist(0, ~0ULL);
    uint64_t K = keyDist(gen); // compile-time base key

    // Derive compile-time XTEA key from K
    uint32_t xtea_k[4];
    xtea_k[0] = static_cast<uint32_t>(K & 0xFFFFFFFF);
    xtea_k[1] = static_cast<uint32_t>(K >> 32);
    xtea_k[2] = xtea_k[0] ^ 0x3F4B5C6D;
    xtea_k[3] = xtea_k[1] ^ 0x7E8D9C0A;

    std::vector<uint64_t> C(chunkCount);
    for (size_t c = 0; c < chunkCount; ++c) {
        uint64_t chunkInt = 0;
        for (size_t i = 0; i < 8; ++i) {
            size_t charIdx = c * 8 + i;
            uint8_t charVal = 0;
            if (charIdx < len - 1) {
                charVal = StrVal[charIdx];
            } else if (charIdx == len - 1) {
                charVal = 0;
            } else {
                std::uniform_int_distribution<uint16_t> byteDist(1, 255);
                charVal = static_cast<uint8_t>(byteDist(gen));
            }
            chunkInt |= (static_cast<uint64_t>(charVal) << (i * 8));
        }

        // Encrypt chunkInt using XTEA
        uint32_t v[2];
        v[0] = static_cast<uint32_t>(chunkInt & 0xFFFFFFFF);
        v[1] = static_cast<uint32_t>(chunkInt >> 32);

        xtea_encrypt(32, v, xtea_k);

        C[c] = (static_cast<uint64_t>(v[1]) << 32) | v[0];
    }

    std::string EncGVName = ("_enc_str_" + GV->getName()).str();
    GlobalVariable *EncGV = F.getParent()->getGlobalVariable(EncGVName);
    if (!EncGV) {
        ArrayType *EncArrTy = ArrayType::get(Builder.getInt64Ty(), chunkCount);
        std::vector<Constant*> EncValues;
        for (uint64_t val : C) {
            EncValues.push_back(ConstantInt::get(Builder.getInt64Ty(), val));
        }
        Constant *EncInit = ConstantArray::get(EncArrTy, EncValues);
        EncGV = new GlobalVariable(
            *F.getParent(),
            EncArrTy,
            true,
            GlobalValue::InternalLinkage,
            EncInit,
            EncGVName
        );
    }

    GlobalVariable *MagicGV = EnvBinding::getOrCreateMagicVariable(*F.getParent(), config, Ctx);

    Value *LoadM = nullptr;
    if (config.isValid) {
        LoadM = Builder.CreateZExtOrTrunc(
            Builder.CreateLoad(MagicGV->getValueType(), MagicGV),
            Builder.getInt64Ty()
        );
    } else {
        LoadM = ConstantInt::get(Builder.getInt64Ty(), config.MagicValue);
    }

    uint64_t K_xor_M = K ^ M;
    Value *DecryptionKey = Builder.CreateXor(LoadM, ConstantInt::get(Builder.getInt64Ty(), K_xor_M));

    // Derive 128-bit XTEA key from DecryptionKey in IR
    Value *K0 = Builder.CreateTrunc(DecryptionKey, Builder.getInt32Ty(), "xtea_k0");
    Value *K1_64 = Builder.CreateLShr(DecryptionKey, ConstantInt::get(Builder.getInt64Ty(), 32));
    Value *K1 = Builder.CreateTrunc(K1_64, Builder.getInt32Ty(), "xtea_k1");
    Value *K2 = Builder.CreateXor(K0, ConstantInt::get(Builder.getInt32Ty(), 0x3F4B5C6D), "xtea_k2");
    Value *K3 = Builder.CreateXor(K1, ConstantInt::get(Builder.getInt32Ty(), 0x7E8D9C0A), "xtea_k3");

    // Allocate dynamic keys [4 x i32] on caller's stack
    ArrayType *KeyArrTy = ArrayType::get(Builder.getInt32Ty(), 4);
    AllocaInst *KeyAlloc = EntryBuilder.CreateAlloca(KeyArrTy, nullptr, "xtea_keys");
    KeyAlloc->setAlignment(Align(16));

    // Store K0..K3 to KeyAlloc
    Value *Idx0[] = {Builder.getInt32(0), Builder.getInt32(0)};
    Value *K0Ptr = Builder.CreateInBoundsGEP(KeyArrTy, KeyAlloc, Idx0);
    Builder.CreateStore(K0, K0Ptr);

    Value *Idx1[] = {Builder.getInt32(0), Builder.getInt32(1)};
    Value *K1Ptr = Builder.CreateInBoundsGEP(KeyArrTy, KeyAlloc, Idx1);
    Builder.CreateStore(K1, K1Ptr);

    Value *Idx2[] = {Builder.getInt32(0), Builder.getInt32(2)};
    Value *K2Ptr = Builder.CreateInBoundsGEP(KeyArrTy, KeyAlloc, Idx2);
    Builder.CreateStore(K2, K2Ptr);

    Value *Idx3[] = {Builder.getInt32(0), Builder.getInt32(3)};
    Value *K3Ptr = Builder.CreateInBoundsGEP(KeyArrTy, KeyAlloc, Idx3);
    Builder.CreateStore(K3, K3Ptr);

    std::vector<Value*> keys_to_tag = {DecryptionKey, K0, K1, K2, K3};
    for (Value *val : keys_to_tag) {
        if (auto *Inst = dyn_cast<Instruction>(val)) {
            LLVMContext &Ctx = Inst->getContext();
            MDNode *MD = MDNode::get(Ctx, {});
            Inst->setMetadata("amaze.target.constant", MD);
        }
    }

    // Call outlined helper stub
    Function *Stub = getOrCreateXteaDecryptStub(*F.getParent());
    Value *BufPtr = Builder.CreatePointerCast(StrAlloc, PointerType::get(Ctx, 0));
    Value *EncPtr = Builder.CreatePointerCast(EncGV, PointerType::get(Ctx, 0));
    Value *KeyPtr = Builder.CreatePointerCast(KeyAlloc, PointerType::get(Ctx, 0));
    Value *LenVal = Builder.getInt32(chunkCount);

    Value *CallArgs[] = {BufPtr, EncPtr, KeyPtr, LenVal};
    Builder.CreateCall(Stub->getFunctionType(), Stub, CallArgs);

    return StrAlloc;
}

bool applyOneRound(Function &F, RNG &gen, std::uniform_int_distribution<> &dist) {
    bool modified = false;
    LLVMContext &Ctx = F.getContext();
    
    std::vector<std::pair<Instruction*, Use*>> ToReplace;

    for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
            for (Use &Op : I.operands()) {
                auto *GV = dyn_cast<GlobalVariable>(Op.get());
                if (!GV || !GV->isConstant() || !GV->hasInitializer()) continue;
                StringRef GVName = GV->getName();
                if (GVName.startswith("_ZTS") || GVName.startswith("_ZTI") || GVName.startswith("llvm.")) {
                    continue;
                }

                auto *CDA = dyn_cast<ConstantDataArray>(GV->getInitializer());
                if (!CDA || !CDA->isString()) continue;

                if (dist(gen) > StrChance) continue;

                if (isEscaped(GV)) continue;

                ToReplace.push_back({&I, &Op});
            }
        }
    }

    if (!ToReplace.empty()) {
        BasicBlock *BlockA = &F.getEntryBlock();
        Instruction *SplitPt = nullptr;
        for (Instruction &I : *BlockA) {
            if (isa<PHINode>(&I) || isa<AllocaInst>(&I)) {
                continue;
            }
            if (auto *CI = dyn_cast<CallInst>(&I)) {
                if (Function *Callee = CI->getCalledFunction()) {
                    if (Callee->isIntrinsic()) {
                        unsigned iid = Callee->getIntrinsicID();
                        if (iid == Intrinsic::lifetime_start || iid == Intrinsic::lifetime_end ||
                            iid == Intrinsic::dbg_declare || iid == Intrinsic::dbg_value ||
                            iid == Intrinsic::dbg_label || iid == Intrinsic::dbg_addr) {
                            continue;
                        }
                    }
                }
            }
            SplitPt = &I;
            break;
        }
        if (!SplitPt) {
            SplitPt = BlockA->getTerminator();
        }
        BasicBlock *BlockC = BlockA->splitBasicBlock(SplitPt, "LazyDec.PrecedingBlockC");
        BasicBlock *BlockB = BasicBlock::Create(Ctx, "LazyDec.DecryptionBlockB", &F, BlockC);

        BlockA->getTerminator()->eraseFromParent();

        IRBuilder<> BuilderA(BlockA);
        BuilderA.CreateBr(BlockB);

        IRBuilder<> BuilderB(BlockB);
        std::unordered_map<GlobalVariable*, AllocaInst*> GlobalToAlloca;
        std::vector<std::pair<AllocaInst*, size_t>> StrAllocSizes;

        for (auto &Pair : ToReplace) {
            Instruction *Inst = Pair.first;
            Use *Op = Pair.second;
            auto *GV = cast<GlobalVariable>(Op->get());

            if (!GlobalToAlloca.count(GV)) {
                AllocaInst *StrAlloc = generateDecryptedString(F, BuilderB, GV, StrAllocSizes, BlockC, gen);
                if (!StrAlloc) continue;
                GlobalToAlloca[GV] = StrAlloc;
            }

            if (auto *CI = dyn_cast<CallInst>(Inst)) {
                CI->setTailCall(false);
            }
            IRBuilder<> InstBuilder(Inst);
            Value *CastedAlloc = InstBuilder.CreatePointerCast(GlobalToAlloca[GV], GV->getType(), "str_ptr_cast");
            Op->set(CastedAlloc);
        }

        BuilderB.CreateBr(BlockC);

        modified = true;

        std::vector<GlobalVariable*> DeadGlobals;
        for (GlobalVariable &GV : F.getParent()->globals()) {
            if (GV.use_empty() && GV.hasInitializer() && isa<ConstantDataArray>(GV.getInitializer())) {
                DeadGlobals.push_back(&GV);
            }
        }
        for (GlobalVariable *GV : DeadGlobals) {
            GV->eraseFromParent();
        }

        std::vector<ReturnInst*> Returns;
        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                if (auto *RI = dyn_cast<ReturnInst>(&I)) {
                    Returns.push_back(RI);
                }
            }
        }
        for (ReturnInst *RI : Returns) {
            IRBuilder<> ScrubBuilder(RI);
            for (auto &Pair : StrAllocSizes) {
                AllocaInst *StrAlloc = Pair.first;
                size_t chunkCount = Pair.second;

                Value *ScrubPtr = ScrubBuilder.CreatePointerCast(StrAlloc, ScrubBuilder.getPtrTy());

                ScrubBuilder.CreateMemSet(
                    ScrubPtr,
                    ScrubBuilder.getInt8(0),
                    chunkCount * 8,
                    Align(16)
                );
            }
        }
    }

    return modified;
}
} // end anonymous namespace

PreservedAnalyses StringObfuscation::run(Module &M, ModuleAnalysisManager &AM) {
    bool modified = false;
    int rounds = StrRound;
    errs() << "Current string obfuscation rounds is set to: " << rounds << "\n";

    std::random_device rd;
    RNG gen(rd());
    std::uniform_int_distribution<> dist(0, 100);

    for (Function &F : M) {
        if (F.isDeclaration()) continue;
        if (F.getName().startswith("__amaze_")) continue;
        errs() << "<testmessage> Running StringObfuscation on function: " << F.getName() << "\n";
        for (int i = 0; i < rounds; ++i) {
            modified |= applyOneRound(F, gen, dist);
        }
    }
    return modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
