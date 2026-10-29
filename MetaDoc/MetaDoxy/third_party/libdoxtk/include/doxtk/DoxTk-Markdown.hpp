#pragma once
#include "doxtk/DoxTk.hpp"

// Markdown reader: bytes -> MDAST. No third-party dependencies.
namespace doxtk {
// Decodes with `encoding` (utf-8, ascii, iso-8859-1) and parses Markdown.
// Nesting errors carry zero-based line/column context; encoding errors carry
// a byte offset. Returns an immutable tree and its decoded source bytes.
[[nodiscard]] Result<Document> parse_markdown(
    std::span<const std::byte> input, std::string_view encoding = "utf-8");

struct MarkdownModuleInfo {
    static constexpr std::string_view description = "Markdown reader producing MDAST.";
};
} // namespace doxtk
