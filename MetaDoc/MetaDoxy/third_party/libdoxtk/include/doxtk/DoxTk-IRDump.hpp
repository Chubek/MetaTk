#pragma once
#include "doxtk/DoxTk-AST.hpp"
#include "doxtk/DoxTk.hpp"
#include <string>
#include <string_view>

// IR JSON/XML inspection envelopes for tree-shaped IRs such as MDAST.
namespace doxtk {
inline constexpr std::string_view ir_json_ir = "DoxTkJSON";
inline constexpr std::string_view ir_xml_ir = "DoxTkXML";

// Deterministic single-line JSON. Keys are emitted in this exact order: kind,
// text, attributes (sorted by key), start, end, children. Position objects are
// present only when valid; otherwise "start":null,"end":null. Metadata keys
// are sorted. Invalid UTF-8 and XML-forbidden characters return structured errors.
[[nodiscard]] Result<std::string> to_ir_json(const Node& root, std::string_view ir,
    std::string_view media_type, const std::unordered_map<std::string, std::string>& metadata);
// Emits an XML 1.0 document. Element and attribute names come from a fixed
// alphabet so node kinds never leak raw text into markup.
[[nodiscard]] Result<std::string> to_ir_xml(const Node& root, std::string_view ir,
    std::string_view media_type, const std::unordered_map<std::string, std::string>& metadata);

struct IRDumpModuleInfo {
    static constexpr std::string_view description = "IR JSON/XML inspection envelopes.";
};
} // namespace doxtk
