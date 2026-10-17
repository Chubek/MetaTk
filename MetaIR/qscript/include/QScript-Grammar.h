#ifndef METACHAIN_QSCRIPT_GRAMMAR_H
#define METACHAIN_QSCRIPT_GRAMMAR_H
// QScript grammars: Raku-style ("Perl 6") scannerless grammars on ParseTk.
//
//   grammar Calc {
//       rule  TOP    { <expr> }
//       rule  expr   { <term>+ % <op> }
//       token term   { \d+ | '(' ~ ')' <expr> }
//       proto token op {*}
//       token op:sym<+>  { <sym> }
//       token op:sym<->  { <sym> }
//   }
//
// There is no separate lexer: every declaration matches characters directly.
// `token` matches exactly what it says, `rule` additionally turns source
// whitespace after an atom into an implicit <.ws> (Raku :sigspace), and
// `regex` is accepted as a synonym of `token`. Each declaration is translated
// to a ParseTk production (character classes become ParseTk regex lexemes)
// and parsed with whitespace skipping disabled, so ParseTk acts as a
// character-level PEG engine. Alternation is ordered (PEG), except that the
// variants of a `proto` are tried longest-literal-prefix first, which gives
// the longest-token behaviour operators need (`<=` before `<`).
//
// A successful parse yields a match tree (qs_match) of named sub-rule
// captures only: literals and character classes are not captured, `<.name>`
// suppresses a capture, `<alias=name>` renames one, and proto variants
// appear under the proto name with their `sym` recorded. See
// manual/qscript/05-grammars.md for the full syntax.
#include "qscript/QScript.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct qs_grammar qs_grammar;
typedef struct qs_match qs_match;
typedef struct qs_span { size_t from, to, line, column; } qs_span;
/* Return nonzero to descend into the match's children, zero to prune. */
typedef int (*qs_match_visitor)(const qs_match *match, size_t depth, void *user);

/* Compile grammar source. On failure returns NULL; see qs_error(context). */
qs_grammar *qs_grammar_compile(qs_context *, const char *source, size_t length);
/* The grammar QScript itself is parsed with (source text and compiled). */
const char *qs_grammar_qscript_source(void);
qs_grammar *qs_grammar_qscript(qs_context *);
void qs_grammar_destroy(qs_grammar *);
const char *qs_grammar_name(const qs_grammar *);
size_t qs_grammar_rule_count(const qs_grammar *);
const char *qs_grammar_rule_name(const qs_grammar *, size_t index);
/* The generated ParseTk grammar, for debugging. Buffer owned by context. */
qs_status qs_grammar_peg(qs_context *, const qs_grammar *, const char **text, size_t *length);

/* Parse input from a start rule (NULL means "TOP"). The whole input must
   match. Returns a root match to release with qs_match_destroy, or NULL. */
qs_match *qs_grammar_parse(qs_context *, const qs_grammar *, const char *input, size_t length, const char *start);
/* Only roots returned by qs_grammar_parse may be destroyed. */
void qs_match_destroy(qs_match *);
const char *qs_match_name(const qs_match *);
const char *qs_match_sym(const qs_match *);      /* "" unless a proto variant */
const char *qs_match_text(const qs_match *, size_t *length);
qs_status qs_match_span(const qs_match *, qs_span *);
size_t qs_match_count(const qs_match *);
const qs_match *qs_match_child(const qs_match *, size_t index);
const qs_match *qs_match_find(const qs_match *, const char *name);  /* first direct child */
qs_status qs_match_walk(const qs_match *, qs_match_visitor, void *user);
/* S-expression dump, e.g. (TOP (expr (term "1") (op:+ "+") (term "2"))). */
qs_status qs_match_dump(qs_context *, const qs_match *, const char **text, size_t *length);

#ifdef __cplusplus
}

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// A capture in a match tree. Children are named sub-rule captures in order.
struct qs_match {
    std::string name, sym, text;
    std::size_t from = 0, to = 0, line = 1, column = 1;
    std::vector<qs_match> children;

    const qs_match* find(std::string_view n) const {
        for (const auto& c : children) if (c.name == n) return &c;
        return nullptr;
    }
    std::vector<const qs_match*> all(std::string_view n) const {
        std::vector<const qs_match*> r;
        for (const auto& c : children) if (c.name == n) r.push_back(&c);
        return r;
    }
    bool has(std::string_view n) const { return find(n) != nullptr; }
    std::string dump() const {
        std::ostringstream out; dump_to(out); return out.str();
    }
    // Generic AST view: inner nodes are tagged name (or name:sym), leaves
    // carry the matched text.
    dsl::ASTNode to_ast() const {
        const std::string tag = sym.empty() ? name : name + ":" + sym;
        if (children.empty()) return dsl::ASTNode(tag, text);
        std::vector<dsl::ASTNode> kids;
        for (const auto& c : children) kids.push_back(c.to_ast());
        return dsl::ASTNode(tag, std::move(kids));
    }
private:
    static void quote(std::ostringstream& out, std::string_view s) {
        out << '"';
        for (char c : s) {
            if (c == '"' || c == '\\') out << '\\' << c;
            else if (c == '\n') out << "\\n";
            else if (c == '\t') out << "\\t";
            else out << c;
        }
        out << '"';
    }
    void dump_to(std::ostringstream& out) const {
        out << '(' << name;
        if (!sym.empty()) out << ':' << sym;
        if (children.empty()) { out << ' '; quote(out, text); }
        for (const auto& c : children) { out << ' '; c.dump_to(out); }
        out << ')';
    }
};

