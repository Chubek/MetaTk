#pragma once
#include "doxtk/DoxTk.hpp"

// Binary container helpers.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkBinfile.cpp.
namespace doxtk {
struct BinfileModuleInfo {
    static constexpr std::string_view description = "Binary container helpers.";
};
} // namespace doxtk
