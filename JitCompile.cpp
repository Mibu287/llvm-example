#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/Mangling.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/SymbolStringPool.h"
#include "llvm/ExecutionEngine/Orc/TargetProcessControl.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
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
#include "llvm/Support/Error.h"
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
    inst_builder.CreateCondBr(base_condition, early_ret_block, recursive_block);
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

void initialize_all() {
  static const bool initialized = []() noexcept -> bool {
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();
    return true;
  }();

  return (void)initialized;
}

int main() {
  // Initialize compilation targets
  initialize_all();

  // Setup the module
  auto context = std::make_unique<llvm::LLVMContext>();
  auto module = std::make_unique<llvm::Module>("factorial", *context);
  const auto default_triple = llvm::sys::getDefaultTargetTriple();
  module->setTargetTriple(default_triple);

  // Define factorial function
  llvm::Function *factorial = DefineFactorial(*module);
  DefineMain(*module, factorial);

  // Setup JIT layers
  llvm::orc::JITTargetMachineBuilder jit_machine_builder{
      llvm::Triple{default_triple}};
  auto data_layout = jit_machine_builder.getDefaultDataLayoutForTarget();
  if (!data_layout) {
    std::cerr << "Can not get default data layout for target" << std::endl;
    return 1;
  }

  module->setDataLayout(*data_layout);

  auto symbol_pool = std::make_shared<llvm::orc::SymbolStringPool>();
  auto execution_session =
      std::make_unique<llvm::orc::ExecutionSession>(std::move(symbol_pool));

  llvm::orc::RTDyldObjectLinkingLayer object_layer{
      *execution_session,
      []() { return std::make_unique<llvm::SectionMemoryManager>(); }};

  auto ir_compiler = std::make_unique<llvm::orc::ConcurrentIRCompiler>(
      std::move(jit_machine_builder));

  llvm::orc::IRCompileLayer orc_compile_layer{*execution_session, object_layer,
                                              std::move(ir_compiler)};

  llvm::orc::JITDylib &main_dylib =
      execution_session->createBareJITDylib("<main>");

  auto current_process_loader =
      llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
          data_layout->getGlobalPrefix());
  if (!current_process_loader) {
    std::cerr << "Can not get library search generator for current process\n";
    return 1;
  }

  main_dylib.addGenerator(std::move(*current_process_loader));

  {
    llvm::orc::ResourceTrackerSP resource_tracker =
        main_dylib.getDefaultResourceTracker();

    llvm::cantFail(orc_compile_layer.add(
        resource_tracker,
        llvm::orc::ThreadSafeModule{std::move(module), std::move(context)}));
  }

  llvm::orc::MangleAndInterner mangler{*execution_session, *data_layout};

  llvm::Expected<llvm::JITEvaluatedSymbol> symbol =
      execution_session->lookup({&main_dylib}, mangler("factorial"));
  if (!symbol) {
    std::cerr << "Failed to get factorial symbol\n";
    return 1;
  }

  // get factorial function
  using factorial_func_t = long (*)(long);
  factorial_func_t factorial_func =
      llvm::jitTargetAddressToPointer<factorial_func_t>(symbol->getAddress());

#define PRINT_CALL(func_call)                                                  \
  std::cout << #func_call << " = " << func_call << std::endl;

  PRINT_CALL(factorial_func(5));
  PRINT_CALL(factorial_func(10));
  PRINT_CALL(factorial_func(15));
  PRINT_CALL(factorial_func(20));

  if (auto err = execution_session->endSession()) {
    execution_session->reportError(std::move(err));
    return 1;
  }
}
