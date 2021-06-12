#ifndef DEFAULT_TARGETS_HPP_
#define DEFAULT_TARGETS_HPP_

#include "llvm/Support/Host.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

void InitializeAllTargets() {
    static const bool initialized = []() {
        llvm::InitializeAllTargets();
        llvm::InitializeAllTargetMCs();
        llvm::InitializeAllAsmPrinters();
        llvm::InitializeAllAsmParsers();
        return true;
    }();

    return (void)initialized;
}

const llvm::Triple &GetDefaultTargetTriple() {
    static const llvm::Triple default_triple{
        llvm::sys::getDefaultTargetTriple()};
    return default_triple;
}

llvm::TargetMachine *GetDefaultTargetMachine() {
    using OwnedTargetMachine = std::unique_ptr<llvm::TargetMachine>;

    static OwnedTargetMachine default_target_machine = []() {
        InitializeAllTargets();

        const llvm::Triple &default_triple = GetDefaultTargetTriple();

        std::string lookup_error;
        const llvm::Target *registered_target =
            llvm::TargetRegistry::lookupTarget(default_triple.str(),
                                               lookup_error);
        if (registered_target == nullptr)
            return OwnedTargetMachine{};

        llvm::TargetOptions opt{};
        llvm::Optional<llvm::Reloc::Model> reloc_model{llvm::Reloc::PIC_};

        return OwnedTargetMachine{registered_target->createTargetMachine(
            default_triple.str(), "generic", "", opt, reloc_model)};
    }();

    return default_target_machine.get();
}

const llvm::DataLayout &GetDefaultDataLayout() {
    static const llvm::DataLayout &default_layout = []() {
        llvm::TargetMachine *default_target_machine = GetDefaultTargetMachine();
        return default_target_machine->createDataLayout();
    }();

    return default_layout;
}

#endif // DEFAULT_TARGETS_HPP_