namespace qscript::grammar {
using Match = ::qs_match;
enum class Kind { Token, Rule, Regex };
struct Declaration {
    std::string name;          // display name, e.g. "infix" or "kw-if"
    Kind kind = Kind::Token;
    bool proto = false;        // `proto token name {*}`
    bool has_sym = false;      // `name:sym<...>` variant
    std::string sym;
    std::string body;          // regex body between the braces
    std::size_t line = 0;
};
struct ParseOptions {
    std::size_t max_depth = 4096;         // ParseTk evaluation depth
    std::size_t max_operations = 0;       // 0: 2'000'000 + 512 per input byte
    std::size_t max_input_bytes = 64u * 1024u * 1024u;
};
struct ParseOutcome {
    std::optional<Match> match;
    std::string error;
    std::vector<std::string> expected;   // what would have been accepted at the error
    std::string found;                   // the input text at the error (one line, short)
    std::size_t offset = 0, line = 0, column = 0;
    explicit operator bool() const { return match.has_value(); }
};

namespace detail {
struct Node {
    enum T { Empty, Lit, Ref, Class, Seq, Alt, Star, Plus, Opt, And, Not } t = Empty;
    std::string text;
    std::vector<Node> kids;
    std::string desc;  // source spelling of a character-class atom, for errors
    static Node make(T t, std::string text = {}, std::vector<Node> kids = {}) {
        Node n; n.t = t; n.text = std::move(text); n.kids = std::move(kids); return n;
    }
};
inline bool word(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}
inline bool name_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
inline bool name_char(char c) { return word(c) || c == '-'; }
inline std::string internal(std::string_view display) {
    std::string r(display);
    for (char& c : r) if (c == '-') c = '@';
    return r;
}
inline std::string display(std::string_view internal_name) {
    std::string r(internal_name);
    for (char& c : r) if (c == '@') c = '-';
    return r;
}
// Regex bracket contents for builtin named character classes.
inline std::optional<std::string> named_class(std::string_view n) {
    static const std::map<std::string, std::string, std::less<>> classes{
        {"alpha", "A-Za-z_"}, {"digit", "0-9"}, {"alnum", "A-Za-z0-9_"},
        {"upper", "A-Z"}, {"lower", "a-z"}, {"xdigit", "0-9A-Fa-f"},
        {"space", " \\t\\r\\n\\f\\v"}, {"blank", " \\t"}, {"cntrl", "\\x00-\\x1f\\x7f"},
        {"punct", "!-/:-@\\[-`{-~"}};
    auto it = classes.find(n);
    if (it == classes.end()) return std::nullopt;
    return it->second;
}
inline std::string escape_class_char(unsigned char c) {
    static const char* hex = "0123456789abcdef";
    if (c < 0x20 || c >= 0x7f) return std::string("\\x") + hex[c >> 4] + hex[c & 15];
    if (c == '\\' || c == ']' || c == '[' || c == '^' || c == '-') return std::string("\\") + char(c);
    return std::string(1, char(c));
}
inline std::string quote_literal(std::string_view s) {
    std::string r = "'";
    for (char c : s) {
        if (c == '\\' || c == '\'') { r += '\\'; r += c; }
        else if (c == '\n') r += "\\n";
        else if (c == '\t') r += "\\t";
        else if (c == '\r') r += "\\r";
        else r += c;
    }
    return r + "'";
}

class Builder;
class BodyParser {
    std::string_view s_;
    std::size_t p_ = 0;
    bool sigspace_;
    const std::string* sym_;
    Builder& b_;
    std::string rule_;
    int angle_ = 0;
    char frugal_op_ = 0;
    [[noreturn]] void fail(const std::string& m) const {
        throw std::invalid_argument("in " + rule_ + ": " + m);
    }
    bool skip() {
        bool any = false;
        while (p_ < s_.size()) {
            if (s_[p_] == ' ' || s_[p_] == '\t' || s_[p_] == '\r' || s_[p_] == '\n') { ++p_; any = true; }
            else if (s_[p_] == '#') { while (p_ < s_.size() && s_[p_] != '\n') ++p_; any = true; }
            else break;
        }
        return any;
    }
    bool at(std::string_view t) const { return s_.substr(p_, t.size()) == t; }
    bool eat(std::string_view t) { if (!at(t)) return false; p_ += t.size(); return true; }
    bool stop() const {
        if (p_ >= s_.size()) return true;
        const char c = s_[p_];
        return c == '|' || c == ']' || c == ')' || (c == '>' && angle_ > 0);
    }
    Node ws_ref();
    Node subrule(std::string name, bool silent, std::string alias);
    Node char_class();
    std::string bracket(bool& negated_out);
    Node escape();
    Node angle();
    Node atom(std::size_t depth);
    Node atom_inner(std::size_t depth);
    Node quantify(Node a, bool& frugal, std::size_t depth);
    Node sequence(std::size_t depth);
    Node alternation(std::size_t depth);
    unsigned number() {
        const auto start = p_; unsigned long v = 0;
        while (p_ < s_.size() && s_[p_] >= '0' && s_[p_] <= '9') {
            v = v * 10 + unsigned(s_[p_++] - '0');
            if (v > 4096) fail("repetition count too large (limit 4096)");
        }
        if (p_ == start) fail("expected repetition count after **");
        return unsigned(v);
    }
public:
    BodyParser(std::string_view s, bool sigspace, const std::string* sym, Builder& b, std::string rule)
        : s_(s), sigspace_(sigspace), sym_(sym), b_(b), rule_(std::move(rule)) {}
    Node run() {
        skip();
        auto n = alternation(0);
        if (p_ != s_.size()) fail(std::string("unexpected '") + s_[p_] + "'");
        return n;
    }
};

class Builder {
public:
    parsetk::Registry registry;
    std::map<std::string, std::string> lexemes;       // regex -> internal name
    std::map<std::string, std::string> descriptions;  // internal name -> readable
    std::map<std::string, std::string> variant_syms;  // "infix#3" -> "+"
    std::set<std::string> defined;                    // internal production names
    std::map<std::string, std::string> wanted;        // referenced name -> first user rule
    std::set<std::string> wrappers;
    std::ostringstream peg;
    std::size_t helper_count = 0;

