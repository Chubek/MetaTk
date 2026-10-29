#pragma once
#include "doxtk/DoxTk.hpp"

// Encoding detection and transcoding.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkEncoding.cpp.
namespace doxtk {
// Validates Unicode scalar values, rejecting overlong sequences and surrogates.
[[nodiscard]] Error validate_utf8(std::string_view text);
// No guessing or replacement characters. Supported encodings: utf-8, ascii,
// iso-8859-1. BOMs and line endings are preserved as content.
[[nodiscard]] Result<std::string> decode_text(
    std::span<const std::byte> bytes, std::string_view encoding = "utf-8");

struct EncodingModuleInfo {
    static constexpr std::string_view description = "Encoding detection and transcoding.";
};
} // namespace doxtk
