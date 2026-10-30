#include "MetaPP-Internal.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
namespace metapp {
[[noreturn]] void fail(const Token &at, const std::string &message) {
    throw Error(at.file + ":" + std::to_string(at.line) + ": error: " + message);
}
static bool digit(unsigned char c) { return c >= '0' && c <= '9'; }
static bool start(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 128; }
static bool part(unsigned char c) { return start(c) || digit(c); }
bool identifier(const std::string &s) {
    return !s.empty() && start(static_cast<unsigned char>(s[0])) &&
        std::all_of(s.begin() + 1, s.end(), [](unsigned char c) { return part(c); });
}
Tokens lex(const std::string &source, const std::string &file, bool source_phases) {
    if (source.size() > max_text_bytes) throw Error(file + ": input exceeds 64 MiB limit");
    std::string normalized;
    const std::string tri = "=/'()!<>-", replacement = "#\\^[]|{}~";
    for (size_t i = 0; i < source.size(); ++i) {
        char c = source[i];
        if (c == '\0') throw Error(file + ": NUL byte in source");
        if (c == '\r') { if (i + 1 < source.size() && source[i + 1] == '\n') ++i; c = '\n'; }
        if (source_phases && c == '?' && i + 2 < source.size() && source[i + 1] == '?') {
            auto p = tri.find(source[i + 2]);
            if (p != std::string::npos) { c = replacement[p]; i += 2; }
        }
        normalized += c;
    }
    std::string s;
    std::vector<unsigned> lines;
    unsigned line = 1;
    for (size_t i = 0; i < normalized.size(); ++i) {
        if (source_phases && normalized[i] == '\\' && i + 1 < normalized.size() && normalized[i + 1] == '\n') { ++line; ++i; continue; }
        lines.push_back(line); s += normalized[i];
        if (normalized[i] == '\n') ++line;
    }
    Tokens result;
    bool space = false;
    auto add = [&](Kind kind, size_t begin, size_t end, std::string text = {}) {
        if (result.size() == max_tokens) throw Error(file + ": token limit exceeded");
        Token t; t.kind = kind; t.text = text.empty() ? s.substr(begin, end - begin) : text;
        t.space = space; t.line = lines[begin]; t.file = file;
        result.push_back(std::move(t)); space = false;
    };
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '\n') { add(Kind::newline, i, i + 1); ++i; space = true; continue; }
        if (s[i] == ' ' || s[i] == '\t' || s[i] == '\v' || s[i] == '\f') { space = true; ++i; continue; }
        if (s.compare(i, 2, "//") == 0) {
            while (i < s.size() && s[i] != '\n') ++i;
            space = true; continue;
        }
        if (s.compare(i, 2, "/*") == 0) {
            size_t begin = i; i += 2;
            while (i < s.size() && s.compare(i, 2, "*/") != 0) {
                ++i;
            }
            if (i == s.size()) { Token t; t.file = file; t.line = lines[begin]; fail(t, "unterminated comment"); }
            i += 2; space = true; continue;
        }
        size_t begin = i, q = i;
        // Header names have their own lexical rules: // and /* inside <...>
        // are filename characters, not comments.
        if (s[i] == '<' && result.size() >= 2 && result.back().text == "include" &&
            result[result.size() - 2].is("#") &&
            (result.size() == 2 || result[result.size() - 3].kind == Kind::newline)) {
            size_t end = i + 1;
            while (end < s.size() && s[end] != '>' && s[end] != '\n') ++end;
            if (end < s.size() && s[end] == '>') { add(Kind::header, i, end + 1); i = end + 1; continue; }
        }
        if (s[i] == 'L' || s[i] == 'u' || s[i] == 'U') {
            ++q; if (s[i] == 'u' && q < s.size() && s[q] == '8') ++q;
        }
        if (q < s.size() && (s[q] == '"' || s[q] == '\'')) {
            char delimiter = s[q]; i = q + 1;
            while (i < s.size() && s[i] != delimiter && s[i] != '\n') {
                if (s[i] == '\\' && i + 1 < s.size()) ++i;
                ++i;
            }
            if (i == s.size() || s[i] != delimiter) { Token t; t.file = file; t.line = lines[begin]; fail(t, "unterminated literal"); }
            add(delimiter == '"' ? Kind::string : Kind::character, begin, ++i); continue;
        }
        if (start(static_cast<unsigned char>(s[i]))) {
            while (++i < s.size() && part(static_cast<unsigned char>(s[i]))) {}
            add(Kind::identifier, begin, i); continue;
        }
        if (digit(static_cast<unsigned char>(s[i])) ||
            (s[i] == '.' && i + 1 < s.size() && digit(static_cast<unsigned char>(s[i + 1])))) {
            while (++i < s.size()) {
                char c = s[i], prev = s[i - 1];
                if (part(static_cast<unsigned char>(c)) || c == '.' ||
                    ((c == '+' || c == '-') && (prev == 'e' || prev == 'E' || prev == 'p' || prev == 'P'))) continue;
                break;
            }
            add(Kind::number, begin, i); continue;
        }
        static const char *puncts[] = {"%:%:", ">>=", "<<=", "...", "##", "->", "++", "--", "<<", ">>",
            "<=", ">=", "==", "!=", "&&", "||", "*=", "/=", "%=", "+=", "-=", "&=", "^=", "|=", "<:", ":>", "<%", "%>", "%:"};
        std::string p(1, s[i]);
        for (const char *candidate : puncts) if (s.compare(i, std::char_traits<char>::length(candidate), candidate) == 0) { p = candidate; break; }
        i += p.size();
        add(Kind::punct, begin, i, p);
    }
    return result;
}
std::string quote(const std::string &s) {
    std::string result = "\"";
    for (unsigned char c : s) {
        if (c == '\\' || c == '"') {
            char escaped[] = {'\\', static_cast<char>(c)};
            append_text(result, std::string_view(escaped, sizeof escaped));
        } else if (c < 32 || c == 127) {
            // Three octal digits keep a following digit from joining the escape.
            char escaped[] = {'\\', char('0' + (c >> 6)), char('0' + ((c >> 3) & 7)), char('0' + (c & 7))};
            append_text(result, std::string_view(escaped, sizeof escaped));
        } else {
            char byte = static_cast<char>(c);
            append_text(result, std::string_view(&byte, 1));
        }
    }
    append_text(result, "\""); return result;
}
void append_text(std::string &out, std::string_view text) {
    if (out.size() > max_text_bytes || text.size() > max_text_bytes - out.size())
        throw Error("output exceeds 64 MiB limit");
    out.append(text.data(), text.size());
}
std::string render(const Tokens &tokens) {
    std::string result;
    bool first = true;
    for (const auto &t : tokens) {
        if (t.kind == Kind::newline) { append_text(result, "\n"); first = true; }
        else if (t.kind != Kind::placemarker) {
            if (!first) append_text(result, " ");
            append_text(result, t.text); first = false;
        }
    }
    return result;
}
std::string read_file(const std::string &path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw Error(path + ": cannot open input");
    std::string data;
    char buffer[8192];
    while (stream.read(buffer, sizeof buffer) || stream.gcount()) {
        data.append(buffer, static_cast<size_t>(stream.gcount()));
        if (data.size() > 64 * 1024 * 1024) throw Error(path + ": input exceeds 64 MiB limit");
    }
    if (!stream.eof()) throw Error(path + ": input read failed");
    return data;
}
}
