#pragma once
#include "doxtk/DoxTk.hpp"

// Parsing interfaces and parse diagnostics.
// This header is intentionally dependency-free. Add stable public declarations
// here and keep implementations in the matching src/DoxTkParser.cpp.
namespace doxtk {
// Plaintext is a single UTF-8 text value, with no normalization of content.
[[nodiscard]] Result<Document> parse_document(std::span<const std::byte> input,
    std::string_view format, std::string_view encoding = "utf-8");

struct ParserModuleInfo {
    static constexpr std::string_view description = "Parsing interfaces and parse diagnostics.";
};
} // namespace doxtk
