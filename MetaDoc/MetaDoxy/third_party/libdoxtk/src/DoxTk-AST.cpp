#include "doxtk/DoxTk-AST.hpp"
#include "doxtk/DoxTk-Encoding.hpp"
#include <algorithm>
#include <array>
#include <utility>

namespace doxtk {
namespace {
constexpr std::array kinds{
    "document", "paragraph", "heading", "blockquote", "list", "listItem", "code",
    "thematicBreak", "html", "definition", "text", "emphasis", "strong", "inlineCode",
    "link", "image", "break", "verbatim"
};

bool is_valid_position(const SourcePosition& position) {
    // Both endpoints of a node must be valid or invalid together.
    return position.valid || (position.line == 0 && position.column == 0 && position.offset == 0);
}

Error validate_position(const SourcePosition& start, const SourcePosition& end,
                        std::string_view path) {
    if (start.valid != end.valid || !is_valid_position(start) || !is_valid_position(end)) {
        return {ErrorCode::internal_error, "Node position is partially set", std::string(path)};
    }
    if (!start.valid) return {};
    if (end.offset < start.offset || end.line < start.line ||
        (end.line == start.line && end.column < start.column) ||
        (end.offset == start.offset && (end.line != start.line || end.column != start.column))) {
        return {ErrorCode::internal_error, "Node end precedes node start", std::string(path)};
    }
    return {};
}

Error validate_at(const Node& node, std::string_view path, std::size_t depth) {
    if (depth > 256) {
        return {ErrorCode::internal_error, "Node nesting exceeds 256 levels", std::string(path)};
    }
    if (node.kind.empty()) return {ErrorCode::internal_error, "Node kind is empty", std::string(path)};
    if (std::find(kinds.begin(), kinds.end(), node.kind) == kinds.end()) {
        return {ErrorCode::internal_error, "Unknown node kind", std::string(path) + ": " + node.kind};
    }
    if (const auto error = validate_position(node.start, node.end, path)) return error;
    const auto check_text = [path](std::string_view text) -> Error {
        auto error = validate_utf8(text);
        if (error) error.context = std::string(path) + ": " + error.context;
        return error;
    };
    if (const auto error = check_text(node.text)) return error;
    const bool leaf = node.kind == "text" || node.kind == "code" || node.kind == "inlineCode" ||
        node.kind == "html" || node.kind == "verbatim" || node.kind == "image" ||
        node.kind == "break" || node.kind == "thematicBreak" || node.kind == "definition";
    if (leaf && !node.children.empty()) {
        return {ErrorCode::invalid_argument, "Leaf node cannot have children", std::string(path)};
    }
    if (!leaf && !node.text.empty()) {
        return {ErrorCode::invalid_argument, "Container text must be represented by text children", std::string(path)};
    }
    for (const auto& [key, value] : node.attributes) {
        if (key.empty()) return {ErrorCode::invalid_argument, "Empty attribute key", std::string(path)};
        if (const auto error = check_text(key)) return error;
        if (const auto error = check_text(value)) return error;
    }
    for (std::size_t i = 1; i < node.attributes.size(); ++i) {
        if (!(node.attributes[i - 1].first < node.attributes[i].first)) {
            return {ErrorCode::internal_error, "Node attributes are not sorted by key", std::string(path)};
        }
    }
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        const auto& child = node.children[i];
        if (node.start.valid && child.start.valid &&
            (child.start.offset < node.start.offset || child.end.offset > node.end.offset ||
             std::make_pair(child.start.line, child.start.column) < std::make_pair(node.start.line, node.start.column) ||
             std::make_pair(child.end.line, child.end.column) > std::make_pair(node.end.line, node.end.column))) {
            return {ErrorCode::invalid_argument, "Child position lies outside parent", std::string(path)};
        }
        if (auto error = validate_at(node.children[i], std::string(path) + ".children[" + std::to_string(i) + "]", depth + 1)) {
            return error;
        }
    }
    return {};
}

std::string log_text(std::string_view text) {
    constexpr char hex[] = "0123456789abcdef";
    std::string output;
    for (unsigned char ch : text) {
        if (ch == '\\') output += "\\\\";
        else if (ch == '"') output += "\\\"";
        else if (ch == '\n') output += "\\n";
        else if (ch == '\r') output += "\\r";
        else if (ch == '\t') output += "\\t";
        else if (ch < 0x20 || ch == 0x7f) {
            output += "\\x";
            output.push_back(hex[ch >> 4]);
            output.push_back(hex[ch & 15]);
        } else output.push_back(static_cast<char>(ch));
    }
    return output;
}

void outline_at(const Node& node, std::string_view path, std::string& output, std::size_t depth) {
    if (depth > 256) {
        output += std::string(path) + " <depth-limit>\n";
        return;
    }
    output += path;
    output += ' ';
    output += log_text(node.kind);
    if (!node.text.empty()) {
        output += " text=\"";
        output += log_text(node.text);
        output += '"';
    }
    for (const auto& [key, value] : node.attributes) output += " " + log_text(key) + "=" + log_text(value);
    if (node.start.valid) output += " @" + std::to_string(node.start.line) + ":" + std::to_string(node.start.column);
    output += '\n';
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        outline_at(node.children[i], std::string(path) + "." + std::to_string(i), output, depth + 1);
    }
}
}

