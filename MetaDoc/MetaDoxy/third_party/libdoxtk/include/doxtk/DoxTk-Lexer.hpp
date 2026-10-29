#pragma once
#include "doxtk/DoxTk.hpp"

// Lexical analysis for Doxir and format grammars.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkLexer.cpp.
namespace doxtk {
struct LexerModuleInfo {
    static constexpr std::string_view description = "Lexical analysis for Doxir and format grammars.";
};
} // namespace doxtk
