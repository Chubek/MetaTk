#include "doxtk/DoxTk-Markdown.hpp"
#include "doxtk/DoxTk-Bytestream.hpp"
#include "doxtk/DoxTk-Encoding.hpp"
#include "doxtk/DoxTk-MDAST.hpp"
#include <algorithm>
#include <memory>

namespace doxtk {
namespace {
std::string_view trim(std::string_view text) {
    const auto start = text.find_first_not_of(" \t");
    if (start == std::string_view::npos) return {};
    return text.substr(start, text.find_last_not_of(" \t") - start + 1);
}

struct Line {
    std::string_view text;
    std::size_t offset;
};

struct Marker {
    std::size_t length;
    bool ordered;
    std::string start;
    char delimiter;
};

std::optional<Marker> list_marker(std::string_view text) {
    if (text.size() >= 2 && (text[0] == '-' || text[0] == '*' || text[0] == '+') &&
        (text[1] == ' ' || text[1] == '\t')) return Marker{2, false, "", text[0]};
    std::size_t i = 0;
    while (i < text.size() && i < 9 && text[i] >= '0' && text[i] <= '9') ++i;
    if (i && i + 1 < text.size() && (text[i] == '.' || text[i] == ')') &&
        (text[i + 1] == ' ' || text[i + 1] == '\t')) {
        return Marker{i + 2, true, std::string(text.substr(0, i)), text[i]};
    }
    return {};
}

bool thematic(std::string_view text) {
    if (text.empty() || (text[0] != '-' && text[0] != '*' && text[0] != '_')) return false;
    std::size_t count = 0;
    for (char ch : text) {
        if (ch == text[0]) ++count;
        else if (ch != ' ' && ch != '\t') return false;
    }
    return count >= 3;
}

std::size_t heading_depth(std::string_view text) {
    std::size_t count = 0;
    while (count < text.size() && text[count] == '#') ++count;
    return count && count <= 6 && (count == text.size() || text[count] == ' ' || text[count] == '\t') ? count : 0;
}

std::size_t fence_length(std::string_view text) {
    if (text.empty() || (text[0] != '`' && text[0] != '~')) return 0;
    std::size_t count = 0;
    while (count < text.size() && text[count] == text[0]) ++count;
    return count >= 3 ? count : 0;
}

bool whitespace(char ch) { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; }

bool word_byte(char ch) {
    const auto byte = static_cast<unsigned char>(ch);
    return byte >= 0x80 || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9');
}

// Match a whole delimiter run, never a suffix of a longer run.
std::size_t closing_run(std::string_view text, std::size_t from, char marker, std::size_t length) {
    auto next = text.find(marker, from);
    while (next != std::string_view::npos) {
        auto end = next;
        while (end < text.size() && text[end] == marker) ++end;
        if (end - next == length) return next;
        next = text.find(marker, end);
    }
    return std::string_view::npos;
}

// A small, explicit subset reader. Unsupported syntax remains literal text or
// a verbatim node; it is never discarded as an unresolved definition.
class MarkdownParser {
public:
    explicit MarkdownParser(std::string_view source) : source_(source) {
        line_starts_.push_back(0);
        for (std::size_t i = 0; i < source.size(); ++i) {
            if ((static_cast<unsigned char>(source[i]) & 0xc0) == 0x80) continuation_offsets_.push_back(i);
            if (source[i] == '\r' || source[i] == '\n') {
                if (source[i] == '\r' && i + 1 < source.size() && source[i + 1] == '\n') ++i;
                line_starts_.push_back(i + 1);
            }
        }
    }

    Result<Node> parse() {
        std::vector<Line> lines;
        for (std::size_t offset = 0; offset < source_.size();) {
            const auto newline = source_.find_first_of("\r\n", offset);
            const auto end = newline == std::string_view::npos ? source_.size() : newline;
            lines.push_back({source_.substr(offset, end - offset), offset});
            offset = end;
            if (offset < source_.size() && source_[offset++] == '\r' && offset < source_.size() && source_[offset] == '\n') ++offset;
        }
        auto root = make("document", 0, source_.size());
        root.children = blocks(lines, 0);
        if (error_) return Result<Node>::failure(error_.code, error_.message, error_.context);
        if (const auto error = root.validate()) return Result<Node>::failure(error.code, error.message, error.context);
        return Result<Node>::success(std::move(root));
    }

private:
    SourcePosition position(std::size_t offset) const {
        const auto line = static_cast<std::size_t>(std::upper_bound(line_starts_.begin(), line_starts_.end(), offset) - line_starts_.begin() - 1);
        const auto continuation_count = static_cast<std::size_t>(
            std::lower_bound(continuation_offsets_.begin(), continuation_offsets_.end(), offset) -
            std::lower_bound(continuation_offsets_.begin(), continuation_offsets_.end(), line_starts_[line]));
        const auto column = offset - line_starts_[line] - continuation_count;
        return SourcePosition::at(line, column, offset);
    }

