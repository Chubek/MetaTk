#pragma once
#include "doxtk/DoxTk.hpp"
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// AST node model and tree traversal.
namespace doxtk {

// Source positions are zero-based line/column pairs counted in Unicode
// characters, plus a zero-based byte offset. A missing position has
// line == column == offset == 0 and valid == false; line 0 column 0 is a real
// position, so `valid` distinguishes the two.
struct SourcePosition {
    std::size_t line{0};
    std::size_t column{0};
    std::size_t offset{0};
    bool valid{false};

    static SourcePosition at(std::size_t line, std::size_t column, std::size_t offset) noexcept {
        return {line, column, offset, true};
    }
};

// A generic AST node. Nodes are values: copying a node copies its subtree.
// Callers must keep attributes sorted by key with no duplicates; validate()
// checks this invariant. The Markdown reader produces sorted attributes.
struct Node {
    std::string kind;
    std::string text;                                        // Inline text value, if any.
    std::vector<std::pair<std::string, std::string>> attributes;
    std::vector<Node> children;
    SourcePosition start;
    SourcePosition end;

    [[nodiscard]] bool empty() const noexcept { return kind.empty(); }
    [[nodiscard]] bool has_children() const noexcept { return !children.empty(); }
    [[nodiscard]] std::size_t attribute_count() const noexcept { return attributes.size(); }
    // Returns nullptr when the attribute is absent.
    [[nodiscard]] const std::string* attribute(std::string_view name) const noexcept;
    [[nodiscard]] bool attribute_equals(std::string_view name, std::string_view expected) const noexcept;
    [[nodiscard]] const Node* first_child(std::string_view kind) const noexcept;
    [[nodiscard]] std::size_t count_descendants() const noexcept;

    // Validates kinds, positions, and attribute ordering. Returned errors carry
    // the offending path (for example "document[0].children[2]") as context.
    [[nodiscard]] Error validate() const;
    // Renders one log-safe line per node, escaping controls and quotes. Trees
    // deeper than 256 levels are truncated with an explicit <depth-limit> line.
    [[nodiscard]] std::string outline() const;
};

using NodeList = std::vector<Node>;

// Accept/visit traversal. Default implementations walk children; overriding
// `visit_*` is optional. Return false from a handler to stop the traversal.
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    [[nodiscard]] virtual bool visit_document(const Node& node) { return visit_children(node); }
    [[nodiscard]] virtual bool visit_paragraph(const Node& node) { return visit_children(node); }
    [[nodiscard]] virtual bool visit_text(const Node& node) { (void)node; return true; }
    [[nodiscard]] virtual bool visit_heading(const Node& node) { return visit_children(node); }
    [[nodiscard]] virtual bool visit_blockquote(const Node& node) { return visit_children(node); }
    [[nodiscard]] virtual bool visit_list(const Node& node) { return visit_children(node); }
    [[nodiscard]] virtual bool visit_list_item(const Node& node) { return visit_children(node); }
    [[nodiscard]] virtual bool visit_code(const Node& node) { (void)node; return true; }
    [[nodiscard]] virtual bool visit_thematic_break(const Node& node) { (void)node; return true; }
    [[nodiscard]] virtual bool visit_html(const Node& node) { (void)node; return true; }
    [[nodiscard]] virtual bool visit_emphasis(const Node& node) { return visit_children(node); }
    [[nodiscard]] virtual bool visit_strong(const Node& node) { return visit_children(node); }
    [[nodiscard]] virtual bool visit_inline_code(const Node& node) { (void)node; return true; }
    [[nodiscard]] virtual bool visit_link(const Node& node) { return visit_children(node); }
    [[nodiscard]] virtual bool visit_image(const Node& node) { return visit_children(node); }
    [[nodiscard]] virtual bool visit_break(const Node& node) { (void)node; return true; }
    [[nodiscard]] virtual bool visit_definition(const Node& node) { (void)node; return true; }
    [[nodiscard]] virtual bool visit_verbatim(const Node& node) { (void)node; return true; }
    // Called once before any dispatch and once after the traversal completes
    // (only if the traversal was not stopped early).
    virtual void enter(const Node&) {}
    virtual void leave(const Node&) {}
    [[nodiscard]] virtual bool visit_children(const Node& node);
    // Unknown kinds reach here instead of being dropped.
    [[nodiscard]] virtual bool visit_unknown(const Node&) { return false; }

    // Returns false when a handler stopped the traversal.
    [[nodiscard]] bool traverse(const Node& node);
};

// Human-readable names, possibly comma-separated for grouped kinds.
[[nodiscard]] std::string_view node_kind_name(std::string_view kind) noexcept;

struct ASTModuleInfo {
    static constexpr std::string_view description = "AST node model and tree traversal.";
};
} // namespace doxtk