    std::string lexeme(const std::string& regex, std::string description) {
        auto it = lexemes.find(regex);
        if (it != lexemes.end()) return it->second;
        const std::string n = "#L" + std::to_string(lexemes.size());
        lexemes.emplace(regex, n);
        descriptions[n] = std::move(description);
        registry.add_symbol({parsetk::Symbol::Kind::Lexeme, n, parsetk::PatternSpec::regex(regex), {}});
        peg << "lexeme " << n << " = /" << regex << "/\n";
        return n;
    }
    void production(const std::string& name, const std::string& body) {
        registry.add_symbol({parsetk::Symbol::Kind::Production, name, {}, body});
        peg << "rule " << name << " = " << body << "\n";
    }
    std::string reference(const std::string& name, const std::string& from) {
        wanted.emplace(name, from);
        return name;
    }
    std::string wrapper(const std::string& wrapper_name, const std::string& target, const std::string& from) {
        reference(target, from);
        if (wrappers.insert(wrapper_name).second) production(wrapper_name, target);
        return wrapper_name;
    }
    std::string serialize(const Node& n) {
        switch (n.t) {
        case Node::Empty: return "()";
        case Node::Lit: return quote_literal(n.text);
        case Node::Ref: return n.text;
        case Node::Class: return lexeme(n.text, n.desc.empty() ? "character class /" + n.text + "/" : n.desc);
        case Node::Seq: case Node::Alt: {
            if (n.kids.empty()) return "()";
            if (n.kids.size() == 1) return serialize(n.kids[0]);
            std::string r = "(";
            for (std::size_t i = 0; i < n.kids.size(); ++i) {
                if (i) r += n.t == Node::Seq ? " " : " | ";
                r += serialize(n.kids[i]);
            }
            return r + ")";
        }
        case Node::Star: return "(" + serialize(n.kids[0]) + ")*";
        case Node::Plus: return "(" + serialize(n.kids[0]) + ")+";
        case Node::Opt: return "(" + serialize(n.kids[0]) + ")?";
        case Node::And: return "&(" + serialize(n.kids[0]) + ")";
        case Node::Not: return "!(" + serialize(n.kids[0]) + ")";
        }
        return "()";
    }
};

inline Node BodyParser::ws_ref() {
    return Node::make(Node::Ref, b_.wrapper(".ws", "ws", rule_));
}
inline Node BodyParser::subrule(std::string name, bool silent, std::string alias) {
    if (name.empty() || !name_start(name[0])) fail("invalid sub-rule name <" + name + ">");
    for (char c : name) if (!name_char(c)) fail("invalid sub-rule name <" + name + ">");
    const auto target = internal(name);
    std::string ref = silent ? b_.wrapper("." + target, target, rule_) : b_.reference(target, rule_);
    if (!alias.empty()) {
        for (char c : alias) if (!name_char(c)) fail("invalid alias name " + alias);
        ref = b_.wrapper(internal(alias) + "$" + ref, ref, rule_);
    }
    return Node::make(Node::Ref, ref);
}
inline std::string BodyParser::bracket(bool& negated) {
    // At '[': read contents until ']' into regex bracket contents.
    ++p_;
    std::string out;
    negated = false;
    while (true) {
        if (p_ >= s_.size()) fail("unterminated character class");
        char c = s_[p_];
        if (c == ']') { ++p_; break; }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++p_; continue; }
        auto single = [&]() -> std::optional<unsigned char> {
            char d = s_[p_];
            if (d == '\\') {
                if (++p_ >= s_.size()) fail("dangling escape in character class");
                char e = s_[p_++];
                switch (e) {
                case 'n': return '\n'; case 't': return '\t'; case 'r': return '\r';
                case 'f': return '\f'; case '0': return '\0';
                case 'e': return 27;
                case 'x': {
                    bool braced = p_ < s_.size() && s_[p_] == '[';
                    if (braced) ++p_;
                    unsigned v = 0; int digits = 0;
                    while (p_ < s_.size() && std::isxdigit(static_cast<unsigned char>(s_[p_])) && digits < 2) {
                        const char h = s_[p_++]; ++digits;
                        v = v * 16 + unsigned(std::isdigit(static_cast<unsigned char>(h)) ? h - '0' : (std::tolower(h) - 'a' + 10));
                    }
                    if (!digits) fail("expected hex digits after \\x");
                    if (braced && (p_ >= s_.size() || s_[p_++] != ']')) fail("expected ] after \\x[..");
                    return static_cast<unsigned char>(v);
                }
                case 'd': case 'w': case 's': case 'h': case 'v': case 'D': case 'W': case 'S': case 'N':
                    --p_; return std::nullopt;
                default: return static_cast<unsigned char>(e);
                }
            }
            ++p_; return static_cast<unsigned char>(d);
        };
        if (c == '\\' && p_ + 1 < s_.size() && std::string_view("dwshvDWSN").find(s_[p_ + 1]) != std::string_view::npos) {
            const char e = s_[p_ + 1]; p_ += 2;
            switch (e) {
            case 'd': out += "0-9"; break;
            case 'w': out += "A-Za-z0-9_"; break;
            case 's': out += " \\t\\r\\n\\f\\v"; break;
            case 'h': out += " \\t"; break;
            case 'v': out += "\\n\\r\\f\\v"; break;
            case 'D': out += "\\D"; break;
            case 'W': out += "\\W"; break;
            case 'S': out += "\\S"; break;
            case 'N': fail("\\N is not allowed inside a character class; use <-[\\n]>");
            }
            continue;
        }
        auto lo = single();
        if (!lo) fail("invalid character class escape");
        if (s_.substr(p_, 2) == "..") {
            p_ += 2;
            while (p_ < s_.size() && (s_[p_] == ' ' || s_[p_] == '\t')) ++p_;
            if (p_ >= s_.size()) fail("unterminated character range");
            auto hi = single();
            if (!hi || *hi < *lo) fail("invalid character range");
            out += escape_class_char(*lo) + "-" + escape_class_char(*hi);
        } else out += escape_class_char(*lo);
    }
    if (out.empty()) fail("empty character class");
    return out;
}
inline Node BodyParser::char_class() {
    // After '<': sequence of [+|-] ( [ ... ] | name ), terminated by '>'.
    std::vector<std::string> pos, neg;
    bool first = true;
    while (true) {
        while (p_ < s_.size() && (s_[p_] == ' ' || s_[p_] == '\t' || s_[p_] == '\n')) ++p_;
        if (p_ >= s_.size()) fail("unterminated character class assertion");
        if (s_[p_] == '>') { ++p_; break; }
        bool negative = false;
        if (s_[p_] == '+' || s_[p_] == '-') { negative = s_[p_] == '-'; ++p_; }
        else if (!first) fail("expected + or - between character class terms");
        first = false;
        while (p_ < s_.size() && s_[p_] == ' ') ++p_;
        std::string contents;
        if (p_ < s_.size() && s_[p_] == '[') { bool n = false; contents = bracket(n); }
        else {
            const auto start = p_;
            while (p_ < s_.size() && word(s_[p_])) ++p_;
            auto named = named_class(s_.substr(start, p_ - start));
            if (!named) fail("unknown character class <" + std::string(s_.substr(start, p_ - start)) + ">");
            contents = *named;
        }
        (negative ? neg : pos).push_back(contents);
    }
    std::string positive, negative;
    for (const auto& c : pos) positive += c;
    for (const auto& c : neg) negative += c;
    std::string regex = positive.empty() ? "[\\s\\S]" : "[" + positive + "]";
    if (!negative.empty()) regex = "(?![" + negative + "])" + regex;
    return Node::make(Node::Class, regex);
}
inline Node BodyParser::escape() {
    ++p_;
    if (p_ >= s_.size()) fail("dangling backslash");
    const char e = s_[p_++];
    switch (e) {
    case 'd': return Node::make(Node::Class, "[0-9]");
    case 'D': return Node::make(Node::Class, "[^0-9]");
    case 'w': return Node::make(Node::Class, "[A-Za-z0-9_]");
    case 'W': return Node::make(Node::Class, "[^A-Za-z0-9_]");
    case 's': return Node::make(Node::Class, "[ \\t\\r\\n\\f\\v]");
    case 'S': return Node::make(Node::Class, "[^ \\t\\r\\n\\f\\v]");
    case 'h': return Node::make(Node::Class, "[ \\t]");
    case 'H': return Node::make(Node::Class, "[^ \\t]");
    case 'v': return Node::make(Node::Class, "[\\n\\r\\f\\v]");
    case 'V': return Node::make(Node::Class, "[^\\n\\r\\f\\v]");
    case 'N': return Node::make(Node::Class, "[^\\n]");
    case 'n': return Node::make(Node::Lit, "\n");
    case 't': return Node::make(Node::Lit, "\t");
    case 'r': return Node::make(Node::Lit, "\r");
    case 'e': return Node::make(Node::Lit, "\x1b");
    case '0': return Node::make(Node::Lit, std::string(1, '\0'));
    case 'x': {
        bool braced = p_ < s_.size() && s_[p_] == '[';
        if (braced) ++p_;
        unsigned v = 0; int digits = 0;
        while (p_ < s_.size() && std::isxdigit(static_cast<unsigned char>(s_[p_])) && digits < 2) {
            const char h = s_[p_++]; ++digits;
            v = v * 16 + unsigned(std::isdigit(static_cast<unsigned char>(h)) ? h - '0' : (std::tolower(h) - 'a' + 10));
        }
        if (!digits) fail("expected hex digits after \\x");
        if (braced && (p_ >= s_.size() || s_[p_++] != ']')) fail("expected ] after \\x[..");
        return Node::make(Node::Lit, std::string(1, static_cast<char>(v)));
    }
    default:
        if (word(e)) fail(std::string("unknown escape \\") + e);
        return Node::make(Node::Lit, std::string(1, e));
    }
}
inline Node BodyParser::angle() {
    // At '<'.
    ++p_;
    if (p_ < s_.size() && (s_[p_] == '[' || ((s_[p_] == '-' || s_[p_] == '+') && p_ + 1 < s_.size() && (s_[p_ + 1] == '[' || name_start(s_[p_ + 1])))))
        return char_class();
    auto read_name = [&] {
        const auto start = p_;
        while (p_ < s_.size() && name_char(s_[p_])) ++p_;
        return std::string(s_.substr(start, p_ - start));
    };
    auto close = [&] {
        while (p_ < s_.size() && s_[p_] == ' ') ++p_;
        if (p_ >= s_.size() || s_[p_] != '>') fail("expected '>' to close assertion");
        ++p_;
    };
    if (p_ < s_.size() && (s_[p_] == '?' || s_[p_] == '!')) {
        const bool positive = s_[p_++] == '?';
        if (p_ < s_.size() && s_[p_] == '>') { ++p_; return positive ? Node::make(Node::Empty) : Node::make(Node::Not, {}, {Node::make(Node::Empty)}); }
        auto n = read_name();
        Node inner;
        if (n == "before") {
            ++angle_; skip();
            inner = alternation(1);
            --angle_;
            close();
        } else if (n == "after") {
            fail("<?after ...> needs lookbehind, which a PEG cannot provide");
        } else {
            if (n.empty()) fail("expected a rule name or 'before' in lookahead");
            if (auto named = named_class(n); named) inner = Node::make(Node::Class, "[" + *named + "]");
            else inner = subrule(n, true, {});
            close();
        }
        return Node::make(positive ? Node::And : Node::Not, {}, {std::move(inner)});
    }
    bool silent = false;
    if (p_ < s_.size() && s_[p_] == '.') { silent = true; ++p_; }
    auto n = read_name();
    if (n.empty()) fail("expected a sub-rule name after '<'");
    if (!silent && p_ < s_.size() && s_[p_] == '=') {
        ++p_;
        bool inner_silent = false;
        if (p_ < s_.size() && s_[p_] == '.') { inner_silent = true; ++p_; }
        auto target = read_name();
        close();
        return subrule(target, inner_silent, n);
    }
    if (p_ < s_.size() && (s_[p_] == '(' || s_[p_] == ':')) fail("sub-rule arguments are not supported: <" + n + ">");
    close();
    if (n == "sym" && !silent) {
        if (!sym_) fail("<sym> used outside a proto variant");
        return Node::make(Node::Lit, *sym_);
    }
    return subrule(n, silent, {});
}
inline Node BodyParser::atom(std::size_t depth) {
    const auto start = p_;
    Node n = atom_inner(depth);
    if (n.t == Node::Class && n.desc.empty()) n.desc = std::string(s_.substr(start, p_ - start));
    return n;
}
inline Node BodyParser::atom_inner(std::size_t depth) {
    if (depth > 200) fail("regex nesting too deep");
    const char c = s_[p_];
    if (c == '\'' || c == '"') {
        const char quote = s_[p_++]; std::string text;
        while (true) {
            if (p_ >= s_.size()) fail("unterminated quoted literal");
            char d = s_[p_++];
            if (d == quote) break;
            if (d == '\\') {
                if (p_ >= s_.size()) fail("unterminated quoted literal");
                char e = s_[p_++];
                switch (e) {
                case 'n': d = '\n'; break; case 't': d = '\t'; break; case 'r': d = '\r'; break;
                case '0': d = '\0'; break; case '\\': case '\'': case '"': d = e; break;
                default: fail(std::string("unknown escape in literal: \\") + e);
                }
            }
            text += d;
        }
        if (text.empty()) return Node::make(Node::Empty);
        return Node::make(Node::Lit, text);
    }
    if (c == '[' || c == '(') {
        const char closing = c == '[' ? ']' : ')';
        ++p_; skip();
        const int saved = angle_; angle_ = 0;
        auto n = alternation(depth + 1);
        angle_ = saved;
        if (p_ >= s_.size() || s_[p_] != closing) fail(std::string("expected '") + closing + "'");
        ++p_;
        return n;
    }
    if (c == '\\') return escape();
    if (c == '.') { ++p_; return Node::make(Node::Class, "[\\s\\S]"); }
    if (at("^^") || at("^")) { p_ += at("^^") ? 2 : 1; return Node::make(Node::Empty); }
    if (at("$$")) {
        p_ += 2;
        return Node::make(Node::Alt, {}, {Node::make(Node::And, {}, {Node::make(Node::Lit, "\n")}),
                                          Node::make(Node::Not, {}, {Node::make(Node::Class, "[\\s\\S]")})});
    }
    if (c == '$') { ++p_; return Node::make(Node::Not, {}, {Node::make(Node::Class, "[\\s\\S]")}); }
    if (at(">>") || at("\xc2\xbb")) {
        p_ += at(">>") ? 2 : 2;
        return Node::make(Node::Not, {}, {Node::make(Node::Class, "[A-Za-z0-9_]")});
    }
    if (at("<<") || at("\xc2\xab")) { p_ += 2; return Node::make(Node::Empty); }
    if (c == '<') return angle();
    if (c == '{') fail("embedded code blocks are not supported; build values from the match tree");
    if (c == ':') fail("inline adverbs (:i, :s, ...) are not supported");
    if (word(c)) {
        const auto start = p_;
        while (p_ < s_.size() && word(s_[p_])) ++p_;
        return Node::make(Node::Lit, std::string(s_.substr(start, p_ - start)));
    }
    fail(std::string("unrecognized metacharacter '") + c + "' (quote literal punctuation)");
}
inline Node BodyParser::quantify(Node a, bool& frugal, std::size_t depth) {
    // Returns the quantified node. For a frugal quantifier (*? +? ??) the
    // unquantified atom is returned with frugal=true and frugal_op_ set; the
    // caller rewrites `X*? Y` as `(!Y X)* Y`.
    frugal = false;
    const auto before = p_;
    skip();
    if (p_ >= s_.size()) { p_ = before; return a; }
    auto fuse = [&](Node::T kind, const Node& x) -> Node {
        if (x.t == Node::Class) {
            Node many = Node::make(Node::Class, "(?:" + x.text + ")+");
            many.desc = x.desc.empty() ? std::string() : x.desc + "+";
            if (kind == Node::Plus) return many;
            if (kind == Node::Star) return Node::make(Node::Opt, {}, {many});
        }
        return Node::make(kind, {}, {x});
    };
    Node q;
    char op = 0;
    if (at("**")) {
        p_ += 2; skip();
        unsigned lo = number(), hi = 0; bool unbounded = false;
        if (eat("..")) {
            if (eat("*")) unbounded = true;
            else hi = number();
        } else hi = lo;
        if (!unbounded && hi < lo) fail("invalid repetition range");
        if (a.t == Node::Class && !unbounded && hi > 0) {
            q = Node::make(Node::Class, "(?:" + a.text + "){" + std::to_string(lo ? lo : 1) + "," + std::to_string(hi) + "}");
            if (!lo) q = Node::make(Node::Opt, {}, {q});
        } else {
            Node seq = Node::make(Node::Seq);
            for (unsigned i = 0; i < lo; ++i) seq.kids.push_back(a);
            if (unbounded) seq.kids.push_back(fuse(Node::Star, a));
            else {
                std::optional<Node> tail;
                for (unsigned i = lo; i < hi; ++i) {
                    Node step = Node::make(Node::Seq, {}, {a});
                    if (tail) step.kids.push_back(*tail);
                    tail = Node::make(Node::Opt, {}, {step});
                }
                if (tail) seq.kids.push_back(*tail);
            }
            q = seq.kids.size() == 1 ? seq.kids[0] : (seq.kids.empty() ? Node::make(Node::Empty) : std::move(seq));
        }
    } else if (s_[p_] == '*' || s_[p_] == '+' || s_[p_] == '?') {
        op = s_[p_++];
        if (p_ < s_.size() && s_[p_] == '?') {
            ++p_; frugal = true; frugal_op_ = op;
            return a;
        }
        if (p_ < s_.size() && (s_[p_] == ':' || s_[p_] == '!')) ++p_;
        q = fuse(op == '*' ? Node::Star : op == '+' ? Node::Plus : Node::Opt, a);
    } else { p_ = before; return a; }
    // Separator modifiers: X+ % SEP, X* % SEP, X+ %% SEP (trailing allowed).
    const auto after = p_;
    skip();
    if ((op == '*' || op == '+') && p_ < s_.size() && s_[p_] == '%') {
        bool trailing = false;
        if (eat("%%")) trailing = true; else ++p_;
        skip();
        if (stop()) fail("expected separator after %");
        Node sep = atom(depth + 1);
        Node step = Node::make(Node::Seq);
        if (sigspace_) step.kids.push_back(ws_ref());
        step.kids.push_back(sep);
        if (sigspace_) step.kids.push_back(ws_ref());
        step.kids.push_back(a);
        Node list = Node::make(Node::Seq, {}, {a, Node::make(Node::Star, {}, {step})});
        if (trailing) {
            Node tail = Node::make(Node::Seq);
            if (sigspace_) tail.kids.push_back(ws_ref());
            tail.kids.push_back(sep);
            list.kids.push_back(Node::make(Node::Opt, {}, {tail}));
        }
        return op == '*' ? Node::make(Node::Opt, {}, {list}) : list;
    }
    p_ = after;
    return q;
}
inline Node BodyParser::sequence(std::size_t depth) {
    Node seq = Node::make(Node::Seq);
    std::optional<Node> pending; char pending_op = 0;
    auto push = [&](Node n) {
        if (pending) {
            // X*? Y  ==  (!Y X)* Y ;  X+? Y == X (!Y X)* Y ;  X?? Y == (!Y X)? Y
            Node body = Node::make(Node::Seq, {}, {Node::make(Node::Not, {}, {n}), *pending});
            if (pending_op == '+') seq.kids.push_back(*pending);
            seq.kids.push_back(Node::make(pending_op == '?' ? Node::Opt : Node::Star, {}, {body}));
            pending.reset();
        }
        seq.kids.push_back(std::move(n));
    };
    while (true) {
        skip();
        if (stop()) break;
        Node a = atom(depth + 1);
        bool frugal = false;
        Node q = quantify(std::move(a), frugal, depth);
        if (frugal) {
            if (pending) fail("two adjacent frugal quantifiers");
            pending = std::move(q); pending_op = frugal_op_;
        } else {
            // Goal matching: OPEN ~ CLOSE INNER  ==  OPEN INNER CLOSE
            const auto save = p_;
            skip();
            if (p_ < s_.size() && s_[p_] == '~') {
                ++p_; skip();
                bool f = false;
                Node close = quantify(atom(depth + 1), f, depth);
                skip();
                if (f || stop()) fail("expected an atom after the ~ closing atom");
                Node inner = quantify(atom(depth + 1), f, depth);
                if (f) fail("frugal quantifier not allowed in ~ goal");
                push(std::move(q));
                if (sigspace_) push(ws_ref());
                push(std::move(inner));
                if (sigspace_) push(ws_ref());
                push(std::move(close));
            } else {
                p_ = save;
                push(std::move(q));
            }
        }
        if (skip() && sigspace_ && !pending) push(ws_ref());
    }
    if (pending) {
        if (pending_op == '+') seq.kids.push_back(*pending);
        pending.reset();
    }
    if (seq.kids.size() == 1) return std::move(seq.kids[0]);
    return seq;
}
inline Node BodyParser::alternation(std::size_t depth) {
    if (depth > 200) fail("regex nesting too deep");
    skip();
    if (eat("||") || eat("|")) skip();  // leading separator, as in Raku
    Node alt = Node::make(Node::Alt);
    alt.kids.push_back(sequence(depth + 1));
    while (true) {
        skip();
        if (eat("||") || eat("|")) { skip(); alt.kids.push_back(sequence(depth + 1)); }
        else break;
    }
    if (alt.kids.size() == 1) return std::move(alt.kids[0]);
    return alt;
}
// Length of the literal text a node must start with (LTM ordering for protos).
inline std::size_t literal_prefix(const Node& n) {
    switch (n.t) {
    case Node::Lit: return n.text.size();
    case Node::Seq: {
        std::size_t total = 0;
        for (const auto& k : n.kids) {
            if (k.t == Node::Lit) total += k.text.size();
            else { if (k.t != Node::Not && k.t != Node::And) total += literal_prefix(k); break; }
        }
        return total;
    }
    case Node::Alt: {
        std::size_t best = SIZE_MAX;
        for (const auto& k : n.kids) best = std::min(best, literal_prefix(k));
        return best == SIZE_MAX ? 0 : best;
    }
    case Node::Plus: return literal_prefix(n.kids[0]);
    default: return 0;
    }
}
inline const char* kind_name(Kind k) { return k == Kind::Rule ? "rule" : k == Kind::Regex ? "regex" : "token"; }
} // namespace detail

