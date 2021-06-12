#include "CreateObjectFile.hpp"
#include "utils.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

int main(int argc, char *argv[]) {

    // Get source file
    if (argc < 2) {
        llvm::errs() << "No file path found in command line arguments\n";
        return 1;
    }

    llvm::StringRef file_path{argv[1]};

    // Create ObjectFile
    OwningObjectFile owned_object_file =
        llvm::cantFail(createObjectFileFromSource(file_path));

    llvm::object::ObjectFile *object_file = owned_object_file.getBinary();

    if (!object_file->isRelocatableObject()) {
        llvm::errs() << file_path << " is not relocatable object\n";
        return 1;
    }

    // Print all relocatable symbols
    for (const auto &section : object_file->sections()) {
        if (has_reloc_symbols(section))
            print_reloc_symbols(section);
    }
}
