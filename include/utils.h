#ifndef INCLUDE_UTILS_H_
#define INCLUDE_UTILS_H_

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
#include <memory>
#include <utility>

#define PRINT_EXPR(expr)                                                       \
    llvm::outs() << llvm::raw_ostream::GREEN << #expr                          \
                 << llvm::raw_ostream::RESET << " = "                          \
                 << llvm::raw_ostream::BLUE << (expr)                          \
                 << llvm::raw_ostream::RESET << '\n';

llvm::FormattedNumber format_address(uint64_t addr) {
    return llvm::format_hex(addr, 16);
}

bool has_reloc_symbols(const llvm::object::SectionRef &section) {
    return (section.relocation_begin() != section.relocation_end());
};

void print_reloc_symbols(const llvm::object::SectionRef &section) {
    auto section_name = llvm::cantFail(section.getName());

    llvm::outs() << llvm::raw_ostream::GREEN << "RELOCATION RECORDS FOR ["
                 << section_name << "]\n"
                 << llvm::raw_ostream::RESET;

    llvm::outs() << "Offset          \tType          \tSymbol\n";

    for (const auto &reloc_symbol : section.relocations()) {

        llvm::SmallVector<char> symbol_type;
        reloc_symbol.getTypeName(symbol_type);

        auto symbol_name = llvm::cantFail(reloc_symbol.getSymbol()->getName());

        auto symbol_address = format_address(reloc_symbol.getOffset());

        llvm::outs() << symbol_address << '\t' << symbol_type << '\t'
                     << symbol_name << '\n';
    }

    llvm::outs() << '\n';
};

[[maybe_unused]] static const char disable_colors_if_piped = []() {
    if (!llvm::outs().has_colors()) {
        llvm::outs().enable_colors(false);
    }
    if (!llvm::errs().has_colors()) {
        llvm::errs().enable_colors(false);
    }
    return 0;
}();

#define EXIT_ON_ERROR(Class, Name, RHS)                                        \
    Class Name;                                                                \
    {                                                                          \
        auto _expected_##Name = (RHS);                                         \
        if (!_expected_##Name) {                                               \
            llvm::errs() << llvm::raw_ostream::RED                             \
                         << "ERROR: " << llvm::raw_ostream::RESET              \
                         << _expected_##Name.takeError() << '\n';              \
            return 1;                                                          \
        }                                                                      \
        Name = std::move(*_expected_##Name);                                   \
    }

#endif // INCLUDE_UTILS_H_