// A compiled grammar. Use Grammar::compile for source text, or add
// declarations programmatically and call build().
} // namespace qscript::grammar

struct qs_grammar {
    std::string name;
    std::vector<qscript::grammar::Declaration> declarations;
    std::string error;

    static qs_grammar compile(std::string_view source);
    qs_grammar& token(std::string n, std::string body) { return add(std::move(n), qscript::grammar::Kind::Token, std::move(body)); }
    qs_grammar& rule(std::string n, std::string body) { return add(std::move(n), qscript::grammar::Kind::Rule, std::move(body)); }
    qs_grammar& regex(std::string n, std::string body) { return add(std::move(n), qscript::grammar::Kind::Regex, std::move(body)); }
    qs_grammar& proto(std::string n, qscript::grammar::Kind k = qscript::grammar::Kind::Token) {
        qscript::grammar::Declaration d; d.name = std::move(n); d.kind = k; d.proto = true; d.body = "*";
        return add_declaration(std::move(d));
    }
    qs_grammar& variant(std::string n, std::string sym, std::string body, qscript::grammar::Kind k = qscript::grammar::Kind::Token) {
        qscript::grammar::Declaration d; d.name = std::move(n); d.kind = k; d.has_sym = true; d.sym = std::move(sym); d.body = std::move(body);
        return add_declaration(std::move(d));
    }
    qs_grammar& add_declaration(qscript::grammar::Declaration d) {
        for (auto& existing : declarations)
            if (existing.name == d.name && existing.has_sym == d.has_sym && existing.sym == d.sym && existing.proto == d.proto) {
                existing = std::move(d); built_ = false; return *this;
            }
        declarations.push_back(std::move(d)); built_ = false; return *this;
    }
    // Translate declarations to ParseTk. Returns false and sets error on failure.
    bool build();
    bool ok() const { return built_ && error.empty(); }
    const parsetk::Registry& registry() const { return registry_; }
    const std::string& peg() const { return peg_; }
    std::vector<std::string> rule_names() const {
        std::vector<std::string> r; std::set<std::string> seen;
        for (const auto& d : declarations) if (seen.insert(d.name).second) r.push_back(d.name);
        return r;
    }
    qscript::grammar::ParseOutcome parse(std::string_view input, std::string_view start = "TOP",
                                         qscript::grammar::ParseOptions options = {}) const;
private:
    qs_grammar& add(std::string n, qscript::grammar::Kind k, std::string body) {
        qscript::grammar::Declaration d; d.name = std::move(n); d.kind = k; d.body = std::move(body);
        return add_declaration(std::move(d));
    }
    parsetk::Registry registry_;
    std::string peg_;
    std::map<std::string, std::string> descriptions_, variant_syms_;
    bool built_ = false;
};

