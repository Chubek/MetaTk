#pragma once
#include "doxtk/DoxTk.hpp"

// Optional XML helpers.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkXML.cpp.
namespace doxtk {
struct XMLModuleInfo {
    static constexpr std::string_view description = "Optional XML helpers.";
};
} // namespace doxtk
