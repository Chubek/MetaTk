#pragma once
#include "doxtk/DoxTk.hpp"

// Streaming document events.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkEventStream.cpp.
namespace doxtk {
struct EventStreamModuleInfo {
    static constexpr std::string_view description = "Streaming document events.";
};
} // namespace doxtk
