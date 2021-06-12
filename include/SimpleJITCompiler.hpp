#ifndef SIMPLE_JIT_COMPILER_HPP_
#define SIMPLE_JIT_COMPILER_HPP_

#include "DefaultTarget.hpp"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/Mangling.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Support/Error.h"
#include <memory>
#include <string>
#include <string_view>

class SimpleJITCompiler
{
  public:
    SimpleJITCompiler();
    ~SimpleJITCompiler();

    llvm::Error add(std::string_view module_name,
                    std::unique_ptr<llvm::Module> module,
                    std::unique_ptr<llvm::LLVMContext> context);

    llvm::Expected<llvm::JITEvaluatedSymbol>
    lookup(std::string_view module_name, std::string_view symbol_name);

  private:
    llvm::orc::ExecutionSession execution_session_;
    llvm::orc::RTDyldObjectLinkingLayer object_layer_;
    llvm::orc::IRCompileLayer compile_layer_;
    llvm::orc::MangleAndInterner mangler_;
};

SimpleJITCompiler::SimpleJITCompiler()
    : execution_session_{},
      object_layer_{execution_session_, []()
                    { return std::make_unique<llvm::SectionMemoryManager>(); }},
      compile_layer_{execution_session_, object_layer_,
                     std::make_unique<llvm::orc::SimpleCompiler>(
                         *GetDefaultTargetMachine())},
      mangler_{execution_session_, GetDefaultDataLayout()}
{
}

SimpleJITCompiler::~SimpleJITCompiler()
{
    auto err = execution_session_.endSession();
    if (err)
        execution_session_.reportError(std::move(err));
}

llvm::Error
SimpleJITCompiler::add(std::string_view module_name,
                       std::unique_ptr<llvm::Module> module,
                       std::unique_ptr<llvm::LLVMContext> context)
{
    llvm::orc::JITDylib &added_module =
        execution_session_.createBareJITDylib(std::string{module_name});

    return compile_layer_.add(
        added_module,
        llvm::orc::ThreadSafeModule{std::move(module), std::move(context)});
}

llvm::Expected<llvm::JITEvaluatedSymbol>
SimpleJITCompiler::lookup(std::string_view module_name,
                          std::string_view symbol_name)
{
    llvm::orc::JITDylib *dylib =
        execution_session_.getJITDylibByName(module_name);
    return execution_session_.lookup({dylib}, mangler_(symbol_name));
}

#endif // SIMPLE_JIT_COMPILER_HPP_
