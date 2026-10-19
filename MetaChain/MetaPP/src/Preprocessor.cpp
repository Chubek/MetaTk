#include "MetaPP.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <utility>

namespace metapp {
namespace {
bool ident_start(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool ident_char(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
bool identifier(std::string_view s) {
    return !s.empty() && ident_start(s.front()) && std::all_of(s.begin(), s.end(), ident_char);
}
bool builtin(std::string_view s) {
    return s == "__FILE__" || s == "__LINE__" || s == "__COUNTER__";
}
bool hash(std::string_view s) { return s == "#" || s == "%:"; }
bool paste_operator(std::string_view s) { return s == "##" || s == "%:%:"; }
std::string quote(std::string_view s) {
    std::string result = "\"";
    for (char c : s) {
        switch (c) {
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\v': result += "\\v"; break;
        case '\a': result += "\\a"; break;
        default:
            if (c == '\\' || c == '"') result += '\\';
            if (static_cast<unsigned char>(c) < 32 || c == 127) {
                auto byte = static_cast<unsigned char>(c);
                result += '\\';
                result += static_cast<char>('0' + ((byte >> 6) & 7));
                result += static_cast<char>('0' + ((byte >> 3) & 7));
                result += static_cast<char>('0' + (byte & 7));
            } else result += c;
        }
    }
    return result + '"';
}
Token make_token(std::string text, const Location& where, TokenKind kind = TokenKind::number) {
    return {kind, std::move(text), where, false, {}};
}

std::string filename_literal(const Token& token) {
    std::string result;
    const auto& text = token.text;
    for (std::size_t i = 1; i + 1 < text.size(); ++i) {
        unsigned value = static_cast<unsigned char>(text[i]);
        if (text[i] == '\\') {
            if (++i + 1 >= text.size()) throw Error(token.location, "invalid #line filename escape");
            switch (text[i]) {
            case '\\': case '"': case '\'': case '?': value = static_cast<unsigned char>(text[i]); break;
            case 'n': value = '\n'; break;
            case 'r': value = '\r'; break;
            case 't': value = '\t'; break;
            case 'b': value = '\b'; break;
            case 'f': value = '\f'; break;
            case 'v': value = '\v'; break;
            case 'a': value = '\a'; break;
            default: {
                unsigned base = text[i] == 'x' ? 16 : 8;
                std::size_t begin = text[i] == 'x' ? i + 1 : i;
                std::size_t end = begin;
                value = 0;
                while (end + 1 < text.size() && (base == 16 || end - begin < 3)) {
                    unsigned char c = static_cast<unsigned char>(text[end]);
                    unsigned digit = c >= '0' && c <= '9' ? c - '0' :
                        c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 99;
                    if (digit >= base) break;
                    if (value > (255 - digit) / base) throw Error(token.location, "out-of-range #line filename escape");
                    value = value * base + digit;
                    ++end;
                }
                if (end == begin) throw Error(token.location, "invalid #line filename escape");
                i = end - 1;
            }
            }
        }
        if (value == 0) throw Error(token.location, "NUL in #line filename");
        result += static_cast<char>(value);
    }
    return result;
}

struct ProcessingGuard {
    bool& active;
    ProcessingGuard(bool& flag, Location where) : active(flag) {
        if (active) throw Error(std::move(where), "reentrant preprocessing is not supported");
        active = true;
    }
    ~ProcessingGuard() { active = false; }
};

// Translation phase 2 precedes comment recognition. Retain physical locations even
// when a token or a directive crosses a spliced newline.
Tokens lex(std::string_view input, Location start, bool newlines) {
    std::string source;
    struct Origin { std::size_t offset; std::size_t line; std::size_t column; };
    std::vector<Origin> origins{{0, start.line, start.column}};
    Location at = start;
    auto record_origin = [&] {
        Origin origin{source.size(), at.line, at.column};
        if (origins.back().offset == origin.offset) origins.back() = origin;
        else origins.push_back(origin);
    };
    for (std::size_t i = 0; i < input.size();) {
        if (input[i] == '\\' && i + 1 < input.size() &&
            (input[i + 1] == '\n' || (input[i + 1] == '\r' && i + 2 < input.size() && input[i + 2] == '\n'))) {
            i += input[i + 1] == '\r' ? 3 : 2;
            ++at.line;
            at.column = 1;
            record_origin();
            continue;
        }
        char c = input[i++];
        if (c == '\0') throw Error(at, "NUL byte in source");
        if (c == '\r') {
            if (i < input.size() && input[i] == '\n') ++i;
            c = '\n';
        }
        source += c;
        if (c == '\n') { ++at.line; at.column = 1; record_origin(); }
        else ++at.column;
    }
    auto location = [&](std::size_t offset) {
        auto origin = std::upper_bound(origins.begin(), origins.end(), offset,
            [](std::size_t value, const Origin& entry) { return value < entry.offset; });
        --origin;
        return Location{start.file, origin->line, origin->column + offset - origin->offset};
    };
    Tokens result;
    bool space = false;
    auto newline = [&](std::size_t i) {
        if (newlines) result.push_back(make_token("\n", location(i), TokenKind::punctuation));
        space = true;
    };
    for (std::size_t i = 0; i < source.size();) {
        char c = source[i];
        if (c == '\n') { newline(i++); continue; }
        if (std::isspace(static_cast<unsigned char>(c))) { space = true; ++i; continue; }
        if (source.compare(i, 2, "//") == 0) {
            while (i < source.size() && source[i] != '\n') ++i;
            space = true;
            continue;
        }
        if (source.compare(i, 2, "/*") == 0) {
            auto begin = i;
            i += 2;
            while (i < source.size() && source.compare(i, 2, "*/") != 0) {
                if (source[i] == '\n') newline(i);
                ++i;
            }
            if (i == source.size()) throw Error(location(begin), "unterminated comment");
            i += 2;
            space = true;
            continue;
        }
        auto begin = i;
        TokenKind kind = TokenKind::punctuation;
        // A literal header name is a single, opaque preprocessing token. In
        // particular, its identifiers and comment-like characters are not code.
        bool include_operand = newlines && result.size() >= 2 && result.back().text == "include" &&
            hash(result[result.size() - 2].text) &&
            (result.size() == 2 || result[result.size() - 3].text == "\n");
        if (include_operand && (c == '<' || c == '"')) {
            char delimiter = c == '<' ? '>' : '"';
            ++i;
            while (i < source.size() && source[i] != delimiter && source[i] != '\n') ++i;
            if (i == source.size() || source[i] != delimiter) throw Error(location(begin), "unterminated include path");
            ++i;
            result.push_back({TokenKind::header_name, source.substr(begin, i - begin), location(begin), space, {}});
            space = false;
            continue;
        }
        // C character/string prefixes are part of a single preprocessing token.
        std::size_t prefix = 0;
        if (c == 'L' || c == 'u' || c == 'U') {
            prefix = (c == 'u' && source.compare(i, 2, "u8") == 0) ? 2 : 1;
            if (i + prefix >= source.size() || (source[i + prefix] != '"' && source[i + prefix] != '\'')) prefix = 0;
        }
        if (c == '"' || c == '\'' || prefix) {
            i += prefix;
            char delimiter = source[i++];
            kind = delimiter == '"' ? TokenKind::string_literal : TokenKind::character_literal;
            bool closed = false;
            while (i < source.size() && source[i] != '\n') {
                if (source[i] == delimiter) { ++i; closed = true; break; }
                if (source[i] == '\\' && i + 1 < source.size()) i += 2;
                else ++i;
            }
            if (!closed) throw Error(location(begin), "unterminated literal");
        } else if (ident_start(c)) {
            kind = TokenKind::identifier;
            while (i < source.size() && ident_char(source[i])) ++i;
        } else if (std::isdigit(static_cast<unsigned char>(c)) ||
                   (c == '.' && i + 1 < source.size() && std::isdigit(static_cast<unsigned char>(source[i + 1])))) {
            kind = TokenKind::number;
            ++i;
            while (i < source.size()) {
                char n = source[i];
                if (ident_char(n) || n == '.' || (n == '\'' && i + 1 < source.size() && ident_char(source[i + 1])) ||
                    ((n == '+' || n == '-') && (source[i - 1] == 'e' || source[i - 1] == 'E' || source[i - 1] == 'p' || source[i - 1] == 'P'))) ++i;
                else break;
            }
        } else {
            static constexpr std::string_view operators[] = {
                "%:%:", "...", ">>=", "<<=", "##", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=",
                "&&", "||", "*=", "/=", "%=", "+=", "-=", "&=", "^=", "|=", "<:", ":>", "<%", "%>", "%:"
            };
            std::size_t length = 1;
            for (auto op : operators) if (source.compare(i, op.size(), op) == 0) { length = op.size(); break; }
            i += length;
        }
        std::string text = source.substr(begin, i - begin);
        result.push_back({kind, std::move(text), location(begin), space, {}});
        space = false;
    }
    return result;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw Error({path.string()}, "cannot open input file");
    std::string text{std::istreambuf_iterator<char>(in), {}};
    if (in.bad()) throw Error({path.string()}, "cannot read input file");
    return text;
}
std::filesystem::path normalized_path(const std::filesystem::path& path) {
    std::error_code ec;
    auto result = std::filesystem::weakly_canonical(path, ec);
    return ec ? path.lexically_normal() : result;
}
} // namespace

Error::Error(Location where, const std::string& message)
    : std::runtime_error(where.file + ":" + std::to_string(where.line) + ":" +
                         std::to_string(where.column) + ": " + message), location(std::move(where)) {}

Tokens tokenize(std::string_view source, Location start) { return lex(source, std::move(start), false); }

std::string render(const Tokens& tokens) {
    std::string result;
    for (const auto& token : tokens) {
        if (!result.empty()) result += ' ';
        result += token.text;
    }
    return result;
}

// Installed below, after the integer expression parser has been defined.
namespace { void install_standard_kernels(Preprocessor& pp, const Options& options); }

Preprocessor::Preprocessor(Options options) : options_(std::move(options)) {
    install_standard_kernels(*this, options_);
}

void Preprocessor::reset() {
    warnings_.clear();
    dependencies_.clear();
    once_.clear();
    expansions_ = 0;
    counter_ = 0;
    emitted_tokens_ = 0;
}

void Preprocessor::define(std::string_view definition) {
    auto equal = definition.find('=');
    auto signature = tokenize(definition.substr(0, equal), {"<command-line>"});
    if (signature.empty() || signature.front().kind != TokenKind::identifier ||
        (signature.size() > 1 && (signature[1].text != "(" || signature[1].leading_space || signature.back().text != ")")))
        throw Error({"<command-line>"}, "invalid macro signature");
    if (signature.size() > 1) {
        for (std::size_t i = 2; i + 1 < signature.size(); ++i)
            if (signature[i].text == "(" || signature[i].text == ")") throw Error({"<command-line>"}, "invalid macro signature");
    }
    std::string text(definition.substr(0, equal));
    text += ' ';
    text += equal == std::string_view::npos ? "1" : definition.substr(equal + 1);
    parse_define(tokenize(text, {"<command-line>"}), {"<command-line>"});
}

void Preprocessor::undefine(std::string_view name) {
    if (!identifier(name) || builtin(name)) throw Error({}, "invalid macro name for undef: " + std::string(name));
    macros_.erase(std::string(name));
}
bool Preprocessor::is_defined(std::string_view name) const { return builtin(name) || macros_.contains(std::string(name)); }

void Preprocessor::parse_define(const Tokens& tokens, const Location& location) {
    if (tokens.empty() || tokens[0].kind != TokenKind::identifier || builtin(tokens[0].text) || tokens[0].text == "defined")
        throw Error(location, "expected a non-reserved macro name");
    Macro macro;
    std::size_t i = 1;
    if (i < tokens.size() && tokens[i].text == "(" && !tokens[i].leading_space) {
        macro.function_like = true;
        ++i;
        if (i < tokens.size() && tokens[i].text != ")") {
            for (;;) {
                if (i >= tokens.size()) throw Error(location, "unterminated macro parameter list");
                if (tokens[i].text == "...") { macro.variadic = true; ++i; break; }
                if (tokens[i].kind != TokenKind::identifier || tokens[i].text == "__VA_ARGS__" ||
                    std::find(macro.parameters.begin(), macro.parameters.end(), tokens[i].text) != macro.parameters.end())
                    throw Error(tokens[i].location, "invalid or duplicate macro parameter");
                macro.parameters.push_back(tokens[i++].text);
                if (i >= tokens.size() || tokens[i].text != ",") break;
                ++i;
            }
        }
        if (i >= tokens.size() || tokens[i].text != ")") throw Error(location, "expected ')' in macro definition");
        ++i;
    }
    macro.replacement.assign(tokens.begin() + static_cast<std::ptrdiff_t>(i), tokens.end());
    const auto& r = macro.replacement;
    if (!r.empty() && (paste_operator(r.front().text) || paste_operator(r.back().text))) throw Error(location, "'##' cannot occur at the edge of a replacement");
    for (std::size_t j = 0; j < r.size(); ++j) {
        if (r[j].text == "__VA_ARGS__" && !macro.variadic) throw Error(location, "__VA_ARGS__ requires a variadic macro");
        if (macro.function_like && hash(r[j].text)) {
            if (j + 1 == r.size() ||
                (std::find(macro.parameters.begin(), macro.parameters.end(), r[j + 1].text) == macro.parameters.end() &&
                 !(macro.variadic && r[j + 1].text == "__VA_ARGS__")))
                throw Error(location, "'#' must precede a macro parameter");
        }
    }
    auto old = macros_.find(tokens[0].text);
    if (old != macros_.end()) {
        const auto& m = old->second;
        bool same = m.function_like == macro.function_like && m.variadic == macro.variadic &&
                    m.parameters == macro.parameters && m.replacement.size() == r.size();
        for (std::size_t j = 0; same && j < r.size(); ++j)
            same = m.replacement[j].text == r[j].text && (j == 0 || m.replacement[j].leading_space == r[j].leading_space);
        if (!same) throw Error(location, "incompatible redefinition of macro '" + tokens[0].text + "'");
    }
    macros_[tokens[0].text] = std::move(macro);
}

Tokens Preprocessor::expand(const Tokens& input) {
    if (expansion_depth_ >= options_.max_expansion_depth)
        throw Error(input.empty() ? Location{} : input.front().location, "macro argument nesting limit exceeded");
    struct DepthGuard {
        std::size_t& depth;
        explicit DepthGuard(std::size_t& value) : depth(value) { ++depth; }
        ~DepthGuard() { --depth; }
    } guard(expansion_depth_);
    Tokens work = input;
    for (std::size_t i = 0; i < work.size();) {
        if (work.size() > options_.max_output_tokens) throw Error(work[i].location, "token limit exceeded");
        Token invocation = work[i];
        if (invocation.kind != TokenKind::identifier || invocation.hide_set.contains(invocation.text)) { ++i; continue; }
        if (builtin(invocation.text)) {
            if (++expansions_ > options_.max_expansions) throw Error(invocation.location, "macro expansion limit exceeded");
            if (invocation.text == "__FILE__") work[i] = make_token(quote(invocation.location.file), invocation.location, TokenKind::string_literal);
            else work[i] = make_token(std::to_string(invocation.text == "__LINE__" ? invocation.location.line : counter_++), invocation.location);
            work[i].leading_space = invocation.leading_space;
            work[i].hide_set = invocation.hide_set;
            ++i;
            continue;
        }
        auto found = macros_.find(invocation.text);
        if (found == macros_.end()) { ++i; continue; }
        Macro macro = found->second;
        std::size_t end = i + 1;
        std::vector<Tokens> args;
        Tokens separators;
        std::set<std::string> hide = invocation.hide_set;
        if (macro.function_like) {
            if (end == work.size() || work[end].text != "(") { ++i; continue; }
            ++end;
            args.emplace_back();
            std::size_t nesting = 0;
            for (; end < work.size(); ++end) {
                const auto& t = work[end];
                if (t.text == ")" && nesting == 0) break;
                if (t.text == "," && nesting == 0) { separators.push_back(t); args.emplace_back(); }
                else {
                    if (t.text == "(") ++nesting;
                    if (t.text == ")") --nesting;
                    args.back().push_back(t);
                }
            }
            if (end == work.size()) throw Error(invocation.location, "unterminated invocation of '" + invocation.text + "'");
            std::set<std::string> intersection;
            std::set_intersection(hide.begin(), hide.end(), work[end].hide_set.begin(), work[end].hide_set.end(),
                                  std::inserter(intersection, intersection.end()));
            hide = std::move(intersection);
            ++end;
            if (args.size() == 1 && args[0].empty() && macro.parameters.empty()) args.clear();
            if ((!macro.variadic && args.size() != macro.parameters.size()) ||
                (macro.variadic && args.size() < macro.parameters.size()))
                throw Error(invocation.location, "wrong number of arguments to '" + invocation.text + "'");
        }
        if (++expansions_ > options_.max_expansions) throw Error(invocation.location, "macro expansion limit exceeded");
        hide.insert(invocation.text);
        std::map<std::string, Tokens> raw;
        std::map<std::string, Tokens> expanded;
        for (std::size_t j = 0; j < macro.parameters.size(); ++j) raw[macro.parameters[j]] = args[j];
        if (macro.variadic) {
            Tokens rest;
            for (std::size_t j = macro.parameters.size(); j < args.size(); ++j) {
                if (j > macro.parameters.size()) rest.push_back(separators[j - 1]);
                rest.insert(rest.end(), args[j].begin(), args[j].end());
            }
            raw["__VA_ARGS__"] = std::move(rest);
        }
        // Keep ## operators separate from substituted argument tokens: an argument
        // containing ## must never itself become a paste operator.
        std::vector<Tokens> pieces;
        std::vector<bool> paste;
        std::vector<bool> spaces;
        bool pending_paste = false;
        std::size_t piece_tokens = 0;
        for (std::size_t j = 0; j < macro.replacement.size(); ++j) {
            auto t = macro.replacement[j];
            bool leading_space = t.leading_space;
            t.location = invocation.location;
            if (paste_operator(t.text)) {
                if (pending_paste) throw Error(invocation.location, "adjacent '##' operators");
                pending_paste = true;
                continue;
            }
            Tokens piece;
            if (macro.function_like && hash(t.text)) {
                const auto& argument = raw.at(macro.replacement[++j].text);
                std::string spelling;
                for (const auto& a : argument) {
                    if (!spelling.empty() && a.leading_space) spelling += ' ';
                    spelling += a.text;
                }
                piece.push_back(make_token(quote(spelling), invocation.location, TokenKind::string_literal));
            } else if (raw.contains(t.text)) {
                bool adjacent = pending_paste || (j + 1 < macro.replacement.size() && paste_operator(macro.replacement[j + 1].text));
                if (adjacent) piece = raw.at(t.text);
                else {
                    if (!expanded.contains(t.text)) expanded[t.text] = expand(raw.at(t.text));
                    piece = expanded.at(t.text);
                }
                if (!piece.empty()) piece.front().leading_space = t.leading_space;
            } else piece.push_back(std::move(t));
            if (piece.size() > options_.max_output_tokens - piece_tokens)
                throw Error(invocation.location, "macro replacement token limit exceeded");
            piece_tokens += piece.size();
            pieces.push_back(std::move(piece));
            paste.push_back(pending_paste);
            spaces.push_back(leading_space);
            pending_paste = false;
        }
        Tokens replacement;
        bool pending_space = false;
        auto append_piece = [&](Tokens piece, bool leading_space) {
            if (piece.empty()) { pending_space = pending_space || leading_space; return; }
            piece.front().leading_space = leading_space || pending_space;
            pending_space = false;
            replacement.insert(replacement.end(), piece.begin(), piece.end());
        };
        // Empty pieces act as placemarkers. Only the immediate pieces are pasted.
        for (std::size_t j = 0; j < pieces.size(); ++j) {
            if (j + 1 < pieces.size() && paste[j + 1]) {
                bool leading_space = spaces[j];
                auto combined = std::move(pieces[j]);
                while (j + 1 < pieces.size() && paste[j + 1]) {
                    auto& next = pieces[++j];
                    if (combined.empty()) combined = std::move(next);
                    else if (!next.empty()) {
                        auto joined = tokenize(combined.back().text + next.front().text, invocation.location);
                        if (joined.size() != 1) throw Error(invocation.location, "token paste does not form a preprocessing token");
                        std::set_intersection(combined.back().hide_set.begin(), combined.back().hide_set.end(),
                                              next.front().hide_set.begin(), next.front().hide_set.end(),
                                              std::inserter(joined[0].hide_set, joined[0].hide_set.end()));
                        joined[0].leading_space = combined.back().leading_space;
                        combined.back() = std::move(joined[0]);
                        combined.insert(combined.end(), next.begin() + 1, next.end());
                    }
                }
                append_piece(std::move(combined), leading_space);
            } else append_piece(std::move(pieces[j]), spaces[j]);
        }
        for (auto& t : replacement) t.hide_set.insert(hide.begin(), hide.end());
        if (!replacement.empty()) replacement.front().leading_space = invocation.leading_space;
        if (end < work.size() && (pending_space || (replacement.empty() && invocation.leading_space)))
            work[end].leading_space = true;
        if (replacement.size() > options_.max_output_tokens - (work.size() - (end - i)))
            throw Error(invocation.location, "macro replacement token limit exceeded");
        work.erase(work.begin() + static_cast<std::ptrdiff_t>(i), work.begin() + static_cast<std::ptrdiff_t>(end));
        work.insert(work.begin() + static_cast<std::ptrdiff_t>(i), replacement.begin(), replacement.end());
    }
    return work;
}

void Preprocessor::register_directive(std::string name, Directive handler) {
    static const std::set<std::string> reserved = {"define", "undef", "if", "ifdef", "ifndef", "elif", "else", "endif", "include", "line", "error", "warning", "pragma", "kernel", "rewrite"};
    if (!identifier(name) || reserved.contains(name) || !handler) throw std::invalid_argument("invalid or reserved directive name");
    directives_[std::move(name)] = std::make_shared<Directive>(std::move(handler));
}
void Preprocessor::register_kernel(std::string name, Kernel handler) {
    if (!identifier(name) || !handler) throw std::invalid_argument("invalid kernel name");
    kernels_[std::move(name)] = std::make_shared<Kernel>(std::move(handler));
}
void Preprocessor::add_rewrite(Tokens pattern, Tokens replacement) {
    if (pattern.empty()) throw std::invalid_argument("rewrite pattern must not be empty");
    rewrites_.push_back({std::move(pattern), std::move(replacement)});
}
void Preprocessor::clear_rewrites() { rewrites_.clear(); }

Tokens Preprocessor::rewrite(Tokens tokens) const {
    Tokens output;
    for (std::size_t i = 0; i < tokens.size();) {
        bool matched = false;
        for (const auto& rule : rewrites_) {
            if (rule.pattern.size() > tokens.size() - i) continue;
            bool equal = true;
            for (std::size_t j = 0; equal && j < rule.pattern.size(); ++j)
                equal = rule.pattern[j].kind == tokens[i + j].kind && rule.pattern[j].text == tokens[i + j].text;
            if (!equal) continue;
            if (rule.replacement.size() > options_.max_output_tokens - output.size())
                throw Error(tokens[i].location, "rewrite token limit exceeded");
            for (auto t : rule.replacement) {
                t.location = tokens[i].location;
                output.push_back(std::move(t));
            }
            i += rule.pattern.size();
            matched = true;
            break;
        }
        if (!matched) output.push_back(tokens[i++]);
        if (output.size() > options_.max_output_tokens) throw Error(tokens[i - 1].location, "rewrite token limit exceeded");
    }
    return output;
}

std::filesystem::path Preprocessor::resolve_include(std::string_view name, bool quoted,
        const std::string& current, const Location& location) const {
    if (name.find('\0') != std::string_view::npos) throw Error(location, "NUL in include path");
    std::vector<std::filesystem::path> candidates;
    auto requested = std::filesystem::path(name);
    if (requested.is_absolute()) candidates.push_back(requested);
    else {
        if (quoted) candidates.push_back(std::filesystem::path(current).parent_path() / requested);
        for (const auto& dir : options_.include_paths) candidates.push_back(dir / requested);
    }
    for (const auto& path : candidates) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(path, ec)) return normalized_path(path);
    }
    throw Error(location, "include file not found: " + std::string(name));
}

std::string Preprocessor::include_file(const std::filesystem::path& path, std::size_t depth) {
    auto normalized = normalized_path(path);
    if (once_.contains(normalized)) return {};
    if (depth > options_.max_include_depth) throw Error({path.string()}, "include depth limit exceeded");
    auto source = read_file(path);
    if (std::find(dependencies_.begin(), dependencies_.end(), normalized) == dependencies_.end()) dependencies_.push_back(normalized);
    return process_source(source, normalized.string(), depth);
}

std::string Preprocessor::process(std::string_view source, std::string filename) {
    ProcessingGuard guard(processing_, {filename});
    reset();
    return process_source(source, filename, 0);
}
std::string Preprocessor::process_file(const std::filesystem::path& path) {
    ProcessingGuard guard(processing_, {path.string()});
    reset();
    return include_file(path, 0);
}

namespace {
struct Integer {
    std::uint64_t bits = 0;
    bool unsigned_value = false;
    bool truth() const { return bits != 0; }
    std::int64_t signed_value() const {
        // Avoid implementation-defined conversion of an out-of-range unsigned value.
        if (bits <= static_cast<std::uint64_t>(INT64_MAX)) return static_cast<std::int64_t>(bits);
        return -1 - static_cast<std::int64_t>(~bits);
    }
};

Integer literal(const Token& token) {
    if (token.kind == TokenKind::character_literal) {
        auto first = token.text.find('\'');
        auto content = std::string_view(token.text).substr(first + 1, token.text.size() - first - 2);
        std::uint64_t value = 0;
        for (std::size_t i = 0; i < content.size(); ++i) {
            unsigned c = static_cast<unsigned char>(content[i]);
            if (c == '\\') {
                if (++i == content.size()) throw Error(token.location, "invalid character escape");
                char e = content[i];
                switch (e) {
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case 'a': c = '\a'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'v': c = '\v'; break;
                case '\\': case '\'': case '"': case '?': c = static_cast<unsigned char>(e); break;
                default: {
                    int base = e == 'x' ? 16 : 8;
                    std::size_t begin = e == 'x' ? i + 1 : i;
                    std::size_t end = begin;
                    unsigned number = 0;
                    while (end < content.size() && (base == 16 || end - begin < 3)) {
                        char ch = content[end];
                        unsigned digit = ch >= '0' && ch <= '9' ? static_cast<unsigned>(ch - '0') :
                            ch >= 'a' && ch <= 'f' ? static_cast<unsigned>(ch - 'a' + 10) :
                            ch >= 'A' && ch <= 'F' ? static_cast<unsigned>(ch - 'A' + 10) : 99;
                        if (digit >= static_cast<unsigned>(base)) break;
                        number = number * static_cast<unsigned>(base) + digit;
                        ++end;
                    }
                    if (end == begin) throw Error(token.location, "invalid character escape");
                    c = number;
                    i = end - 1;
                }
                }
            }
            value = (value << 8) | (c & 255);
        }
        if (content.empty()) throw Error(token.location, "empty character constant");
        return {value, false};
    }
    if (token.kind != TokenKind::number) throw Error(token.location, "expected integer in preprocessor expression");
    std::string s = token.text;
    int base = 10;
    std::size_t start = 0;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; start = 2; }
    else if (s.size() > 2 && s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) { base = 2; start = 2; }
    else if (s.size() > 1 && s[0] == '0') base = 8;
    auto digit = [base](char c) {
        unsigned value = c >= '0' && c <= '9' ? static_cast<unsigned>(c - '0') :
            c >= 'a' && c <= 'f' ? static_cast<unsigned>(c - 'a' + 10) :
            c >= 'A' && c <= 'F' ? static_cast<unsigned>(c - 'A' + 10) : 99;
        return value < static_cast<unsigned>(base);
    };
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\'' && (i == 0 || i + 1 == s.size() || !digit(s[i - 1]) || !digit(s[i + 1])))
            throw Error(token.location, "invalid digit separator in integer constant");
    }
    s.erase(std::remove(s.begin(), s.end(), '\''), s.end());
    std::uint64_t number = 0;
    auto [end, ec] = std::from_chars(s.data() + start, s.data() + s.size(), number, base);
    std::string suffix(end, static_cast<std::size_t>(s.data() + s.size() - end));
    if (suffix.find("lL") != std::string::npos || suffix.find("Ll") != std::string::npos)
        throw Error(token.location, "invalid integer suffix: " + token.text);
    std::transform(suffix.begin(), suffix.end(), suffix.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    static const std::set<std::string> suffixes = {"", "u", "l", "ul", "lu", "ll", "ull", "llu"};
    if (ec != std::errc{} || !suffixes.contains(suffix)) throw Error(token.location, "invalid integer constant: " + token.text);
    return {number, suffix.find('u') != std::string::npos || number > static_cast<std::uint64_t>(INT64_MAX)};
}

