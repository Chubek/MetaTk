#pragma once
#include "doxtk/DoxTk.hpp"

// Decode serialized IR documents.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkDeserializer.cpp.
namespace doxtk {
struct DeserializerModuleInfo {
    static constexpr std::string_view description = "Decode serialized IR documents.";
};
} // namespace doxtk
