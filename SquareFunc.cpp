#include "DefaultTarget.hpp"
#include "SimpleJITCompiler.hpp"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
#include <memory>
#include <utility>

std::unique_ptr<llvm::Module> DefineSquare(llvm::LLVMContext &context) {
    auto module = std::make_unique<llvm::Module>("square", context);
    module->setTargetTriple(GetDefaultTargetTriple().str());
    module->setDataLayout(GetDefaultDataLayout());

    {
        // Declare function
        llvm::Type *double_type = llvm::Type::getDoubleTy(context);
        llvm::FunctionType *func_type = llvm::FunctionType::get(
            double_type, {double_type}, /*isVarArg*/ false);

        llvm::Function *square_func = llvm::Function::Create(
            func_type, llvm::Function::ExternalLinkage, "square", *module);

        // Add instructions
        llvm::IRBuilder<> ir_builder{context};
        llvm::BasicBlock *entry_block =
            llvm::BasicBlock::Create(context, "", square_func);

        ir_builder.SetInsertPoint(entry_block);
        llvm::Argument *func_arg = square_func->getArg(0);
        llvm::Value *result = ir_builder.CreateFMul(func_arg, func_arg);
        ir_builder.CreateRet(result);
    }

    return module;
}

int main() {

    // Create module
    auto context = std::make_unique<llvm::LLVMContext>();
    std::unique_ptr<llvm::Module> square_module = DefineSquare(*context);
    llvm::verifyModule(*square_module, &llvm::errs());
    square_module->print(llvm::outs(), nullptr);

    // Create JIT'd program
    std::string module_name = square_module->getName().str();
    const char *symbol_name = "square";

    SimpleJITCompiler compiler{};
    auto err =
        compiler.add(module_name, std::move(square_module), std::move(context));
    if (err) {
        std::cerr << "Failed to add module\n";
        return 1;
    }

    llvm::Expected<llvm::JITEvaluatedSymbol> square_symbol =
        compiler.lookup(module_name, symbol_name);

    if (!square_symbol) {
        std::cerr << "Failed to get square function symbol\n";
        return 1;
    }

    using square_func_t = double (*)(double);
    square_func_t square_func = llvm::jitTargetAddressToPointer<square_func_t>(
        square_symbol->getAddress());

    std::cout << "square(10) = " << square_func(10.0) << std::endl;
}
