#pragma once
#include "doxtk/DoxTk-AST.hpp"
#include "doxtk/DoxTk.hpp"

// MDAST IR: the Markdown syntax tree produced by the Markdown reader.
// Node kinds and attribute names follow doxir-db/MDAST.doxir.
namespace doxtk {
namespace mdast {

// Document root.
inline constexpr std::string_view document = "document";
// Blocks.
inline constexpr std::string_view paragraph = "paragraph";
inline constexpr std::string_view heading = "heading";           // depth: 1..6
inline constexpr std::string_view blockquote = "blockquote";
inline constexpr std::string_view list_node = "list";             // ordered, start, spread
inline constexpr std::string_view list_item = "listItem";         // checked (optional)
inline constexpr std::string_view code = "code";                 // fenced, lang, meta
inline constexpr std::string_view thematic_break = "thematicBreak";
inline constexpr std::string_view html = "html";
inline constexpr std::string_view definition = "definition";     // identifier, url, title
// Phrases and inlines.
inline constexpr std::string_view text = "text";
inline constexpr std::string_view emphasis = "emphasis";
inline constexpr std::string_view strong = "strong";
inline constexpr std::string_view inline_code = "inlineCode";
inline constexpr std::string_view link = "link";                 // url, title
inline constexpr std::string_view image = "image";               // url, title, alt
inline constexpr std::string_view line_break = "break";
// Unsupported block constructs are preserved as verbatim nodes (text only).
// Other unsupported syntax is literal text: this is not a CommonMark implementation.
inline constexpr std::string_view verbatim = "verbatim";         // reason

// Parses a Markdown document (CommonMark subset; see docs/manual/markdown.md).
// Nesting failures include zero-based line/column context. Encoding failures
// include a zero-based byte offset.
[[nodiscard]] Result<Node> parse(std::string_view source);

// Flattens an MDAST tree with a documented policy per node kind, returning a
// lossiness diagnostic. Registry conversion additionally requires allow_lossy.
[[nodiscard]] Result<std::string> to_plaintext(const Node& root);

struct MDASTModuleInfo {
    static constexpr std::string_view description = "MDAST IR and Markdown reader.";
};
} // namespace mdast
} // namespace doxtk
