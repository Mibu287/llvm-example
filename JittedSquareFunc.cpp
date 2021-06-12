#include "CreateObjectFile.hpp"
#include "DefaultTarget.hpp"
#include "SimpleJITCompiler.hpp"
#include "utils.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
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

    assert(!llvm::verifyModule(*module, &llvm::errs()));
    return module;
}

llvm::Expected<uint64_t>
lookupSymbol(const llvm::object::ObjectFile &object_file,
             llvm::Twine raw_name) {
    // Get mangled name for default target
    std::string mangled_name;
    {
        llvm::raw_string_ostream mangled_name_stream{mangled_name};
        auto const &default_layout = GetDefaultDataLayout();

        llvm::Mangler::getNameWithPrefix(mangled_name_stream, raw_name,
                                         default_layout);
    }

    // Lookup symbol in object file
    for (const auto &symbol : object_file.symbols()) {
        llvm::StringRef symbol_name;
        {
            llvm::Expected<llvm::StringRef> expected_symbol_name =
                symbol.getName();
            if (!expected_symbol_name) {
                return expected_symbol_name.takeError();
            }
            symbol_name = *expected_symbol_name;
        }

        uint64_t symbol_address;
        {
            llvm::Expected<uint64_t> expected_symbol_address =
                symbol.getAddress();
            if (!expected_symbol_address) {
                return expected_symbol_address.takeError();
            }
            symbol_address = *expected_symbol_address;
        }

        if (symbol_name == mangled_name)
            return symbol_address;
    }

    return llvm::createStringError(std::error_code{}, "Name not found");
}

int main() {

    // Create module
    llvm::LLVMContext context{};
    std::unique_ptr<llvm::Module> square_module = DefineSquare(context);

    llvm::outs() << llvm::raw_ostream::GREEN << "Text representation of IR\n"
                 << llvm::raw_ostream::RESET;

    square_module->print(llvm::outs(), nullptr);

    // Create JIT'd program
    EXIT_ON_ERROR(OwningObjectFile, object_file,
                  createObjectFileFromModule(*square_module));

    // Lookup symbol in Jitted program
    EXIT_ON_ERROR(uint64_t, square_function,
                  lookupSymbol(*object_file.getBinary(), "square"));

    llvm::outs() << llvm::raw_ostream::MAGENTA
                 << "Address of square function: " << llvm::raw_ostream::RESET
                 << format_address(square_function) << '\n';
}
