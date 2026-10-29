#pragma once
#include "doxtk/DoxTk.hpp"

// Inter-process communication abstractions.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkIPC.cpp.
namespace doxtk {
struct IPCModuleInfo {
    static constexpr std::string_view description = "Inter-process communication abstractions.";
};
} // namespace doxtk
