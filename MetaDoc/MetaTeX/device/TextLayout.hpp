#pragma once
#include "MetaTeX.h"
#include "../utils/UTF8.hpp"
#include <algorithm>
namespace metatex::detail {
inline std::string blockText(const Document& doc, const Block& block) {
    std::string result;
    for (const auto& s : block.spans) {
        if (s.kind == SpanKind::Link && s.extra.starts_with("ref:")) {
            auto found = doc.references.find(s.extra.substr(4));
            result += found == doc.references.end() ? "??" : found->second;
        } else if (s.kind == SpanKind::Image) result += "[image: " + s.extra + "]";
        else result += s.text;
    }
    return result;
}
// Semantic-device wrapping by Unicode scalar count, not TeX layout.
inline std::vector<std::string> textLines(std::string_view text, std::size_t width) {
    width = std::max<std::size_t>(1, width);
    std::vector<std::string> lines; std::string line; std::size_t count = 0;
    for (std::size_t at = 0; at < text.size();) {
        auto start = at; auto code = util::nextUTF8(text, at);
        if (code == '\r') continue;
        if (code == '\n') { lines.push_back(std::move(line)); line.clear(); count = 0; continue; }
        if (count == width) {
            auto space = line.find_last_of(' ');
            if (space != std::string::npos && space > line.size() / 2) {
                auto tail = line.substr(space + 1); line.resize(space); lines.push_back(std::move(line)); line = std::move(tail);
                count = 0; for (std::size_t i = 0; i < line.size(); ++count) (void)util::nextUTF8(line, i);
            } else { lines.push_back(std::move(line)); line.clear(); count = 0; }
        }
        line.append(code == '\t' ? " " : text.substr(start, at - start)); ++count;
    }
    if (!line.empty() || lines.empty()) lines.push_back(std::move(line));
    return lines;
}
inline std::string psString(std::string_view text) {
    std::string result;
    for (std::size_t at = 0; at < text.size();) {
        auto code = util::nextUTF8(text, at);
        if (code > 255 || code < 32 || (code >= 127 && code < 160)) code = '?';
        if (code == '(' || code == ')' || code == '\\') result += '\\';
        if (code >= 128) {
            result += '\\'; result += static_cast<char>('0' + ((code >> 6) & 7));
            result += static_cast<char>('0' + ((code >> 3) & 7)); result += static_cast<char>('0' + (code & 7));
        } else result += static_cast<char>(code);
    }
    return result;
}
}