namespace qscript::grammar {
using Grammar = ::qs_grammar;
}
namespace qscript {
// The compiled grammar of the QScript language itself.
const grammar::Grammar& language_grammar();
}

inline bool qs_grammar::build() {
    using namespace qscript::grammar;
    using detail::Node;
    error.clear(); built_ = false;
    detail::Builder b;
    try {
        if (declarations.empty()) throw std::invalid_argument("grammar has no declarations");
        std::map<std::string, std::vector<const Declaration*>> variants;
        std::map<std::string, const Declaration*> protos;
        std::vector<std::string> order;
        for (const auto& d : declarations) {
            if (d.name.empty() || !detail::name_start(d.name[0])) throw std::invalid_argument("invalid rule name: " + d.name);
            for (char c : d.name) if (!detail::name_char(c)) throw std::invalid_argument("invalid rule name: " + d.name);
            if (d.has_sym) {
                if (d.sym.empty()) throw std::invalid_argument("empty :sym<> on " + d.name);
                variants[d.name].push_back(&d);
            } else if (d.proto) protos[d.name] = &d;
        }
        for (const auto& d : declarations) {
            const auto in = detail::internal(d.name);
            if (d.has_sym || d.proto) continue;
            if (variants.count(d.name)) throw std::invalid_argument("rule " + d.name + " is both a plain rule and a proto");
            if (!b.defined.insert(in).second) throw std::invalid_argument("duplicate rule: " + d.name);
            detail::BodyParser parser(d.body, d.kind == Kind::Rule, nullptr, b, std::string(detail::kind_name(d.kind)) + " " + d.name);
            b.production(in, b.serialize(parser.run()));
        }
        for (const auto& [base, list] : variants) {
            const auto in = detail::internal(base);
            if (!b.defined.insert(in).second) throw std::invalid_argument("duplicate rule: " + base);
            std::vector<std::pair<std::size_t, std::string>> alternatives;
            std::size_t index = 0;
            std::set<std::string> syms;
            for (const auto* d : list) {
                if (!syms.insert(d->sym).second) throw std::invalid_argument("duplicate variant " + base + ":sym<" + d->sym + ">");
                const auto name = in + "#" + std::to_string(index++);
                detail::BodyParser parser(d->body, d->kind == Kind::Rule, &d->sym, b,
                    std::string(detail::kind_name(d->kind)) + " " + base + ":sym<" + d->sym + ">");
                Node body = parser.run();
                alternatives.emplace_back(detail::literal_prefix(body), name);
                b.production(name, b.serialize(body));
                b.variant_syms[name] = d->sym;
            }
            std::stable_sort(alternatives.begin(), alternatives.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
            std::string body = alternatives.size() == 1 ? alternatives[0].second : "(";
            if (alternatives.size() > 1) {
                for (std::size_t k = 0; k < alternatives.size(); ++k) body += (k ? " | " : "") + alternatives[k].second;
                body += ")";
            }
            b.production(in, body);
        }
        for (const auto& [name, d] : protos)
            if (!variants.count(name)) throw std::invalid_argument("proto " + name + " has no :sym<> variants");
        // Builtins: ws, ident, and the named character classes, unless user-defined.
        for (const auto& [name, from] : std::map<std::string, std::string>(b.wanted)) {
            if (b.defined.count(name) || b.wrappers.count(name)) continue;
            if (name == "ws") {
                b.production("ws", "(" + b.lexeme("[ \\t\\r\\n\\f\\v]+", "whitespace") + ")?");
            } else if (name == "ident") {
                b.production("ident", b.lexeme("[A-Za-z_][A-Za-z0-9_]*", "identifier"));
            } else if (auto cls = detail::named_class(name)) {
                b.production(name, b.lexeme("[" + *cls + "]", name));
            } else {
                throw std::invalid_argument("unknown rule <" + detail::display(name) + "> referenced in " + from);
            }
            b.defined.insert(name);
        }
        parsetk::Parser check(b.registry, parsetk::Options{false});
        if (!check.grammar_error().empty()) throw std::invalid_argument("ParseTk rejected the translated grammar: " + check.grammar_error());
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    registry_ = std::move(b.registry);
    peg_ = b.peg.str();
    descriptions_ = std::move(b.descriptions);
    variant_syms_ = std::move(b.variant_syms);
    built_ = true;
    return true;
}

inline qs_grammar qs_grammar::compile(std::string_view s) {
    using namespace qscript::grammar;
    qs_grammar g;
    std::size_t p = 0, line = 1;
    auto fail = [&](const std::string& m) { throw std::invalid_argument("line " + std::to_string(line) + ": " + m); };
    auto skip = [&] {
        while (p < s.size()) {
            if (s[p] == '\n') { ++line; ++p; }
            else if (s[p] == ' ' || s[p] == '\t' || s[p] == '\r') ++p;
            else if (s[p] == '#') { while (p < s.size() && s[p] != '\n') ++p; }
            else break;
        }
    };
    auto word = [&] {
        const auto start = p;
        while (p < s.size() && detail::name_char(s[p])) ++p;
        return std::string(s.substr(start, p - start));
    };
    auto expect = [&](char c) { skip(); if (p >= s.size() || s[p] != c) fail(std::string("expected '") + c + "'"); ++p; };
    // Brace-balanced body: skips quoted literals, escapes and <[ ... ]> classes.
    auto body = [&]() -> std::string {
        expect('{');
        const auto start = p; int depth = 1;
        while (p < s.size()) {
            const char c = s[p];
            if (c == '\n') ++line;
            if (c == '\\') { p += 2; continue; }
            if (c == '\'' || c == '"') {
                const char q = c; ++p;
                while (p < s.size() && s[p] != q) { if (s[p] == '\\') ++p; if (p < s.size() && s[p] == '\n') ++line; ++p; }
                ++p; continue;
            }
            if (c == '[' && p > 0 && (s[p - 1] == '<' || s[p - 1] == '-' || s[p - 1] == '+')) {
                ++p;
                while (p < s.size() && s[p] != ']') { if (s[p] == '\\') ++p; ++p; }
                ++p; continue;
            }
            if (c == '#') { while (p < s.size() && s[p] != '\n') ++p; continue; }
            if (c == '{') ++depth;
            if (c == '}' && --depth == 0) { ++p; return std::string(s.substr(start, p - 1 - start)); }
            ++p;
        }
        fail("unterminated rule body");
        return {};
    };
    std::map<std::string, std::vector<Declaration>> grammars;
    std::vector<std::string> grammar_order;
    try {
        skip();
        bool any = false;
        while (p < s.size()) {
            if (s.substr(p, 7) != "grammar") fail("expected 'grammar NAME { ... }'");
            p += 7; skip();
            auto gname = word();
            if (gname.empty()) fail("expected grammar name");
            if (grammars.count(gname)) fail("duplicate grammar " + gname);
            std::vector<Declaration> decls;
            skip();
            if (s.substr(p, 2) == "is" && p + 2 < s.size() && !detail::name_char(s[p + 2])) {
                p += 2; skip();
                auto parent = word();
                auto it = grammars.find(parent);
                if (it == grammars.end()) fail("unknown parent grammar " + parent + " (define it earlier in the same source)");
                decls = it->second;
                skip();
            }
            expect('{');
            while (true) {
                skip();
                if (p >= s.size()) fail("unterminated grammar " + gname);
                if (s[p] == '}') { ++p; break; }
                Declaration d; d.line = line;
                auto kw = word();
                if (kw == "proto") { d.proto = true; skip(); kw = word(); }
                if (kw == "token") d.kind = Kind::Token;
                else if (kw == "rule") d.kind = Kind::Rule;
                else if (kw == "regex") d.kind = Kind::Regex;
                else fail("expected token, rule, regex or proto, found '" + kw + "'");
                skip();
                d.name = word();
                if (d.name.empty()) fail("expected rule name");
                if (s.substr(p, 5) == ":sym<" || s.substr(p, 6) == ":sym\xc2\xab") {
                    const bool french = s[p + 4] != '<';
                    p += french ? 6 : 5;
                    const std::string_view close = french ? std::string_view("\xc2\xbb") : std::string_view(">");
                    const auto end = s.find(close, p);
                    if (end == std::string_view::npos) fail("unterminated :sym<...>");
                    d.has_sym = true; d.sym = std::string(s.substr(p, end - p));
                    p = end + close.size();
                    if (d.proto) fail("a proto cannot have a :sym<> adverb");
                }
                d.body = body();
                if (d.proto && metachain::text::trim(d.body) != "*") fail("proto body must be {*}");
                bool replaced = false;
                for (auto& old : decls)
                    if (old.name == d.name && old.has_sym == d.has_sym && old.sym == d.sym && old.proto == d.proto) { old = d; replaced = true; }
                if (!replaced) decls.push_back(std::move(d));
                skip();
                if (p < s.size() && s[p] == ';') ++p;
            }
            grammars[gname] = std::move(decls);
            grammar_order.push_back(gname);
            any = true;
            skip();
        }
        if (!any) fail("source contains no grammar");
        g.name = grammar_order.back();
        g.declarations = grammars[g.name];
        g.build();
    } catch (const std::exception& e) {
        g.error = e.what();
    }
    return g;
}

inline qscript::grammar::ParseOutcome qs_grammar::parse(std::string_view input, std::string_view start,
                                                        qscript::grammar::ParseOptions options) const {
    using namespace qscript::grammar;
    ParseOutcome out;
    if (!ok()) { out.error = error.empty() ? "grammar has not been built" : error; return out; }
    if (input.size() > options.max_input_bytes) { out.error = "input size limit exceeded"; return out; }
    parsetk::Options po;
    po.skip_whitespace = false;
    po.max_depth = options.max_depth;
    po.max_operations = options.max_operations ? options.max_operations : 2'000'000 + 512 * input.size();
    po.max_events = po.max_operations;
    po.max_input_bytes = options.max_input_bytes;
    parsetk::Parser parser(registry_, po);
    // Build the capture tree from the event stream.
    struct Frame { Match m; enum { Normal, Silent, Alias, Variant } role = Normal; bool filled = false; };
    std::vector<Frame> stack;
    std::optional<Match> root;
    int silent_depth = 0;
    parser.subscribe([&](const parsetk::Event& e) {
        if (e.kind == parsetk::EventKind::RuleBegin) {
            Frame f;
            const auto& n = e.name;
            if (silent_depth > 0 || (!n.empty() && n[0] == '.')) { f.role = Frame::Silent; ++silent_depth; }
            else if (auto dollar = n.find('$'); dollar != std::string::npos) { f.role = Frame::Alias; f.m.name = detail::display(n.substr(0, dollar)); }
            else if (auto hash = n.find('#'); hash != std::string::npos) {
                f.role = Frame::Variant;
                auto it = variant_syms_.find(n);
                f.m.sym = it == variant_syms_.end() ? std::string() : it->second;
                f.m.name = detail::display(n.substr(0, hash));
            } else f.m.name = detail::display(n);
            f.m.from = e.span.offset; f.m.line = e.span.line; f.m.column = e.span.column;
            stack.push_back(std::move(f));
        } else if (e.kind == parsetk::EventKind::RuleEnd) {
            if (stack.empty()) return;
            Frame f = std::move(stack.back()); stack.pop_back();
            if (f.role == Frame::Silent) { --silent_depth; return; }
            f.m.to = e.span.offset + e.span.length;
            f.m.text = std::string(input.substr(e.span.offset, e.span.length));
            if (stack.empty()) { root = std::move(f.m); return; }
            auto& parent = stack.back();
            // An alias takes over its single target capture; a proto takes
            // over the variant that matched.
            if (parent.role == Frame::Alias && !parent.filled) {
                parent.m.sym = std::move(f.m.sym);
                parent.m.children = std::move(f.m.children);
                parent.filled = true;
                return;
            }
            if (f.role == Frame::Variant && parent.role == Frame::Normal && !parent.filled && parent.m.name == f.m.name && parent.m.children.empty()) {
                parent.m.sym = std::move(f.m.sym);
                parent.m.children = std::move(f.m.children);
                parent.filled = true;
                return;
            }
            parent.m.children.push_back(std::move(f.m));
        }
    });
    auto r = parser.parse(input, detail::internal(start));
    if (!r) {
        auto readable = [&](std::string m) {
            for (const auto& [n, description] : descriptions_)
                if (m == "expected " + n) return "expected " + description;
            if (m.rfind("expected literal ", 0) == 0) m = "expected " + m.substr(17);
            for (auto pos = m.find('@'); pos != std::string::npos; pos = m.find('@', pos)) m[pos] = '-';
            return m;
        };
        out.error = readable(r.error);
        auto where = r.error_span;
        // Prefer the deepest terminal failures over a generic trailing-input
        // report: "expected ';'" at the real spot beats "unexpected trailing
        // input" at the start of the statement that failed. Optional
        // whitespace is never what the user is missing, so it is dropped.
        if ((r.error == "unexpected trailing input" || r.error == "lookahead failed") && !r.expected_all.empty() &&
                r.expected_span.offset >= r.error_span.offset) {
            std::vector<std::string> wanted;
            for (const auto& m : r.expected_all) {
                auto text = readable(m);
                if (text == "expected whitespace" || text == "lookahead failed" || text.rfind("expected ", 0) != 0) continue;
                text = text.substr(9);
                if (std::find(wanted.begin(), wanted.end(), text) == wanted.end()) wanted.push_back(text);
            }
            out.expected = wanted;
            if (!wanted.empty()) {
                std::string joined;
                for (std::size_t k = 0; k < wanted.size(); ++k)
                    joined += (k == 0 ? "" : k + 1 == wanted.size() ? " or " : ", ") + wanted[k];
                out.error = "expected " + joined; where = r.expected_span;
            }
        }
        out.offset = where.offset; out.line = where.line; out.column = where.column;
        if (where.offset < input.size()) {
            auto rest = input.substr(where.offset, 24);
            out.found = std::string(rest.substr(0, rest.find('\n')));
        }
        return out;
    }
    if (!root) { out.error = "parse produced no match"; return out; }
    out.match = std::move(root);
    return out;
}
#endif
#endif
