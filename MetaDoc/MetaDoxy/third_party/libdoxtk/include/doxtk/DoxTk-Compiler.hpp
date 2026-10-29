#pragma once
#include "doxtk/DoxTk.hpp"

// Compilation pipeline for Doxir definitions.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkCompiler.cpp.
namespace doxtk {
struct CompilerModuleInfo {
    static constexpr std::string_view description = "Compilation pipeline for Doxir definitions.";
};
} // namespace doxtk
