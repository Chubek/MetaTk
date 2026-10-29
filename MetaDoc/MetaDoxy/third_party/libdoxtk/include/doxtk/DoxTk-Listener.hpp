#pragma once
#include "doxtk/DoxTk.hpp"

// Parser/compiler diagnostics and events.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkListener.cpp.
namespace doxtk {
struct ListenerModuleInfo {
    static constexpr std::string_view description = "Parser/compiler diagnostics and events.";
};
} // namespace doxtk
