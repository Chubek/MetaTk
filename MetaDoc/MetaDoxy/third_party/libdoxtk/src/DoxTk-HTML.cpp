#include "doxtk/DoxTk-HTML.hpp"
#include "doxtk/DoxTk-Encoding.hpp"
#include "doxtk/DoxTk-Bytestream.hpp"
#include <algorithm>
#include <charconv>

namespace doxtk {
namespace html {

std::string escape_html(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 32);
    for (unsigned char ch : text) {
        switch (ch) {
            case '&': out += "&"; break;
            case '<': out += "<"; break;
            case '>': out += ">"; break;
            case 0x22: out += "\""; break;
            case 0x27: out += "&apos;"; break;
            default: out += static_cast<char>(ch);
        }
    }
    return out;
}

std::string escape_attr(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 16);
    for (unsigned char ch : text) {
        switch (ch) {
            case '&': out += "&"; break;
            case '<': out += "<"; break;
            case '>': out += ">"; break;
            case 0x22: out += "\""; break;
            case 0x27: out += "&apos;"; break;
            case 0x0a: out += "&#10;"; break;
            case 0x0d: out += "&#13;"; break;
            case 0x09: out += "&#9;"; break;
            default: out += static_cast<char>(ch);
        }
    }
    return out;
}

Result<std::string> to_html(const Node& root, std::string_view /*title*/) {
    using Output = Result<std::string>;
    if (const auto err = root.validate()) return Output::failure(err.code, err.message, err.context);
    std::string out = "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"UTF-8\">\n";
    out += "</head>\n<body>\n";

    std::function<void(const Node&)> render = [&](const Node& node) {
        const auto& kind = node.kind;
        if (kind == "document") {
            for (const auto& child : node.children) render(child);
        } else if (kind == "paragraph") {
            out += "<p>";
            for (const auto& child : node.children) render(child);
            out += "</p>\n";
        } else if (kind == "heading") {
            const auto* depth = node.attribute("depth");
            unsigned d = 1;
            if (depth && std::from_chars(depth->data(), depth->data() + depth->size(), d).ec == std::errc{})
                d = std::clamp(d, 1u, 6u);
            out += "<h" + std::to_string(d) + ">";
            for (const auto& child : node.children) render(child);
            out += "</h" + std::to_string(d) + ">\n";
        } else if (kind == "blockquote") {
            out += "<blockquote>\n";
            for (const auto& child : node.children) render(child);
            out += "</blockquote>\n";
        } else if (kind == "list") {
            const auto* ordered = node.attribute("ordered");
            const auto* start = node.attribute("start");
            out += (ordered && *ordered == "true") ? "<ol" : "<ul";
            if (ordered && *ordered == "true" && start) out += " start=\"" + escape_attr(*start) + "\"";
            out += ">\n";
            for (const auto& child : node.children) render(child);
            out += (ordered && *ordered == "true") ? "</ol>\n" : "</ul>\n";
        } else if (kind == "listItem") {
            const auto* checked = node.attribute("checked");
            out += "<li>";
            if (checked) out += "<input type=\"checkbox\" " + std::string(*checked == "true" ? "checked" : "") + " disabled> ";
            for (const auto& child : node.children) render(child);
            out += "</li>\n";
        } else if (kind == "code") {
            const auto* lang = node.attribute("lang");
            out += "<pre><code";
            if (lang) out += " class=\"language-" + escape_attr(*lang) + "\"";
            out += ">" + escape_html(node.text) + "</code></pre>\n";
        } else if (kind == "thematicBreak") {
            out += "<hr>\n";
        } else if (kind == "text") {
            out += escape_html(node.text);
        } else if (kind == "emphasis") {
            out += "<em>";
            for (const auto& child : node.children) render(child);
            out += "</em>";
        } else if (kind == "strong") {
            out += "<strong>";
            for (const auto& child : node.children) render(child);
            out += "</strong>";
        } else if (kind == "inlineCode") {
            out += "<code>" + escape_html(node.text) + "</code>";
        } else if (kind == "link") {
            const auto* url = node.attribute("url");
            const auto* title_attr = node.attribute("title");
            out += "<a href=\"" + escape_attr(url ? *url : "") + "\"";
            if (title_attr) out += " title=\"" + escape_attr(*title_attr) + "\"";
            out += ">";
            for (const auto& child : node.children) render(child);
            out += "</a>";
        } else if (kind == "image") {
            const auto* url = node.attribute("url");
            const auto* alt = node.attribute("alt");
            const auto* title_attr = node.attribute("title");
            out += "<img src=\"" + escape_attr(url ? *url : "") + "\" alt=\"" + escape_attr(alt ? *alt : "") + "\"";
            if (title_attr) out += " title=\"" + escape_attr(*title_attr) + "\"";
            out += ">";
        } else if (kind == "break") {
            out += "<br>\n";
        } else if (kind == "verbatim") {
            out += "<span class=\"verbatim\"";
            if (const auto* reason = node.attribute("reason"))
                out += " data-reason=\"" + escape_attr(*reason) + "\"";
            out += ">" + escape_html(node.text) + "</span>";
        } else {
            out += "<span class=\"unknown-kind\">" + escape_html(node.kind);
            if (!node.text.empty()) out += ": " + escape_html(node.text);
            out += "</span>";
        }
    };

    render(root);
    out += "</body>\n</html>\n";
    return Output::success(std::move(out));
}

Result<std::vector<std::byte>> serialize_html(const Document& document, std::string_view /*title*/) {
    using Output = Result<std::vector<std::byte>>;
    if (!document.tree) return Output::failure(ErrorCode::invalid_argument, "Document has no AST tree", "serialize_html");
    if (document.tree->kind != "document") return Output::failure(ErrorCode::invalid_argument, "Tree root must be a document", "serialize_html");
    auto html = to_html(*document.tree, "");
    if (!html) return Output::failure(html.error.code, html.error.message, html.error.context);
    return Output::success(to_bytes(*html.value));
}

} // namespace html
} // namespace doxtk
