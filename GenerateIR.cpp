#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include <cassert>
#include <iostream>
#include <string>
#include <system_error>

namespace {

llvm::raw_fd_ostream &out_stream = llvm::outs();
llvm::raw_fd_ostream &err_stream = llvm::errs();

} // namespace

constexpr unsigned SMALL_ARITY = 4;

llvm::Function *
DeclareFunction(llvm::Module &module, const llvm::Twine &func_name,
                llvm::Type *ret_type,
                const llvm::SmallVector<llvm::Type *, SMALL_ARITY> &arg_types) {
    bool isVarArg = false;
    auto func_linkage = llvm::Function::ExternalLinkage;

    llvm::FunctionType *func_type =
        llvm::FunctionType::get(ret_type, arg_types, isVarArg);

    return llvm::Function::Create(func_type, func_linkage, func_name, module);
}

llvm::Function *DefineFactorial(llvm::Module &module) {
    llvm::LLVMContext &context = module.getContext();

    // Declare functions
    llvm::Type *long_type = llvm::Type::getInt64Ty(context);

    llvm::Function *factorial =
        DeclareFunction(module, "factorial", long_type, {long_type});

    // Insert instructions to function
    llvm::IRBuilder<> inst_builder{context};
    llvm::Argument *factorial_arg = factorial->getArg(0);

    llvm::BasicBlock *entry_block =
        llvm::BasicBlock::Create(context, "entry", factorial);
    llvm::BasicBlock *early_ret_block =
        llvm::BasicBlock::Create(context, "early_ret", factorial);
    llvm::BasicBlock *recursive_block =
        llvm::BasicBlock::Create(context, "recursive", factorial);

    llvm::Constant *zero_int64 =
        llvm::ConstantInt::get(long_type, 0, /*is_signed*/ true);
    llvm::Constant *one_int64 =
        llvm::ConstantInt::get(long_type, 1, /*is_signed*/ true);

    { // Base case
        inst_builder.SetInsertPoint(entry_block);
        llvm::Value *base_condition =
            inst_builder.CreateICmpEQ(factorial_arg, zero_int64);
        inst_builder.CreateCondBr(base_condition, early_ret_block,
                                  recursive_block);
    }

    { // Early Ret block
        inst_builder.SetInsertPoint(early_ret_block);
        llvm::Constant *one_int64 =
            llvm::ConstantInt::get(long_type, 1, /*is_signed*/ true);
        inst_builder.CreateRet(one_int64);
    }

    { // Recursive case
        inst_builder.SetInsertPoint(recursive_block);
        llvm::Value *decremented_arg =
            inst_builder.CreateSub(factorial_arg, one_int64);
        llvm::CallInst *recursive_call =
            inst_builder.CreateCall(factorial, decremented_arg);
        llvm::Value *ret_val =
            inst_builder.CreateMul(factorial_arg, recursive_call);
        inst_builder.CreateRet(ret_val);
    }

    assert(!llvm::verifyFunction(*factorial));
    return factorial;
}

llvm::Function *DefineMain(llvm::Module &module, llvm::Function *factorial) {
    llvm::LLVMContext &context = module.getContext();

    // Declare main
    llvm::Type *int_type = llvm::Type::getInt32Ty(context);

    llvm::Function *main_func =
        DeclareFunction(module, "main", int_type, {/*void*/});

    // Define main
    llvm::IRBuilder<> inst_builder{context};
    llvm::BasicBlock *entry_block =
        llvm::BasicBlock::Create(context, "entry", main_func);

    llvm::Type *long_type = llvm::Type::getInt64Ty(context);

    inst_builder.SetInsertPoint(entry_block);

    llvm::Constant *expected_result =
        llvm::ConstantInt::get(long_type, 120, true);

    llvm::Constant *arg = llvm::ConstantInt::get(long_type, 5, true);
    llvm::Value *calculated_result = inst_builder.CreateCall(factorial, arg);

    llvm::Value *cmp_result =
        inst_builder.CreateICmpEQ(calculated_result, expected_result);

    llvm::Value *ret_val = inst_builder.CreateCast(llvm::Instruction::ZExt,
                                                   cmp_result, int_type, "");

    inst_builder.CreateRet(ret_val);

    assert(!llvm::verifyFunction(*main_func, &err_stream));
    return main_func;
}

int main() {
    // Setup the module
    llvm::LLVMContext context{};
    llvm::Module module{"factorial", context};
    const auto default_triple = llvm::sys::getDefaultTargetTriple();
    module.setTargetTriple(default_triple);

    // Define factorial function
    llvm::Function *factorial = DefineFactorial(module);
    DefineMain(module, factorial);

    // Create target machine object
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();

    std::string error;
    const llvm::Target *registered_target =
        llvm::TargetRegistry::lookupTarget(default_triple, error);
    if (registered_target == nullptr) {
        err_stream << "Lookup error " << error << "\n";
        return 1;
    }

    llvm::TargetOptions opt{};
    llvm::Optional<llvm::Reloc::Model> rm{llvm::Reloc::PIC_};
    std::unique_ptr<llvm::TargetMachine> target_machine{
        registered_target->createTargetMachine(default_triple, "generic", "",
                                               opt, rm)};
    if (!target_machine) {
        err_stream << "Failed to create target machine from registry\n";
        return 1;
    }

    std::cout << "DataLayout "
              << target_machine->createDataLayout().getStringRepresentation()
              << std::endl;

    llvm::SmallString<128u> buffer;
    llvm::raw_svector_ostream output{buffer};
    llvm::legacy::PassManager pass;
    bool pass_manager_err = target_machine->addPassesToEmitFile(
        pass, output, nullptr, llvm::CGFT_ObjectFile);
    if (pass_manager_err) {
        err_stream << "Failed to generate object code\n";
        return 1;
    }

    module.setDataLayout(target_machine->createDataLayout());
    assert(!llvm::verifyModule(module, &err_stream));
    module.print(llvm::outs(), nullptr);
    pass.run(module);
}