    Node make(std::string_view kind, std::size_t start, std::size_t end) const {
        Node node;
        node.kind = kind;
        node.start = position(start);
        node.end = position(end);
        return node;
    }

    bool depth_ok(std::size_t depth, std::size_t offset) {
        if (depth <= 64) return true;
        const auto pos = position(offset);
        error_ = {ErrorCode::parse_error, "Markdown nesting exceeds 64 levels",
            "line " + std::to_string(pos.line) + " column " + std::to_string(pos.column)};
        return false;
    }

    // Removes at most three spaces before block markers, keeping byte offsets.
    static Line content(Line line) {
        std::size_t spaces = 0;
        while (spaces < 3 && spaces < line.text.size() && line.text[spaces] == ' ') ++spaces;
        return {line.text.substr(spaces), line.offset + spaces};
    }

    static bool interrupts(Line line) {
        const auto text = content(line).text;
        return heading_depth(text) || fence_length(text) || thematic(text) || list_marker(text) ||
            text.starts_with('>') || text.starts_with('<') || line.text.starts_with("    ") || line.text.starts_with('\t') ||
            (text.starts_with('[') && text.find("]:") != std::string_view::npos);
    }

    void append_text(NodeList& nodes, std::string_view text, std::size_t start, std::size_t end) const {
        if (text.empty()) return;
        if (!nodes.empty() && nodes.back().kind == "text" && nodes.back().end.offset == start) {
            nodes.back().text += text;
            nodes.back().end = position(end);
        } else {
            auto node = make("text", start, end);
            node.text = text;
            nodes.push_back(std::move(node));
        }
    }

    NodeList inlines(std::string_view text, std::size_t offset, std::size_t depth) {
        NodeList nodes;
        if (!depth_ok(depth, offset)) return nodes;
        for (std::size_t i = 0; i < text.size();) {
            const auto start = i;
            if (text[i] == '`') {
                std::size_t run = 1;
                while (i + run < text.size() && text[i + run] == '`') ++run;
                const auto close = closing_run(text, i + run, '`', run);
                if (close != std::string_view::npos) {
                    auto node = make("inlineCode", offset + i, offset + close + run);
                    node.text = text.substr(i + run, close - i - run);
                    nodes.push_back(std::move(node));
                    i = close + run;
                    continue;
                }
                append_text(nodes, text.substr(i, run), offset + i, offset + i + run);
                i += run;
                continue;
            }
            const bool image = text.substr(i).starts_with("![");
            if (image || text[i] == '[') {
                const auto label_start = i + (image ? 2 : 1);
                const auto bracket = text.find(']', label_start);
                if (bracket != std::string_view::npos && bracket + 1 < text.size() && text[bracket + 1] == '(') {
                    // Simple destinations only. A quoted title may contain ')',
                    // but nested destination parentheses remain literal syntax.
                    auto close = bracket + 2;
                    bool quoted = false, nested = false;
                    for (; close < text.size(); ++close) {
                        if (text[close] == '"') quoted = !quoted;
                        if (!quoted && text[close] == '(') nested = true;
                        if (!quoted && text[close] == ')') break;
                    }
                    if (close < text.size() && !nested && !quoted) {
                        auto destination = trim(text.substr(bracket + 2, close - bracket - 2));
                        auto node = make(image ? "image" : "link", offset + i, offset + close + 1);
                        auto quote = destination.find(" \"");
                        if (quote == std::string_view::npos) quote = destination.find("\t\"");
                        if (quote != std::string_view::npos && destination.ends_with('"')) {
                            node.attributes.emplace_back("title", destination.substr(quote + 2, destination.size() - quote - 3));
                            destination = trim(destination.substr(0, quote));
                        }
                        if (destination.find_first_of(" \t\"<>") == std::string_view::npos) {
                            node.attributes.emplace_back("url", destination);
                            if (image) node.attributes.emplace_back("alt", text.substr(label_start, bracket - label_start));
                            else node.children = inlines(text.substr(label_start, bracket - label_start), offset + label_start, depth + 1);
                            std::sort(node.attributes.begin(), node.attributes.end());
                            nodes.push_back(std::move(node));
                            i = close + 1;
                            continue;
                        }
                    }
                }
            }
            if (text[i] == '*' || text[i] == '_') {
                const auto marker = text[i];
                std::size_t run = 1;
                while (i + run < text.size() && text[i + run] == marker) ++run;
                // Triple runs and intraword underscores are deliberately literal.
                if (run <= 2 && i + run < text.size() && !whitespace(text[i + run]) &&
                    (marker != '_' || i == 0 || !word_byte(text[i - 1]))) {
                    auto close = closing_run(text, i + run, marker, run);
                    while (close != std::string_view::npos &&
                           (close == i + run || whitespace(text[close - 1]) ||
                            (marker == '_' && close + run < text.size() && word_byte(text[close + run])))) {
                        close = closing_run(text, close + run, marker, run);
                    }
                    if (close != std::string_view::npos) {
                        auto node = make(run == 1 ? "emphasis" : "strong", offset + i, offset + close + run);
                        node.children = inlines(text.substr(i + run, close - i - run), offset + i + run, depth + 1);
                        nodes.push_back(std::move(node));
                        i = close + run;
                        continue;
                    }
                }
                append_text(nodes, text.substr(i, run), offset + i, offset + i + run);
                i += run;
                continue;
            }
            // Advance a whole UTF-8 scalar so even literal special characters
            // adjacent to Unicode never produce individually invalid nodes.
            ++i;
            while (i < text.size() && (static_cast<unsigned char>(text[i]) & 0xc0) == 0x80) ++i;
            append_text(nodes, text.substr(start, i - start), offset + start, offset + i);
        }
        return nodes;
    }