class Expression {
    const Tokens& tokens_;
    std::size_t pos_ = 0;
    std::size_t depth_ = 0;
    std::size_t max_depth_;
    struct Guard {
        Expression& expression;
        explicit Guard(Expression& e) : expression(e) {
            if (e.depth_ >= e.max_depth_) throw Error(e.location(), "expression nesting limit exceeded");
            ++e.depth_;
        }
        ~Guard() { --expression.depth_; }
    };
    Location location() const { return tokens_[std::min(pos_, tokens_.size() - 1)].location; }
    bool take(std::string_view s) {
        if (pos_ < tokens_.size() && tokens_[pos_].text == s) { ++pos_; return true; }
        return false;
    }
    static int precedence(std::string_view op) {
        if (op == "||") return 1;
        if (op == "&&") return 2;
        if (op == "|") return 3;
        if (op == "^") return 4;
        if (op == "&") return 5;
        if (op == "==" || op == "!=") return 6;
        if (op == "<" || op == ">" || op == "<=" || op == ">=") return 7;
        if (op == "<<" || op == ">>") return 8;
        if (op == "+" || op == "-") return 9;
        if (op == "*" || op == "/" || op == "%") return 10;
        return 0;
    }
    Integer unary(bool evaluate) {
        Guard guard(*this);
        if (take("!")) return {unary(evaluate).truth() ? 0u : 1u, false};
        if (take("~")) { auto n = unary(evaluate); n.bits = ~n.bits; return n; }
        if (take("+")) return unary(evaluate);
        if (take("-")) { auto n = unary(evaluate); n.bits = 0 - n.bits; return n; }
        if (take("(")) {
            auto n = conditional(evaluate);
            if (!take(")")) throw Error(location(), "expected ')' in expression");
            return n;
        }
        if (pos_ == tokens_.size()) throw Error(location(), "missing operand in expression");
        return literal(tokens_[pos_++]);
    }
    Integer binary(int minimum, bool evaluate) {
        auto left = unary(evaluate);
        while (pos_ < tokens_.size()) {
            auto op = tokens_[pos_].text;
            int prec = precedence(op);
            if (prec < minimum) break;
            auto where = tokens_[pos_++].location;
            bool right_evaluate = evaluate && !(op == "&&" && !left.truth()) && !(op == "||" && left.truth());
            auto right = binary(prec + 1, right_evaluate);
            bool u = left.unsigned_value || right.unsigned_value;
            auto a = left.bits;
            auto b = right.bits;
            auto sa = left.signed_value();
            auto sb = right.signed_value();
            Integer value{0, u};
            if (op == "+") value.bits = a + b;
            else if (op == "-") value.bits = a - b;
            else if (op == "*") value.bits = a * b;
            else if (op == "/" || op == "%") {
                if (evaluate && b == 0) throw Error(where, "division by zero in preprocessor expression");
                if (evaluate && !u && sa == INT64_MIN && sb == -1) throw Error(where, "signed division overflow");
                if (evaluate) value.bits = u ? (op == "/" ? a / b : a % b) :
                    static_cast<std::uint64_t>(op == "/" ? sa / sb : sa % sb);
            } else if (op == "<<" || op == ">>") {
                value.unsigned_value = left.unsigned_value;
                if (evaluate && b >= 64) throw Error(where, "invalid shift count");
                if (evaluate) {
                    if (op == "<<") value.bits = a << b;
                    else if (!left.unsigned_value && sa < 0 && b != 0) value.bits = (a >> b) | (~std::uint64_t{0} << (64 - b));
                    else value.bits = a >> b;
                }
            } else if (op == "&") value.bits = a & b;
            else if (op == "|") value.bits = a | b;
            else if (op == "^") value.bits = a ^ b;
            else {
                value.unsigned_value = false;
                if (op == "==") value.bits = a == b;
                else if (op == "!=") value.bits = a != b;
                else if (op == "<") value.bits = u ? a < b : sa < sb;
                else if (op == ">") value.bits = u ? a > b : sa > sb;
                else if (op == "<=") value.bits = u ? a <= b : sa <= sb;
                else if (op == ">=") value.bits = u ? a >= b : sa >= sb;
                else if (op == "&&") value.bits = left.truth() && right.truth();
                else if (op == "||") value.bits = left.truth() || right.truth();
            }
            left = value;
        }
        return left;
    }
    Integer conditional(bool evaluate) {
        Guard guard(*this);
        auto condition = binary(1, evaluate);
        if (!take("?")) return condition;
        auto yes = conditional(evaluate && condition.truth());
        if (!take(":")) throw Error(location(), "expected ':' in conditional expression");
        auto no = conditional(evaluate && !condition.truth());
        auto result = condition.truth() ? yes : no;
        result.unsigned_value = yes.unsigned_value || no.unsigned_value;
        return result;
    }
public:
    Expression(const Tokens& tokens, std::size_t max_depth) : tokens_(tokens), max_depth_(max_depth) {}
    Integer run() {
        if (tokens_.empty()) throw Error({}, "empty integer expression");
        auto result = conditional(true);
        if (pos_ != tokens_.size()) throw Error(location(), "unexpected token in expression: " + tokens_[pos_].text);
        return result;
    }
};

