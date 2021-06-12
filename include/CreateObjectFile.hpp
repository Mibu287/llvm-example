#ifndef INCLUDE_CREATE_OBJECT_FILE_HPP_
#define INCLUDE_CREATE_OBJECT_FILE_HPP_

#include "DefaultTarget.hpp"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/SmallVectorMemoryBuffer.h"
#include <memory>
#include <system_error>
#include <utility>

using OwningObjectFile = llvm::object::OwningBinary<llvm::object::ObjectFile>;

llvm::Expected<OwningObjectFile>
createObjectFileFromSource(llvm::StringRef file_path) {

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

    // Create ObjectFile from Binary
    std::unique_ptr<llvm::object::ObjectFile> object_file;

    {
        auto expected_object_file =
            llvm::object::ObjectFile::createObjectFile(*file_buffer);

        if (!expected_object_file) {
            return {expected_object_file.takeError()};
        }

        object_file = std::move(expected_object_file.get());
    }

    return OwningObjectFile{std::move(object_file), std::move(file_buffer)};
}

llvm::Expected<OwningObjectFile>
createObjectFileFromModule(llvm::Module &module) {

    std::unique_ptr<llvm::MemoryBuffer> object_buffer;

    {
        llvm::SmallVector<char> compiled_buffer;
        llvm::raw_svector_ostream output_stream{compiled_buffer};
        llvm::legacy::PassManager pass_manager;

        bool pass_manager_err = GetDefaultTargetMachine()->addPassesToEmitFile(
            pass_manager, output_stream, nullptr, llvm::CGFT_ObjectFile);
        if (pass_manager_err) {
            return llvm::make_error<llvm::StringError>(
                std::error_code{}, "Failed to create machine code generator");
        }

        pass_manager.run(module);

        object_buffer = std::make_unique<llvm::SmallVectorMemoryBuffer>(
            std::move(compiled_buffer));
    }

    // Create ObjectFile from Binary
    std::unique_ptr<llvm::object::ObjectFile> object_file;

    {
        auto expected_object_file =
            llvm::object::ObjectFile::createObjectFile(*object_buffer);

        if (!expected_object_file) {
            return {expected_object_file.takeError()};
        }

        object_file = std::move(expected_object_file.get());
    }

    return OwningObjectFile{std::move(object_file), std::move(object_buffer)};
}

#endif // INCLUDE_CREATE_OBJECT_FILE_HPP_
