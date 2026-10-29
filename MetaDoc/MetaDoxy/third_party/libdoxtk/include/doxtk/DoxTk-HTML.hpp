#pragma once
#include "doxtk/DoxTk-AST.hpp"
#include "doxtk/DoxTk.hpp"

// HTML serializer: MDAST -> safe, deterministic HTML.
namespace doxtk {
namespace html {
// Serializes an MDAST tree to HTML5. Unsupported nodes render as escaped text
// wrapped in <span class="verbatim"> with a data-reason attribute.
[[nodiscard]] Result<std::string> to_html(const Node& root, std::string_view title = "");

// Serializes a Document with MDAST IR to HTML.
[[nodiscard]] Result<std::vector<std::byte>> serialize_html(const Document& document,
    std::string_view title = "");

struct HTMLModuleInfo {
    static constexpr std::string_view description = "HTML serializer for MDAST.";
};
} // namespace html
} // namespace doxtk
