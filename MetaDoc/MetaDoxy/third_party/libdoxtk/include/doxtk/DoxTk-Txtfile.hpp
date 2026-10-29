#pragma once
#include "doxtk/DoxTk.hpp"

// Text file helpers.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkTxtfile.cpp.
namespace doxtk {
struct TxtfileModuleInfo {
    static constexpr std::string_view description = "Text file helpers.";
};
} // namespace doxtk
