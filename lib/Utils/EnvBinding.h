#pragma once
#include "llvm/IR/Module.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

namespace llvm {

struct EnvConfig {
    std::string MagicSymbolName;
    uint64_t MagicValue;
    bool isValid;
};

class EnvBinding {
public:
    static EnvConfig getTargetConfig(const llvm::Module &M) {
        llvm::Triple TargetTriple(M.getTargetTriple());
        EnvConfig config;
        config.isValid = true;

        if (TargetTriple.isOSWindows()) {
            config.MagicSymbolName = "__ImageBase";
            config.MagicValue = 0x5A4D; // PE Magic 'MZ'
        } else if (TargetTriple.isOSLinux()) {
            config.MagicSymbolName = "__ehdr_start";
            config.MagicValue = 0x464C457F; // ELF Magic '\x7FELF'
        } else if (TargetTriple.isMacOSX()) {
            config.MagicSymbolName = "__mh_execute_header";
            config.MagicValue = 0xFEEDFACF; // Mach-O 64-bit Magic
        } else {
            config.isValid = false;
            config.MagicSymbolName = "dummy_var";
            config.MagicValue = 0x12345678; // Dummy value
        }
        return config;
    }

    static GlobalVariable *getOrCreateMagicVariable(Module &M, const EnvConfig &config, LLVMContext &Ctx) {
        GlobalVariable *MagicGV = M.getGlobalVariable(config.MagicSymbolName);
        Triple TargetTriple(M.getTargetTriple());
        Type *ExpectedTy = TargetTriple.isOSWindows() ? Type::getInt16Ty(Ctx) : Type::getInt32Ty(Ctx);

        if (MagicGV) {
            if (MagicGV->getValueType() != ExpectedTy) {
                errs() << "\033[1;33m[EnvBinding Warning]\033[0m Linker symbol '" << config.MagicSymbolName 
                       << "' already exists but with a conflicting type size.\n"
                       << "                      Reusing original definition type to prevent ODR type conflicts.\n";
            }
            return MagicGV;
        }

        if (config.isValid) {
            MagicGV = new GlobalVariable(
                M,
                ExpectedTy, 
                true,
                GlobalValue::ExternalLinkage,
                nullptr, 
                config.MagicSymbolName
            );
        } else {
            MagicGV = new GlobalVariable(
                M,
                Type::getInt32Ty(Ctx),
                true,
                GlobalValue::InternalLinkage,
                ConstantInt::get(Type::getInt32Ty(Ctx), config.MagicValue),
                config.MagicSymbolName
            );
        }
        return MagicGV;
    }
};

} // namespace llvm