    Node paragraph(std::span<const Line> lines, std::size_t depth) {
        auto node = make("paragraph", lines.front().offset, lines.back().offset + lines.back().text.size());
        for (std::size_t i = 0; i < lines.size(); ++i) {
            auto text = lines[i].text;
            std::size_t backslashes = 0;
            while (backslashes < text.size() && text[text.size() - backslashes - 1] == '\\') ++backslashes;
            const bool slash_break = backslashes % 2 == 1;
            const bool hard_break = i + 1 < lines.size() && (text.ends_with("  ") || slash_break);
            if (hard_break) text.remove_suffix(slash_break ? 1 : 2);
            auto children = inlines(text, lines[i].offset, depth + 1);
            for (auto& child : children) node.children.push_back(std::move(child));
            if (i + 1 < lines.size()) {
                auto newline = make(hard_break ? "break" : "text", lines[i].offset + text.size(), lines[i + 1].offset);
                if (!hard_break) newline.text = "\n";
                node.children.push_back(std::move(newline));
            }
        }
        return node;
    }

    NodeList blocks(std::span<const Line> lines, std::size_t depth) {
        NodeList nodes;
        if (lines.empty() || !depth_ok(depth, lines.front().offset)) return nodes;
        for (std::size_t i = 0; i < lines.size() && !error_;) {
            if (trim(lines[i].text).empty()) { ++i; continue; }
            const auto first = i;
            const auto line = content(lines[i]);
            const auto text = line.text;
            if (const auto run = fence_length(text)) {
                auto node = make("code", lines[i].offset, lines[i].offset + lines[i].text.size());
                node.attributes.emplace_back("fenced", "true");
                const auto info = trim(text.substr(run));
                const auto separator = info.find_first_of(" \t");
                if (!info.empty()) node.attributes.emplace_back("lang", info.substr(0, separator));
                if (separator != std::string_view::npos && !trim(info.substr(separator)).empty())
                    node.attributes.emplace_back("meta", trim(info.substr(separator)));
                ++i;
                const auto content_start = i;
                while (i < lines.size()) {
                    const auto closing = content(lines[i]).text;
                    const auto length = fence_length(closing);
                    if (length >= run && closing[0] == text[0] && trim(closing.substr(length)).empty()) break;
                    if (i > content_start) node.text += '\n';
                    node.text += lines[i++].text;
                }
                if (i < lines.size()) ++i;
                node.end = position(lines[i - 1].offset + lines[i - 1].text.size());
                nodes.push_back(std::move(node));
            } else if (const auto count = heading_depth(text)) {
                auto node = make("heading", lines[i].offset, lines[i].offset + lines[i].text.size());
                auto value = trim(text.substr(count));
                auto end = value.size();
                while (end && value[end - 1] == '#') --end;
                if (end < value.size() && (end == 0 || value[end - 1] == ' ')) value = trim(value.substr(0, end));
                node.attributes.emplace_back("depth", std::to_string(count));
                if (!value.empty()) node.children = inlines(value, static_cast<std::size_t>(value.data() - source_.data()), depth + 1);
                nodes.push_back(std::move(node));
                ++i;
            } else if (thematic(text)) {
                nodes.push_back(make("thematicBreak", lines[i].offset, lines[i].offset + lines[i].text.size()));
                ++i;
            } else if (text.starts_with('>')) {
                std::vector<Line> quoted;
                while (i < lines.size() && content(lines[i]).text.starts_with('>')) {
                    auto inner = content(lines[i++]);
                    inner.text.remove_prefix(1); ++inner.offset;
                    if (inner.text.starts_with(' ')) { inner.text.remove_prefix(1); ++inner.offset; }
                    quoted.push_back(inner);
                }
                auto node = make("blockquote", lines[first].offset, lines[i - 1].offset + lines[i - 1].text.size());
                node.children = blocks(quoted, depth + 1);
                nodes.push_back(std::move(node));
            } else if (const auto marker = list_marker(text)) {
                auto node = make("list", lines[i].offset, lines[i].offset + lines[i].text.size());
                node.attributes.emplace_back("ordered", marker->ordered ? "true" : "false");
                if (marker->ordered) node.attributes.emplace_back("start", marker->start);
                while (i < lines.size()) {
                    auto inner = content(lines[i]);
                    const auto next = list_marker(inner.text);
                    if (!next || next->delimiter != marker->delimiter ||
                        inner.offset - lines[i].offset != line.offset - lines[first].offset || thematic(inner.text)) break;
                    auto item = make("listItem", lines[i].offset, lines[i].offset + lines[i].text.size());
                    inner.text.remove_prefix(next->length); inner.offset += next->length;
                    if (inner.text.starts_with("[ ] ") || inner.text.starts_with("[x] ") || inner.text.starts_with("[X] ")) {
                        item.attributes.emplace_back("checked", inner.text[1] == ' ' ? "false" : "true");
                        inner.text.remove_prefix(4); inner.offset += 4;
                    }
                    item.children.push_back(paragraph(std::span<const Line>(&inner, 1), depth + 1));
                    node.children.push_back(std::move(item));
                    ++i;
                }
                node.end = position(lines[i - 1].offset + lines[i - 1].text.size());
                nodes.push_back(std::move(node));
            } else if (lines[i].text.starts_with("    ") || lines[i].text.starts_with('\t') || text.starts_with('<') ||
                       (text.starts_with('[') && text.find("]:") != std::string_view::npos)) {
                auto node = make("verbatim", lines[i].offset, lines[i].offset + lines[i].text.size());
                node.text = lines[i].text;
                node.attributes.emplace_back("reason", "unsupported-block-syntax");
                nodes.push_back(std::move(node));
                ++i;
            } else {
                ++i;
                while (i < lines.size() && !trim(lines[i].text).empty() && !interrupts(lines[i])) ++i;
                nodes.push_back(paragraph(lines.subspan(first, i - first), depth));
            }
        }
        return nodes;
    }

