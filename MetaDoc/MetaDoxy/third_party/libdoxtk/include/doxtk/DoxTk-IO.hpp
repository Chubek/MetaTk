#pragma once
#include "doxtk/DoxTk.hpp"

// Binary-safe file I/O.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkIO.cpp.
namespace doxtk {
struct IOModuleInfo {
    static constexpr std::string_view description = "Binary-safe file I/O.";
};
} // namespace doxtk
