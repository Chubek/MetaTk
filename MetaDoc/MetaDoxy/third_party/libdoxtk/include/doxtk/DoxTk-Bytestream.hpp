#pragma once
#include "doxtk/DoxTk.hpp"

// Byte-oriented input and output helpers.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkBytestream.cpp.
namespace doxtk {
[[nodiscard]] std::vector<std::byte> to_bytes(std::string_view text);
[[nodiscard]] std::string bytes_to_string(std::span<const std::byte> bytes);

struct BytestreamModuleInfo {
    static constexpr std::string_view description = "Byte-oriented input and output helpers.";
};
} // namespace doxtk