void install_standard_kernels(Preprocessor& pp, const Options& options) {
    pp.register_kernel("eval", [limit = options.max_expression_depth](const Tokens& args, const Location& where) {
        if (args.empty()) throw Error(where, "empty integer expression");
        auto result = Expression(args, limit).run();
        std::string text = result.unsigned_value ? std::to_string(result.bits) + "U" : std::to_string(result.signed_value());
        return tokenize(text, where);
    });
    pp.register_kernel("stringify", [](const Tokens& args, const Location& where) {
        return Tokens{make_token(quote(render(args)), where, TokenKind::string_literal)};
    });
    pp.register_kernel("concat", [](const Tokens& args, const Location& where) {
        std::string text;
        for (const auto& t : args) text += t.text;
        auto result = tokenize(text, where);
        if (result.size() != 1) throw Error(where, "concat kernel requires exactly one resulting token");
        return result;
    });
}
} // namespace

bool Preprocessor::evaluate(const Tokens& expression, const Location& location) {
    if (expression.empty()) throw Error(location, "empty preprocessor expression");
    Tokens prepared;
    for (std::size_t i = 0; i < expression.size(); ++i) {
        if (expression[i].text != "defined") { prepared.push_back(expression[i]); continue; }
        auto where = expression[i].location;
        ++i;
        bool paren = i < expression.size() && expression[i].text == "(";
        if (paren) ++i;
        if (i == expression.size() || expression[i].kind != TokenKind::identifier) throw Error(where, "expected identifier after defined");
        bool value = is_defined(expression[i].text);
        if (paren && (++i == expression.size() || expression[i].text != ")")) throw Error(where, "expected ')' after defined");
        prepared.push_back(make_token(value ? "1" : "0", where));
    }
    prepared = expand(prepared);
    for (auto& t : prepared) if (t.kind == TokenKind::identifier) t = make_token("0", t.location);
    if (prepared.empty()) throw Error(expression.front().location, "empty expanded expression");
    return Expression(prepared, options_.max_expression_depth).run().truth();
}

