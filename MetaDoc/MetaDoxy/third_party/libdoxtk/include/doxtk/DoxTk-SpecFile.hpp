#pragma once
#include "doxtk/DoxTk.hpp"

// Format and IR specification files.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkSpecFile.cpp.
namespace doxtk {
struct SpecFileModuleInfo {
    static constexpr std::string_view description = "Format and IR specification files.";
};
} // namespace doxtk
