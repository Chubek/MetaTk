#pragma once
#include "doxtk/DoxTk.hpp"

// Configuration values and loading.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkConfig.cpp.
namespace doxtk {
struct ConfigModuleInfo {
    static constexpr std::string_view description = "Configuration values and loading.";
};
} // namespace doxtk
