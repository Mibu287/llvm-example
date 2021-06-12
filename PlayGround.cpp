#include "llvm/ADT/StringRef.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Object/SymbolicFile.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>
#include <memory>
#include <tuple>
#include <utility>

#define PRINT_EXPR(expr) std::cout << #expr << " = " << (expr) << std::endl;

using OwningSymbolicFile =
    llvm::object::OwningBinary<llvm::object::SymbolicFile>;

llvm::Expected<OwningSymbolicFile>
createSymbolicFileFromSource(llvm::StringRef file_path) {

    llvm::LLVMContext context{};

    // Create Binary File from source
    std::unique_ptr<llvm::object::Binary> binary_file;
    std::unique_ptr<llvm::MemoryBuffer> file_buffer;

    {
        auto expected_binary_file = llvm::object::createBinary(
            file_path, &context, /*initContent*/ true);

        if (!expected_binary_file) {
            return {expected_binary_file.takeError()};
        }

        std::tie(binary_file, file_buffer) = expected_binary_file->takeBinary();
    }

    // Create SymbolicFile from Binary
    std::unique_ptr<llvm::object::SymbolicFile> symbolic_file;

    {
        auto expected_symbolic_file =
            llvm::object::SymbolicFile::createSymbolicFile(*file_buffer);

        if (!expected_symbolic_file) {
            return {expected_symbolic_file.takeError()};
        }

        symbolic_file = std::move(expected_symbolic_file.get());
    }

    return OwningSymbolicFile{std::move(symbolic_file), std::move(file_buffer)};
}

int main() {
    llvm::StringRef file_path{"build-Debug/PlayGround"};

    OwningSymbolicFile owned_symbolic_file =
        llvm::cantFail(createSymbolicFileFromSource(file_path));

    llvm::object::SymbolicFile *symbolic_file = owned_symbolic_file.getBinary();

    // Print all symbol
    llvm::Error err = llvm::Error::success();

    for (auto iter = symbolic_file->symbol_begin();
         iter != symbolic_file->symbol_end() && !err; ++iter) {

        err = iter->printName(llvm::outs());
        llvm::outs() << '\n';
    }

    if (err) {
        llvm::errs() << err << '\n';
        return 1;
    }
}
