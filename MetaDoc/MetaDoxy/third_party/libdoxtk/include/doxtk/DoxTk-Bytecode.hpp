#pragma once
#include "doxtk/DoxTk.hpp"

// Compiled representation for Doxir tooling.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkBytecode.cpp.
namespace doxtk {
struct BytecodeModuleInfo {
    static constexpr std::string_view description = "Compiled representation for Doxir tooling.";
};
} // namespace doxtk
