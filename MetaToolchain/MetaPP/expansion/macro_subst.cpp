#include "MetaPP-Internal.hpp"
#include <algorithm>
#include <iterator>
#include <filesystem>
namespace metapp {
static std::string stringify(const Tokens &ts) {
    std::string s;
    for (const auto &t : ts) { if (!s.empty() && t.space) s += ' '; s += t.text; }
    return quote(s);
}
Tokens Engine::expand(Tokens ts, unsigned depth) {
    if (depth > 256) throw Error("macro argument nesting exceeds 256");
    for (size_t i = 0; i < ts.size();) {
        if (++steps > 2000000 || ts.size() > max_tokens) throw Error("macro expansion resource limit exceeded");
        Token call = ts[i];
        if (call.kind != Kind::identifier || call.hide.count(call.text)) { ++i; continue; }
        if (call.text == "_Pragma") {
            size_t p = i + 1;
            while (p < ts.size() && ts[p].kind == Kind::newline) ++p;
            if (p == ts.size() || ts[p++].text != "(") fail(call, "_Pragma requires parentheses");
            size_t begin = p;
            unsigned level = 0;
            while (p < ts.size()) {
                if (ts[p].text == ")" && level == 0) break;
                if (ts[p].text == "(") ++level;
                else if (ts[p].text == ")") --level;
                ++p;
            }
            if (p == ts.size()) fail(call, "unterminated _Pragma");
            Tokens arg(ts.begin() + static_cast<ptrdiff_t>(begin), ts.begin() + static_cast<ptrdiff_t>(p));
            arg.erase(std::remove_if(arg.begin(), arg.end(), [](const Token &t) { return t.kind == Kind::newline; }), arg.end());
            arg = expand(std::move(arg), depth + 1);
            if (arg.size() != 1 || arg[0].kind != Kind::string) fail(call, "_Pragma requires one string literal");
            const auto &literal = arg[0].text;
            std::string text;
            for (size_t k = literal.find('"') + 1; k + 1 < literal.size(); ++k) {
                if (literal[k] == '\\' && k + 2 < literal.size() && (literal[k + 1] == '\\' || literal[k + 1] == '"')) ++k;
                text += literal[k];
            }
            auto pragma = lex(text, call.file, false);
            Tokens emitted;
            if (pragma.size() == 1 && pragma[0].text == "once") {
                if (!files.empty()) once.insert(std::filesystem::weakly_canonical(files.back()).string());
            } else emitted = lex("\n#pragma " + text + "\n", call.file, false);
            ts.erase(ts.begin() + static_cast<ptrdiff_t>(i), ts.begin() + static_cast<ptrdiff_t>(p + 1));
            ts.insert(ts.begin() + static_cast<ptrdiff_t>(i), emitted.begin(), emitted.end());
            i += emitted.size(); continue;
        }
        if (builtin(call.text)) {
            auto &t = ts[i++]; t.kind = Kind::number;
            if (call.text == "__LINE__") t.text = std::to_string(call.line);
            else if (call.text == "__FILE__") { t.text = quote(call.file); t.kind = Kind::string; }
            else if (call.text == "__DATE__" || call.text == "__TIME__") {
                t.text = quote(call.text == "__DATE__" ? date : time); t.kind = Kind::string;
            } else if (call.text == "__STDC_VERSION__") t.text = "201112L";
            else t.text = "1";
            continue;
        }
        auto found = macros.find(call.text);
        if (found == macros.end()) { ++i; continue; }
        const Macro &m = found->second;
        size_t end = i + 1;
        std::vector<Tokens> args;
        std::set<std::string> hide = call.hide;
        if (m.function) {
            size_t open = end;
            while (open < ts.size() && ts[open].kind == Kind::newline) ++open;
            if (open == ts.size() || ts[open].text != "(") { ++i; continue; }
            args.emplace_back();
            unsigned nesting = 0;
            bool whitespace = false;
            end = open + 1;
            for (; end < ts.size(); ++end) {
                Token t = ts[end];
                if (t.kind == Kind::newline) { whitespace = true; continue; }
                t.space = t.space || whitespace; whitespace = false;
                if (t.text == ")" && nesting == 0) break;
                if (t.text == "(") ++nesting;
                else if (t.text == ")") --nesting;
                if (t.text == "," && nesting == 0 && (!m.variadic || args.size() < m.params.size())) args.emplace_back();
                else args.back().push_back(std::move(t));
            }
            if (end == ts.size()) fail(call, "unterminated invocation of " + call.text);
            std::set<std::string> intersection;
            std::set_intersection(hide.begin(), hide.end(), ts[end].hide.begin(), ts[end].hide.end(), std::inserter(intersection, intersection.end()));
            hide = std::move(intersection); ++end;
            if (m.params.empty() && args.size() == 1 && args[0].empty()) args.clear();
            if (m.variadic && args.size() + 1 == m.params.size()) args.emplace_back();
            if (args.size() != m.params.size()) fail(call, "wrong argument count for " + call.text);
        }
        hide.insert(call.text);
        std::vector<Tokens> expanded(args.size());
        std::vector<bool> ready(args.size(), false);
        auto param = [&](const Token &t) -> size_t {
            return static_cast<size_t>(std::find(m.params.begin(), m.params.end(), t.text) - m.params.begin());
        };
        Tokens replacement;
        // Paste operators from arguments are data, not replacement operators.
        bool paste = false;
        for (size_t j = 0; j < m.body.size(); ++j) {
            Token t = m.body[j]; t.file = call.file; t.line = call.line;
            if (t.is("##")) { paste = true; continue; }
            Tokens piece;
            if (t.is("#") && m.function) {
                size_t p = param(m.body[++j]); t.text = stringify(args[p]); t.kind = Kind::string; piece.push_back(t);
            } else {
                size_t p = param(t);
                if (p < args.size()) {
                    bool raw = paste || (j + 1 < m.body.size() && m.body[j + 1].is("##"));
                    if (!raw && !ready[p]) { expanded[p] = expand(args[p], depth + 1); ready[p] = true; }
                    piece = raw ? args[p] : expanded[p];
                    if (!piece.empty()) piece.front().space = t.space;
                } else piece.push_back(t);
            }
            if (piece.empty()) { t.text.clear(); t.kind = Kind::placemarker; piece.push_back(t); }
            for (const auto &part : piece) {
                if (part.text.size() > max_text_bytes - expansion_bytes) fail(call, "macro expansion byte limit exceeded");
                expansion_bytes += part.text.size();
            }
            if (paste) {
                if (replacement.empty()) fail(call, "invalid paste operator");
                Token left = replacement.back(), right = piece.front(); replacement.pop_back();
                Token joined = left;
                if (left.kind == Kind::placemarker) joined = right;
                else if (right.kind != Kind::placemarker) {
                    auto parsed = lex(left.text + right.text, call.file, false);
                    if (parsed.size() != 1 || parsed[0].text != left.text + right.text) fail(call, "token paste does not form one preprocessing token");
                    joined = parsed[0]; joined.line = call.line; joined.space = left.space;
                    std::set_intersection(left.hide.begin(), left.hide.end(), right.hide.begin(), right.hide.end(), std::inserter(joined.hide, joined.hide.end()));
                }
                replacement.push_back(std::move(joined)); piece.erase(piece.begin()); paste = false;
            }
            if (piece.size() > max_tokens - replacement.size()) fail(call, "macro expansion token limit exceeded");
            replacement.insert(replacement.end(), piece.begin(), piece.end());
        }
        // Empty arguments and macros are still whitespace boundaries. Preserve
        // their leading space for a subsequent stringification of the result.
        bool empty_space = false;
        for (auto &t : replacement) {
            if (t.kind == Kind::placemarker) empty_space = empty_space || t.space;
            else { t.space = t.space || empty_space; empty_space = false; }
        }
        replacement.erase(std::remove_if(replacement.begin(), replacement.end(), [](const Token &t) { return t.kind == Kind::placemarker; }), replacement.end());
        for (auto &t : replacement) t.hide.insert(hide.begin(), hide.end());
        if (!replacement.empty()) replacement[0].space = call.space;
        if (end < ts.size()) ts[end].space = ts[end].space || empty_space || (replacement.empty() && call.space);
        if (replacement.size() > max_tokens - (ts.size() - (end - i))) fail(call, "macro expansion token limit exceeded");
        ts.erase(ts.begin() + static_cast<ptrdiff_t>(i), ts.begin() + static_cast<ptrdiff_t>(end));
        ts.insert(ts.begin() + static_cast<ptrdiff_t>(i), replacement.begin(), replacement.end());
    }
    return ts;
}
}