std::string Preprocessor::process_source(std::string_view source, const std::string& filename, std::size_t depth) {
    auto all = lex(source, {filename}, true);
    struct Conditional { bool parent; bool taken; bool active; bool seen_else; Location location; };
    std::vector<Conditional> conditionals;
    Tokens pending;
    std::string output;
    std::string logical_file = filename;
    std::int64_t line_offset = 0;
    auto active = [&] { return conditionals.empty() || conditionals.back().active; };
    auto emit = [&](Tokens tokens) {
        tokens = rewrite(expand(tokens));
        if (tokens.size() > options_.max_output_tokens - emitted_tokens_)
            throw Error(tokens.front().location, "total output token limit exceeded");
        emitted_tokens_ += tokens.size();
        if (!tokens.empty()) output += render(tokens) + '\n';
    };
    auto flush = [&] { if (!pending.empty()) { emit(std::move(pending)); pending.clear(); } };
    for (std::size_t pos = 0; pos < all.size();) {
        Tokens line;
        while (pos < all.size() && all[pos].text != "\n") line.push_back(all[pos++]);
        std::size_t physical_next = pos < all.size() ? all[pos].location.line + 1 : (line.empty() ? 1 : line.back().location.line + 1);
        if (pos < all.size()) ++pos;
        for (auto& t : line) {
            t.location.file = logical_file;
            t.location.line = static_cast<std::size_t>(static_cast<std::int64_t>(t.location.line) + line_offset);
        }
        if (line.empty()) continue;
        if (!hash(line.front().text)) {
            if (active()) {
                if (!pending.empty()) line.front().leading_space = true;
                pending.insert(pending.end(), line.begin(), line.end());
            }
            continue;
        }
        flush();
        if (line.size() == 1) continue;
        const auto where = line[0].location;
        const std::string command = line[1].text;
        Tokens args(line.begin() + 2, line.end());
        auto require_identifier = [&] {
            if (args.size() != 1 || args[0].kind != TokenKind::identifier) throw Error(where, "expected a single identifier after #" + command);
        };
        auto require_empty = [&] { if (!args.empty()) throw Error(where, "unexpected tokens after #" + command); };
        if (command == "if" || command == "ifdef" || command == "ifndef") {
            bool parent = active();
            bool value = false;
            if (command != "if") {
                require_identifier();
                value = is_defined(args[0].text);
                if (command == "ifndef") value = !value;
            } else if (parent) value = evaluate(args, where);
            conditionals.push_back({parent, parent && value, parent && value, false, where});
        } else if (command == "elif" || command == "else" || command == "endif") {
            if (conditionals.empty()) throw Error(where, "unmatched #" + command);
            auto& state = conditionals.back();
            if (command == "endif") { require_empty(); conditionals.pop_back(); }
            else {
                if (state.seen_else) throw Error(where, "#" + command + " after #else");
                if (command == "else") { require_empty(); state.seen_else = true; }
                bool value = state.parent && !state.taken && (command == "else" || evaluate(args, where));
                state.active = value;
                state.taken = state.taken || value;
            }
        } else if (!active()) continue;
        else if (command == "define") parse_define(args, where);
        else if (command == "undef") {
            require_identifier();
            if (builtin(args[0].text)) throw Error(where, "cannot undefine a builtin macro");
            undefine(args[0].text);
        }
        else if (command == "include") {
            bool literal = !args.empty() && args.front().kind == TokenKind::header_name;
            if (literal && args.size() != 1) throw Error(where, "unexpected tokens after include path");
            auto expanded = literal ? args : expand(args);
            bool quoted = expanded.size() == 1 && expanded[0].kind == TokenKind::string_literal && expanded[0].text.front() == '"';
            std::string name;
            if (literal) {
                quoted = expanded[0].text.front() == '"';
                name = expanded[0].text.substr(1, expanded[0].text.size() - 2);
            } else if (quoted) name = expanded[0].text.substr(1, expanded[0].text.size() - 2);
            else if (expanded.size() >= 3 && expanded.front().text == "<" && expanded.back().text == ">") {
                for (std::size_t j = 1; j + 1 < expanded.size(); ++j) {
                    if (j > 1 && expanded[j].leading_space) name += ' ';
                    name += expanded[j].text;
                }
            } else throw Error(where, "expected a quoted or angle-bracket include path");
            if (name.empty()) throw Error(where, "empty include path");
            output += include_file(resolve_include(name, quoted, filename, where), depth + 1);
        } else if (command == "error") throw Error(where, "#error " + render(args));
        else if (command == "warning") warnings_.push_back({where, render(args)});
        else if (command == "pragma") {
            if (args.size() == 1 && args[0].text == "once") once_.insert(normalized_path(filename));
            else warnings_.push_back({where, "ignored pragma: " + render(args)});
        } else if (command == "line") {
            auto values = expand(args);
            std::size_t number = 0;
            if (values.empty() || values.size() > 2) throw Error(where, "invalid #line directive");
            auto [end, ec] = std::from_chars(values[0].text.data(), values[0].text.data() + values[0].text.size(), number);
            if (ec != std::errc{} || end != values[0].text.data() + values[0].text.size() || number == 0 || number > 2147483647)
                throw Error(where, "invalid #line number");
            if (values.size() == 2) {
                if (values[1].kind != TokenKind::string_literal || values[1].text.front() != '"') throw Error(where, "invalid #line filename");
                logical_file = filename_literal(values[1]);
            }
            line_offset = static_cast<std::int64_t>(number) - static_cast<std::int64_t>(physical_next);
        } else if (command == "kernel") {
            if (args.empty() || !kernels_.contains(args.front().text)) throw Error(where, "unknown or missing kernel name");
            auto name = args.front().text;
            args.erase(args.begin());
            auto handler = kernels_.at(name);
            auto result = (*handler)(expand(args), where);
            emit(std::move(result));
        } else if (command == "rewrite") {
            auto delimiter = std::find_if(args.begin(), args.end(), [](const Token& t) { return t.text == "=>"; });
            // The C lexer represents => as two punctuation tokens.
            if (delimiter == args.end()) {
                for (auto it = args.begin(); it != args.end(); ++it) {
                    if (it->text == "=" && it + 1 != args.end() && (it + 1)->text == ">") { delimiter = it; break; }
                }
            }
            if (delimiter == args.end() || delimiter == args.begin()) throw Error(where, "expected #rewrite pattern => replacement");
            auto skip = delimiter->text == "=>" ? 1 : 2;
            add_rewrite(Tokens(args.begin(), delimiter), Tokens(delimiter + skip, args.end()));
        } else if (directives_.contains(command)) {
            auto handler = directives_.at(command);
            emit((*handler)(*this, args, where));
        }
        else throw Error(where, "unknown directive #" + command);
    }
    flush();
    if (!conditionals.empty()) throw Error(conditionals.back().location, "unterminated conditional directive");
    return output;
}

} // namespace metapp
