#ifndef METACHAIN_QSCRIPT_INTERNAL_H
#define METACHAIN_QSCRIPT_INTERNAL_H
// Shared helpers for the QScript implementation (not installed API).
#include "qscript/QScript.h"
#include <string>
#include <string_view>
#include <vector>

namespace qscript::detail {
// Statements, declarations and calls may carry a trailing location leaf
// tagged "@" whose value is "line:column:file". It is optional, so ASTs
// built by hand need not include it.
struct Location { std::string file; std::size_t line = 0, column = 0; };
inline bool is_location(const dsl::ASTNode& n) { return n.is_leaf() && n.tag() == "@"; }
inline std::size_t arity(const dsl::ASTNode& n) {
    if (n.is_leaf()) return 0;
    const auto size = n.size();
    return size && is_location(n.children().back()) ? size - 1 : size;
}
inline const dsl::ASTNode& arg(const dsl::ASTNode& n, std::size_t i) { return n.child(i); }
inline std::string encode_location(std::size_t line, std::size_t column, std::string_view file) {
    return std::to_string(line) + ":" + std::to_string(column) + ":" + std::string(file);
}
inline Location location(const dsl::ASTNode& n) {
    Location l;
    if (n.is_leaf() || !n.size() || !is_location(n.children().back())) return l;
    const auto& v = n.children().back().value();
    const auto a = v.find(':'), b = a == std::string::npos ? a : v.find(':', a + 1);
    if (b == std::string::npos) return l;
    l.line = std::strtoull(v.substr(0, a).c_str(), nullptr, 10);
    l.column = std::strtoull(v.substr(a + 1, b - a - 1).c_str(), nullptr, 10);
    l.file = v.substr(b + 1);
    return l;
}
// Embedded sources produced at build time from qscript/grammar and qscript/stdlib.
struct EmbeddedLibrary { const char* name; const char* source; };
const char* embedded_grammar();
const EmbeddedLibrary* embedded_libraries(std::size_t* count);
} // namespace qscript::detail
#endif
