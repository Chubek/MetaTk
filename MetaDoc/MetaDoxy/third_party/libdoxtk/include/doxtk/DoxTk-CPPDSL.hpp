#pragma once

// Link DoxTk::CPPDSL to enable this surface. Core-only headers never include
// DSLtk, even if the optional interface target was built/installed alongside it.
#if defined(DOXTK_ENABLE_CPP_DSL) && DOXTK_ENABLE_CPP_DSL
#include "doxtk/DoxTk-AST.hpp"
#include "doxtk/DoxTk-Encoding.hpp"
#include <DSLtk.hpp>
#include <algorithm>
#include <initializer_list>
#include <memory>

namespace doxtk::cppdsl {
using Children = std::vector<Result<Node>>;
using Metadata = std::unordered_map<std::string, std::string>;

// Value factories compose into existing MDAST nodes, not a new IR. Validation
// failures propagate as Results; no placeholder nodes or implicit conversions.
class DocumentDSL : public ::dsl::DSL<DocumentDSL, ::dsl::Pipeline> {
public:
    static Result<Node> text(std::string value) { return leaf("text", std::move(value)); }
    static Result<Node> inline_code(std::string value) { return leaf("inlineCode", std::move(value)); }
    static Result<Node> line_break() { return leaf("break", ""); }
    static Result<Node> thematic_break() { return leaf("thematicBreak", ""); }
    static Result<Node> code(std::string value, std::string language = "") {
        return leaf("code", std::move(value), {{"fenced", "true"}, {"lang", std::move(language)}});
    }
    static Result<Node> verbatim(std::string value, std::string reason) {
        return leaf("verbatim", std::move(value), {{"reason", std::move(reason)}});
    }
    static Result<Node> image(std::string alt, std::string url, std::string title = "") {
        return leaf("image", "", {{"alt", std::move(alt)}, {"title", std::move(title)}, {"url", std::move(url)}});
    }
    static Result<Node> paragraph(Children children = {}) { return container("paragraph", std::move(children), true); }
    static Result<Node> emphasis(Children children = {}) { return container("emphasis", std::move(children), true); }
    static Result<Node> strong(Children children = {}) { return container("strong", std::move(children), true); }
    static Result<Node> heading(unsigned depth, Children children = {}) {
        if (depth < 1 || depth > 6) return Result<Node>::failure(ErrorCode::invalid_argument, "Heading depth must be 1..6", "cppdsl heading");
        return container("heading", std::move(children), true, {{"depth", std::to_string(depth)}});
    }
    static Result<Node> link(std::string url, Children children = {}, std::string title = "") {
        return container("link", std::move(children), true, {{"title", std::move(title)}, {"url", std::move(url)}});
    }
    static Result<Node> document(Children children = {}) { return container("document", std::move(children), false); }
    static Result<Node> blockquote(Children children = {}) { return container("blockquote", std::move(children), false); }
    static Result<Node> item(Children children = {}, std::optional<bool> checked = {}) {
        Attributes attributes;
        if (checked) attributes.emplace_back("checked", *checked ? "true" : "false");
        return container("listItem", std::move(children), false, std::move(attributes));
    }
    static Result<Node> list(Children items = {}, bool ordered = false, unsigned start = 1) {
        Attributes attributes{{"ordered", ordered ? "true" : "false"}};
        if (ordered) attributes.emplace_back("start", std::to_string(start));
        return container("list", std::move(items), false, std::move(attributes));
    }

    static Result<Document> build(Result<Node> root, Metadata metadata = {}) {
        if (!root.value && !root.error) return Result<Document>::failure(ErrorCode::internal_error, "Missing node and error", "cppdsl build");
        if (!root) return Result<Document>::failure(root.error.code, root.error.message, root.error.context);
        if (root.value->kind != "document") return Result<Document>::failure(ErrorCode::invalid_argument, "Root must be a document", "cppdsl build");
        if (const auto error = root.value->validate()) return Result<Document>::failure(error.code, error.message, error.context);
        for (const auto& [key, value] : metadata) {
            if (const auto error = validate_utf8(key)) return Result<Document>::failure(error.code, error.message, "cppdsl metadata key: " + error.context);
            if (const auto error = validate_utf8(value)) return Result<Document>::failure(error.code, error.message, "cppdsl metadata value: " + error.context);
        }
        Document output;
        output.ir = "MDAST";
        output.media_type = "text/markdown";
        output.metadata = std::move(metadata);
        output.tree = std::make_shared<const Node>(std::move(*root.value));
        return Result<Document>::success(std::move(output), std::move(root.diagnostics));
    }

    // DSLtk's pipe stage participates in operator| through ADL. Errors stay in
    // Result; no exception-based expected failure path is introduced.
    static auto finish(Metadata metadata = {}) {
        return ::dsl::pipe([metadata = std::move(metadata)](Result<Node> root) {
            return build(std::move(root), metadata);
        });
    }

private:
    using Attributes = std::vector<std::pair<std::string, std::string>>;
    static bool phrase(std::string_view kind) {
        return kind == "text" || kind == "inlineCode" || kind == "emphasis" || kind == "strong" ||
            kind == "link" || kind == "image" || kind == "break";
    }
    static bool block(std::string_view kind) {
        return kind == "paragraph" || kind == "heading" || kind == "blockquote" || kind == "list" ||
            kind == "code" || kind == "thematicBreak" || kind == "verbatim";
    }
    static Result<Node> leaf(std::string kind, std::string text, Attributes attributes = {}) {
        Node node;
        node.kind = std::move(kind); node.text = std::move(text); node.attributes = std::move(attributes);
        if (const auto error = node.validate()) return Result<Node>::failure(error.code, error.message, "cppdsl: " + error.context);
        return Result<Node>::success(std::move(node));
    }
    static Result<Node> container(std::string kind, Children children, bool phrases, Attributes attributes = {}) {
        Node node;
        node.kind = std::move(kind); node.attributes = std::move(attributes);
        std::vector<std::string> diagnostics;
        for (auto& child : children) {
            if (!child.value && !child.error) return Result<Node>::failure(ErrorCode::internal_error, "Missing child and error", "cppdsl " + node.kind);
            if (!child) return Result<Node>::failure(child.error.code, child.error.message, child.error.context);
            const auto& child_kind = child.value->kind;
            const bool allowed = node.kind == "list" ? child_kind == "listItem" :
                (phrases ? phrase(child_kind) : block(child_kind));
            if (!allowed) return Result<Node>::failure(ErrorCode::invalid_argument,
                "Invalid child " + child_kind + " for " + node.kind, "cppdsl " + node.kind);
            diagnostics.insert(diagnostics.end(), child.diagnostics.begin(), child.diagnostics.end());
            node.children.push_back(std::move(*child.value));
        }
        if (const auto error = node.validate()) return Result<Node>::failure(error.code, error.message, "cppdsl: " + error.context);
        return Result<Node>::success(std::move(node), std::move(diagnostics));
    }
};
} // namespace doxtk::cppdsl
#endif