const std::string* Node::attribute(std::string_view name) const noexcept {
    const auto found = std::lower_bound(attributes.begin(), attributes.end(), name,
        [](const auto& attribute, std::string_view key) { return attribute.first < key; });
    return found != attributes.end() && found->first == name ? &found->second : nullptr;
}

bool Node::attribute_equals(std::string_view name, std::string_view expected) const noexcept {
    const auto* value = attribute(name);
    return value && *value == expected;
}

const Node* Node::first_child(std::string_view kind) const noexcept {
    const auto found = std::find_if(children.begin(), children.end(),
        [kind](const Node& child) { return child.kind == kind; });
    return found == children.end() ? nullptr : &*found;
}

std::size_t Node::count_descendants() const noexcept {
    std::size_t total = 0;
    for (const auto& child : children) total += 1 + child.count_descendants();
    return total;
}

Error Node::validate() const { return validate_at(*this, "root", 0); }

std::string Node::outline() const {
    std::string output;
    outline_at(*this, "root", output, 0);
    return output;
}

bool ASTVisitor::visit_children(const Node& node) {
    for (const auto& child : node.children) {
        if (!traverse(child)) return false;
    }
    return true;
}

bool ASTVisitor::traverse(const Node& node) {
    enter(node);
    const bool visit = [&] {
        if (node.kind == "document") return visit_document(node);
        if (node.kind == "paragraph") return visit_paragraph(node);
        if (node.kind == "text") return visit_text(node);
        if (node.kind == "heading") return visit_heading(node);
        if (node.kind == "blockquote") return visit_blockquote(node);
        if (node.kind == "list") return visit_list(node);
        if (node.kind == "listItem") return visit_list_item(node);
        if (node.kind == "code") return visit_code(node);
        if (node.kind == "thematicBreak") return visit_thematic_break(node);
        if (node.kind == "html") return visit_html(node);
        if (node.kind == "emphasis") return visit_emphasis(node);
        if (node.kind == "strong") return visit_strong(node);
        if (node.kind == "inlineCode") return visit_inline_code(node);
        if (node.kind == "link") return visit_link(node);
        if (node.kind == "image") return visit_image(node);
        if (node.kind == "break") return visit_break(node);
        if (node.kind == "definition") return visit_definition(node);
        if (node.kind == "verbatim") return visit_verbatim(node);
        return visit_unknown(node);
    }();
    if (visit) leave(node);
    return visit;
}

std::string_view node_kind_name(std::string_view kind) noexcept {
    if (kind == "document") return "document";
    if (kind == "paragraph") return "paragraph";
    if (kind == "heading") return "heading";
    if (kind == "blockquote") return "blockquote";
    if (kind == "list") return "list";
    if (kind == "listItem") return "list item";
    if (kind == "code") return "code block";
    if (kind == "thematicBreak") return "thematic break";
    if (kind == "html") return "html block";
    if (kind == "definition") return "link definition";
    if (kind == "text") return "text";
    if (kind == "emphasis") return "emphasis";
    if (kind == "strong") return "strong";
    if (kind == "inlineCode") return "inline code";
    if (kind == "link") return "link";
    if (kind == "image") return "image";
    if (kind == "break") return "line break";
    if (kind == "verbatim") return "verbatim";
    return "unknown";
}
} // namespace doxtk