    std::string_view source_;
    std::vector<std::size_t> line_starts_;
    std::vector<std::size_t> continuation_offsets_;
    Error error_;
};

std::string flatten(const Node& node) {
    if (node.kind == "text" || node.kind == "inlineCode" || node.kind == "code" || node.kind == "html" ||
        node.kind == "verbatim") return node.text;
    if (node.kind == "break") return "\n";
    if (node.kind == "thematicBreak") return "---";
    if (node.kind == "image") {
        if (const auto* alt = node.attribute("alt")) return *alt;
        return node.text;
    }
    if (node.kind == "definition") {
        if (!node.text.empty()) return node.text;
        const auto* url = node.attribute("url");
        return url ? *url : "";
    }
    std::string output;
    const bool block_container = node.kind == "document" || node.kind == "blockquote" || node.kind == "listItem";
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        if (i && block_container) output += "\n\n";
        if (i && node.kind == "list") output += '\n';
        if (node.kind == "list") output += "- ";
        output += flatten(node.children[i]);
    }
    return output;
}
} // namespace

Result<Node> mdast::parse(std::string_view source) {
    if (const auto error = validate_utf8(source)) return Result<Node>::failure(error.code, error.message, error.context);
    return MarkdownParser(source).parse();
}

Result<std::string> mdast::to_plaintext(const Node& root) {
    if (root.kind != "document") return Result<std::string>::failure(ErrorCode::invalid_argument, "Root must be a document", "to_plaintext");
    if (const auto error = root.validate()) return Result<std::string>::failure(error.code, error.message, error.context);
    return Result<std::string>::success(flatten(root), {
        "MDAST -> Plaintext is lossy: formatting, destinations, attributes, and source positions are not preserved"});
}

Result<Document> parse_markdown(std::span<const std::byte> input, std::string_view encoding) {
    auto text = decode_text(input, encoding);
    if (!text) return Result<Document>::failure(text.error.code, text.error.message, text.error.context);
    auto node = mdast::parse(*text.value);
    if (!node) return Result<Document>::failure(node.error.code, node.error.message, node.error.context);
    Document document;
    document.ir = "MDAST";
    document.media_type = "text/markdown";
    document.data = to_bytes(*text.value);
    document.tree = std::make_shared<const Node>(std::move(*node.value));
    return Result<Document>::success(std::move(document));
}
} // namespace doxtk
