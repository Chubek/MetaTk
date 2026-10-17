#pragma once

// C++20 event-driven PEG parser. Choice is ordered, not longest-match.
#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace parsetk {
template<std::size_t N> struct FixedString {
    char value[N]{};
    constexpr FixedString(const char (&s)[N]) { std::copy_n(s, N, value); }
    constexpr std::string_view view() const { return {value, N - 1}; }
    constexpr operator std::string_view() const { return view(); }
};
template<std::size_t N> FixedString(const char (&)[N]) -> FixedString<N>;
struct SourceSpan { std::size_t offset = 0, length = 0, line = 1, column = 1; };
enum class EventKind { RuleBegin, Token, RuleEnd, ParseError };
struct Event {
    EventKind kind = EventKind::Token;
    std::string name, text;
    SourceSpan span{};
    std::size_t depth = 0;
    bool synthetic = false;
};
class Listener {
public:
    virtual ~Listener() = default;
    virtual void on_event(const Event&) = 0;
};
class EventStream {
    std::vector<std::function<void(const Event&)>> listeners_;
public:
    using Callback = std::function<void(const Event&)>;
    void subscribe(Listener& l) { subscribe([&l](const Event& e) { l.on_event(e); }); }
    void subscribe(Callback cb) { listeners_.push_back(std::move(cb)); }
    void emit(const Event& e) const {
        const auto snapshot = listeners_;
        for (const auto& cb : snapshot) cb(e);
    }
    // Deliver a batch with one listener snapshot.
    void emit_all(const std::vector<Event>& events) const {
        const auto snapshot = listeners_;
        for (const auto& e : events) for (const auto& cb : snapshot) cb(e);
    }
};
struct PatternSpec {
    enum class Kind { Regex, Exact };
    Kind kind = Kind::Regex;
    std::string source;
    static PatternSpec regex(std::string p) { return {Kind::Regex, std::move(p)}; }
    static PatternSpec exact(std::string p) { return {Kind::Exact, std::move(p)}; }
};
namespace Pattern {
inline PatternSpec Exact(std::string_view s) { return PatternSpec::exact(std::string(s)); }
namespace Repo {
inline const auto Integer = PatternSpec::regex("[0-9]+");
inline const auto CIdent = PatternSpec::regex("[A-Za-z_][A-Za-z0-9_]*");
inline const auto Identifier = CIdent;
inline const auto Whitespace = PatternSpec::regex("[ \\t\\r\\n]+");
}
namespace Creator {
// Historical spelling: this is an ECMAScript regular expression, not a PEG.
inline PatternSpec PEG(std::string_view p) { return PatternSpec::regex(std::string(p)); }
}
}
struct Symbol {
    enum class Kind { Lexeme, Keyword, Production };
    Kind kind;
    std::string name;
    PatternSpec pattern{};
    std::string body;
};
template<FixedString Name> class Lexeme {
    PatternSpec pattern_;
public:
    static constexpr auto name = Name.view();
    explicit Lexeme(PatternSpec p = Pattern::Repo::CIdent) : pattern_(std::move(p)) {}
    Symbol symbol() const { return {Symbol::Kind::Lexeme, std::string(name), pattern_, {}}; }
};
template<FixedString Name> class Keyword {
    PatternSpec pattern_;
public:
    static constexpr auto name = Name.view();
    Keyword() : pattern_(Pattern::Exact(name)) {}
    explicit Keyword(PatternSpec p) : pattern_(std::move(p)) {}
    Symbol symbol() const { return {Symbol::Kind::Keyword, std::string(name), pattern_, {}}; }
};
class ProductionSpec {
    std::string body_;
public:
    explicit ProductionSpec(std::string b = {}) : body_(std::move(b)) {}
    const std::string& body() const { return body_; }
};
namespace Production {
inline ProductionSpec New(std::string_view b) { return ProductionSpec(std::string(b)); }
}
template<FixedString Name> class ProductionRule {
    std::string body_;
public:
    static constexpr auto name = Name.view();
    ProductionRule() = default;
    ProductionRule(ProductionSpec p) : body_(p.body()) {}
    Symbol symbol() const { return {Symbol::Kind::Production, std::string(name), {}, body_}; }
};
class Registry {
    std::vector<Symbol> symbols_;
    std::unordered_map<std::string, std::size_t> index_;
public:
    template<class T> void add(const T& t) { add_symbol(t.symbol()); }
    void add_symbol(Symbol s) {
        if (s.name.empty()) throw std::invalid_argument("empty grammar symbol name");
        auto it = index_.find(s.name);
        if (it != index_.end()) symbols_[it->second] = std::move(s);
        else { index_.emplace(s.name, symbols_.size()); symbols_.push_back(std::move(s)); }
    }
    const Symbol* find(std::string_view n) const {
        auto it = index_.find(std::string(n));
        return it == index_.end() ? nullptr : &symbols_[it->second];
    }
    const std::vector<Symbol>& symbols() const { return symbols_; }
};
class GrammarBase {
public:
    virtual ~GrammarBase() = default;
    virtual void define(Registry&) const = 0;
    Registry registry() const { Registry r; define(r); return r; }
};
template<class Derived> class Grammar : public GrammarBase {
    void define(Registry& r) const override { static_cast<const Derived&>(*this).define_grammar(r); }
};
struct ParseResult {
    bool ok = false;
    std::size_t consumed = 0;
    std::string error;
    SourceSpan error_span{};
    // Deepest terminal failure seen anywhere, including inside repetitions
    // and optionals that later succeeded by matching less. Usually the most
    // helpful "expected ..." report when `error` is a trailing-input error.
    std::string expected;
    SourceSpan expected_span{};
    std::vector<std::string> expected_all;  // every distinct failure at expected_span
    static ParseResult failure(std::string message, SourceSpan where = {}, std::size_t consumed = 0) {
        ParseResult r; r.consumed = consumed; r.error = std::move(message); r.error_span = where; return r;
    }
    explicit operator bool() const { return ok; }
};
struct Options {
    bool skip_whitespace = true;
    std::size_t max_depth = 256;
    std::size_t max_operations = 1'000'000;
    std::size_t max_events = 1'000'000;
    std::size_t max_input_bytes = 64u * 1024u * 1024u;
    std::size_t max_grammar_bytes = 1u * 1024u * 1024u;
};
namespace detail {
inline bool space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
inline bool ident(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}
struct Expression {
    enum Kind { Empty, Literal, Reference, Sequence, Choice, Star, Plus, Optional, And, Not } kind = Empty;
    std::string text;
    std::vector<Expression> children;
    const Symbol* symbol = nullptr;  // resolved Reference target (per parse)
};
// Byte-set matcher for regex lexemes that are plain character classes, the
// common case for scannerless grammars: [set], [^set], (?![neg])[set], each
// optionally wrapped as (?:...)+ . Anything else uses std::regex.
struct ByteClass {
    bool table[256] = {};
    bool repeat = false;
    static bool escape(std::string_view s, std::size_t& i, bool (&t)[256], int& single) {
        // At the character after '\'.
        if (i >= s.size()) return false;
        const char c = s[i++];
        auto set_range = [&](unsigned lo, unsigned hi) { for (unsigned k = lo; k <= hi; ++k) t[k] = true; };
        single = -1;
        switch (c) {
        case 's': for (unsigned char k : {' ', '\t', '\n', '\v', '\f', '\r'}) t[k] = true; return true;
        case 'S': for (unsigned k = 0; k < 256; ++k) if (k != ' ' && (k < 9 || k > 13)) t[k] = true; return true;
        case 'd': set_range('0', '9'); return true;
        case 'D': for (unsigned k = 0; k < 256; ++k) if (k < '0' || k > '9') t[k] = true; return true;
        case 'w': set_range('a', 'z'); set_range('A', 'Z'); set_range('0', '9'); t[unsigned('_')] = true; return true;
        case 'W': for (unsigned k = 0; k < 256; ++k) if (!ident(static_cast<char>(k))) t[k] = true; return true;
        case 't': single = '\t'; return true;
        case 'n': single = '\n'; return true;
        case 'r': single = '\r'; return true;
        case 'f': single = '\f'; return true;
        case 'v': single = '\v'; return true;
        case 'x': {
            if (i + 2 > s.size()) return false;
            int v = 0;
            for (int k = 0; k < 2; ++k) {
                const char h = s[i++];
                v = v * 16 + (h >= '0' && h <= '9' ? h - '0' : h >= 'a' && h <= 'f' ? h - 'a' + 10 : h >= 'A' && h <= 'F' ? h - 'A' + 10 : -1000);
            }
            if (v < 0) return false;
            single = v; return true;
        }
        default:
            if (ident(c)) return false;
            single = static_cast<unsigned char>(c); return true;
        }
    }
    // Parse "[...]" at s[i]; fills t. Returns false on unsupported syntax.
    static bool bracket(std::string_view s, std::size_t& i, bool (&t)[256]) {
        if (i >= s.size() || s[i] != '[') return false;
        ++i;
        bool negate = false;
        if (i < s.size() && s[i] == '^') { negate = true; ++i; }
        bool local[256] = {};
        bool first = true;
        while (true) {
            if (i >= s.size()) return false;
            if (s[i] == ']' && !first) { ++i; break; }
            first = false;
            int lo = -1;
            if (s[i] == '\\') { ++i; if (!escape(s, i, local, lo)) return false; }
            else if (s[i] == '[') return false;
            else lo = static_cast<unsigned char>(s[i++]);
            if (lo < 0) continue;
            if (i + 1 < s.size() && s[i] == '-' && s[i + 1] != ']') {
                ++i;
                int hi = -1;
                if (s[i] == '\\') { ++i; bool scratch[256] = {}; if (!escape(s, i, scratch, hi) || hi < 0) return false; }
                else hi = static_cast<unsigned char>(s[i++]);
                if (hi < lo) return false;
                for (int k = lo; k <= hi; ++k) local[k] = true;
            } else local[lo] = true;
        }
        for (unsigned k = 0; k < 256; ++k) t[k] = negate ? !local[k] : local[k];
        return true;
    }
    static std::optional<ByteClass> compile(std::string_view s) {
        ByteClass c;
        std::size_t i = 0;
        auto body = [&](std::string_view b, bool (&t)[256]) {
            std::size_t j = 0;
            bool neg[256] = {}; bool has_neg = false;
            if (b.substr(0, 3) == "(?!") {
                j = 3;
                if (!bracket(b, j, neg) || j >= b.size() || b[j] != ')') return false;
                ++j; has_neg = true;
            }
            if (!bracket(b, j, t) || j != b.size()) return false;
            if (has_neg) for (unsigned k = 0; k < 256; ++k) t[k] = t[k] && !neg[k];
            return true;
        };
        if (s.substr(0, 3) == "(?:" && s.size() > 5 && s.substr(s.size() - 2) == ")+") {
            c.repeat = true;
            if (!body(s.substr(3, s.size() - 5), c.table)) return std::nullopt;
            return c;
        }
        (void)i;
        if (!body(s, c.table)) return std::nullopt;
        return c;
    }
    std::size_t match(std::string_view input, std::size_t at) const {
        std::size_t n = 0;
        while (at + n < input.size() && table[static_cast<unsigned char>(input[at + n])]) { ++n; if (!repeat) break; }
        return n;
    }
};
class GrammarReader {
    std::string_view s_;
    std::size_t p_ = 0;
    void ws() { while (p_ < s_.size() && space(s_[p_])) ++p_; }
    bool take(char c) { ws(); if (p_ < s_.size() && s_[p_] == c) { ++p_; return true; } return false; }
    Expression choice(std::size_t depth) {
        if (depth > 256) throw std::invalid_argument("grammar nesting limit exceeded");
        Expression e; e.kind = Expression::Choice; e.children.push_back(sequence(depth));
        while (take('|') || take('/')) e.children.push_back(sequence(depth));
        if (e.children.size() == 1) return std::move(e.children[0]);
        return e;
    }
    Expression sequence(std::size_t depth) {
        Expression e; e.kind = Expression::Sequence;
        while (true) {
            ws();
            if (p_ == s_.size() || s_[p_] == ')' || s_[p_] == '|' || s_[p_] == '/') break;
            e.children.push_back(atom(depth + 1));
        }
        return e;
    }
    Expression atom(std::size_t depth) {
        if (depth > 256) throw std::invalid_argument("grammar nesting limit exceeded");
        ws(); Expression e;
        if (take('&') || take('!')) {
            const auto op = s_[p_ - 1];
            e.kind = op == '&' ? Expression::And : Expression::Not;
            e.children.push_back(atom(depth + 1)); return e;
        }
        if (take('(')) {
            e = choice(depth + 1);
            if (!take(')')) throw std::invalid_argument("missing ')' in production");
        } else if (p_ < s_.size() && (s_[p_] == '\'' || s_[p_] == '"')) {
            const char quote = s_[p_++]; e.kind = Expression::Literal; bool closed = false;
            while (p_ < s_.size()) {
                char c = s_[p_++];
                if (c == quote) { closed = true; break; }
                if (c == '\\') {
                    if (p_ == s_.size()) break;
                    c = s_[p_++];
                    switch (c) {
                    case 'n': c = '\n'; break; case 't': c = '\t'; break; case 'r': c = '\r'; break;
                    case '\\': case '\'': case '"': break;
                    default: throw std::invalid_argument("invalid grammar literal escape");
                    }
                }
                e.text.push_back(c);
            }
            if (!closed) throw std::invalid_argument("unterminated grammar literal");
        } else {
            e.kind = Expression::Reference; const auto start = p_;
            while (p_ < s_.size() && (ident(s_[p_]) || s_[p_] == '@' || s_[p_] == '$' || s_[p_] == '#' || s_[p_] == '.')) ++p_;
            if (p_ == start) throw std::invalid_argument("invalid production atom");
            e.text = std::string(s_.substr(start, p_ - start));
        }
        ws();
        if (p_ < s_.size() && (s_[p_] == '*' || s_[p_] == '+' || s_[p_] == '?')) {
            const char op = s_[p_++]; Expression wrapper;
            wrapper.kind = op == '*' ? Expression::Star : op == '+' ? Expression::Plus : Expression::Optional;
            wrapper.children.push_back(std::move(e)); return wrapper;
        }
        return e;
    }
public:
    explicit GrammarReader(std::string_view s) : s_(s) {}
    Expression read() {
        auto e = choice(0); ws();
        if (p_ != s_.size()) throw std::invalid_argument("unexpected production suffix");
        return e;
    }
};
}
struct ByteClassHolder { detail::ByteClass cls; };
class Parser {
    Registry registry_;
    Options options_;
    EventStream events_;
    std::unordered_map<std::string, detail::Expression> expressions_;
    std::unordered_map<std::string, std::regex> regexes_;
    std::unordered_map<std::string, ByteClassHolder> classes_;
    // Lazily formatted failure: prefix + *subject + suffix.
    struct Failure { const char* prefix = nullptr; const std::string* subject = nullptr; const char* suffix = ""; bool operator==(const Failure&) const = default; };
    static std::string text(const Failure& f) { return f.prefix ? std::string(f.prefix) + (f.subject ? *f.subject : std::string()) + f.suffix : std::string(); }
    Failure error_f_{}, deepest_f_{};
    std::vector<Failure> deepest_list_;
    std::size_t lookahead_ = 0;
    std::unordered_map<const Symbol*, const detail::Expression*> bodies_;
    std::unordered_map<const Symbol*, const ByteClassHolder*> fast_;
    std::unordered_map<const Symbol*, const std::regex*> slow_;
    std::string grammar_error_, error_, fatal_;
    std::string_view input_;
    std::size_t cursor_ = 0, farthest_ = 0, operations_ = 0, deepest_ = 0;
    std::vector<Event> pending_;
    std::vector<std::pair<const Symbol*, std::size_t>> active_;
    std::vector<std::size_t> lines_;
    bool parsing_ = false;

    void compile() {
        try {
            std::size_t size = 0;
            for (const auto& s : registry_.symbols()) {
                if (s.body.size() > options_.max_grammar_bytes - std::min(size, options_.max_grammar_bytes) ||
                    s.pattern.source.size() > options_.max_grammar_bytes - std::min(size + s.body.size(), options_.max_grammar_bytes))
                    throw std::invalid_argument("grammar size limit exceeded");
                size += s.body.size() + s.pattern.source.size();
                if (s.kind == Symbol::Kind::Production) expressions_.emplace(s.name, detail::GrammarReader(s.body).read());
                else if (s.pattern.kind == PatternSpec::Kind::Regex) {
                    regexes_.emplace(s.name, std::regex(s.pattern.source));
                    if (auto fast = detail::ByteClass::compile(s.pattern.source)) classes_.emplace(s.name, ByteClassHolder{*fast});
                }
            }
            std::function<void(const detail::Expression&)> check = [&](const auto& e) {
                if (e.kind == detail::Expression::Reference && !registry_.find(e.text))
                    throw std::invalid_argument("unknown grammar symbol: " + e.text);
                for (const auto& c : e.children) check(c);
            };
            for (const auto& [name, e] : expressions_) { (void)name; check(e); }
        } catch (const std::exception& e) { grammar_error_ = e.what(); }
    }
    // Bind references and lexeme matchers to this object's tables (cheap;
    // done per parse so copies of a Parser stay self-consistent).
    void resolve() {
        bodies_.clear(); fast_.clear(); slow_.clear();
        for (const auto& s : registry_.symbols()) {
            if (s.kind == Symbol::Kind::Production) {
                if (auto it = expressions_.find(s.name); it != expressions_.end()) bodies_[&s] = &it->second;
            } else if (s.pattern.kind == PatternSpec::Kind::Regex) {
                if (auto it = classes_.find(s.name); it != classes_.end()) fast_[&s] = &it->second;
                if (auto it = regexes_.find(s.name); it != regexes_.end()) slow_[&s] = &it->second;
            }
        }
        std::function<void(detail::Expression&)> bind = [&](detail::Expression& e) {
            if (e.kind == detail::Expression::Reference) e.symbol = registry_.find(e.text);
            for (auto& c : e.children) bind(c);
        };
        for (auto& [name, e] : expressions_) { (void)name; bind(e); }
    }
    SourceSpan span(std::size_t p, std::size_t n = 0) const {
        auto it = std::upper_bound(lines_.begin(), lines_.end(), p);
        const auto line = static_cast<std::size_t>(it - lines_.begin());
        return {p, n, line, p - lines_[line - 1] + 1};
    }
    void ws() { if (options_.skip_whitespace) while (cursor_ < input_.size() && detail::space(input_[cursor_])) ++cursor_; }
    bool fail(const char* prefix, const std::string* subject = nullptr, const char* suffix = "") {
        const Failure f{prefix, subject, suffix};
        if (!lookahead_) {
            if (!deepest_f_.prefix || cursor_ > deepest_) { deepest_ = cursor_; deepest_f_ = f; deepest_list_.clear(); }
            if (cursor_ == deepest_ && deepest_list_.size() < 256 && std::find(deepest_list_.begin(), deepest_list_.end(), f) == deepest_list_.end())
                deepest_list_.push_back(f);
        }
        if (!error_f_.prefix || cursor_ >= farthest_) { farthest_ = cursor_; error_f_ = f; }
        return false;
    }
    bool event(EventKind kind, std::string name, std::size_t p, std::size_t n, std::size_t depth) {
        if (pending_.size() >= options_.max_events) { fatal_ = "parse event limit exceeded"; return false; }
        pending_.push_back({kind, std::move(name), kind == EventKind::Token ? std::string(input_.substr(p, n)) : std::string{}, span(p, n), depth, false});
        return true;
    }
    bool symbol(const Symbol& s, std::size_t depth) {
        ws(); const auto start = cursor_;
        if (s.kind == Symbol::Kind::Production) {
            const auto key = std::make_pair(&s, cursor_);
            if (std::find(active_.begin(), active_.end(), key) != active_.end()) {
                fatal_ = "left recursion detected at " + s.name; return false;
            }
            if (!event(EventKind::RuleBegin, s.name, start, 0, depth)) return false;
            active_.push_back(key);
            auto body = bodies_.find(&s);
            const bool ok = eval(body != bodies_.end() ? *body->second : expressions_.at(s.name), depth + 1);
            active_.pop_back();
            return ok && event(EventKind::RuleEnd, s.name, start, cursor_ - start, depth);
        }
        std::size_t n = 0;
        if (s.pattern.kind == PatternSpec::Kind::Exact) {
            if (input_.substr(cursor_, s.pattern.source.size()) != s.pattern.source) return fail("expected ", &s.name);
            n = s.pattern.source.size();
        } else if (auto fast = fast_.find(&s); fast != fast_.end()) {
            n = fast->second->cls.match(input_, cursor_);
            if (!n) return fail("expected ", &s.name);
        } else {
            std::match_results<std::string_view::const_iterator> m;
            auto slow = slow_.find(&s);
            if (!std::regex_search(input_.begin() + static_cast<std::ptrdiff_t>(cursor_), input_.end(), m,
                    slow != slow_.end() ? *slow->second : regexes_.at(s.name), std::regex_constants::match_continuous)) return fail("expected ", &s.name);
            n = static_cast<std::size_t>(m.length());
        }
        if (n == 0) { fatal_ = "empty terminal match: " + s.name; return false; }
        if (s.kind == Symbol::Kind::Keyword && detail::ident(input_[cursor_ + n - 1]) && cursor_ + n < input_.size() && detail::ident(input_[cursor_ + n]))
            return fail("keyword boundary mismatch: ", &s.name);
        cursor_ += n;
        return event(EventKind::Token, s.name, start, n, depth);
    }
    bool eval(const detail::Expression& e, std::size_t depth) {
        using E = detail::Expression;
        if (++operations_ > options_.max_operations || depth > options_.max_depth) {
            fatal_ = "parse operation or depth limit exceeded"; return false;
        }
        if (!fatal_.empty()) return false;
        const auto save = cursor_, event_count = pending_.size();
        switch (e.kind) {
        case E::Empty: return true;
        case E::Literal: {
            ws(); const auto p = cursor_;
            if (input_.substr(p, e.text.size()) != e.text) return fail("expected literal '", &e.text, "'");
            cursor_ += e.text.size(); return event(EventKind::Token, e.text, p, e.text.size(), depth);
        }
        case E::Reference: return symbol(e.symbol ? *e.symbol : *registry_.find(e.text), depth);
        case E::Sequence:
            for (const auto& c : e.children) if (!eval(c, depth + 1)) return false;
            return true;
        case E::Choice:
            for (const auto& c : e.children) {
                cursor_ = save; pending_.resize(event_count);
                if (eval(c, depth + 1)) return true;
                if (!fatal_.empty()) return false;
            }
            return false;
        case E::And: case E::Not: {
            const auto old_error = error_f_; const auto old_far = farthest_;
            ++lookahead_;
            const bool ok = eval(e.children[0], depth + 1);
            --lookahead_;
            cursor_ = save; pending_.resize(event_count); error_f_ = old_error; farthest_ = old_far;
            if (!fatal_.empty()) return false;
            return (e.kind == E::And ? ok : !ok) || fail("lookahead failed");
        }
        case E::Star: case E::Plus: case E::Optional: {
            std::size_t count = 0;
            while (true) {
                const auto p = cursor_, n = pending_.size(), old_far = farthest_;
                const auto old_error = error_f_;
                if (!eval(e.children[0], depth + 1)) {
                    cursor_ = p; pending_.resize(n);
                    if (!fatal_.empty()) return false;
                    if (e.kind == E::Plus && count == 0) return false;
                    error_f_ = old_error; farthest_ = old_far; return true;
                }
                ++count;
                if (e.kind == E::Optional) return true;
                if (cursor_ == p) { fatal_ = "repetition matched empty input"; return false; }
            }
        }
        }
        return false;
    }
public:
    explicit Parser(const GrammarBase& g, Options o = {}) : Parser(g.registry(), o) {}
    explicit Parser(Registry r, Options o = {}) : registry_(std::move(r)), options_(o) { compile(); }
    EventStream& events() { return events_; }
    const EventStream& events() const { return events_; }
    void subscribe(Listener& l) { events_.subscribe(l); }
    void subscribe(EventStream::Callback cb) { events_.subscribe(std::move(cb)); }
    const std::string& grammar_error() const noexcept { return grammar_error_; }
    ParseResult parse(std::string_view input, std::string_view start_rule, bool require_eof = true) {
        if (parsing_) return ParseResult::failure("Parser::parse is not reentrant");
        parsing_ = true;
        struct Reset { bool& flag; ~Reset() { flag = false; } } reset{parsing_};
        input_ = input.empty() ? std::string_view("") : input; cursor_ = farthest_ = operations_ = deepest_ = lookahead_ = 0;
        error_.clear(); error_f_ = {}; deepest_f_ = {}; deepest_list_.clear();
        resolve(); fatal_.clear(); pending_.clear(); active_.clear(); lines_ = {0};
        if (input.size() > options_.max_input_bytes) return ParseResult::failure("input size limit exceeded");
        for (std::size_t i = 0; i < input.size(); ++i) if (input[i] == '\n') lines_.push_back(i + 1);
        bool ok = false;
        const auto* start = registry_.find(start_rule);
        if (!grammar_error_.empty()) fatal_ = grammar_error_;
        else if (!start || start->kind != Symbol::Kind::Production) fatal_ = "unknown start production: " + std::string(start_rule);
        else ok = symbol(*start, 0);
        if (ok && require_eof) { ws(); if (cursor_ != input.size()) ok = fail("unexpected trailing input"); }
        error_ = text(error_f_);
        if (!fatal_.empty()) { ok = false; error_ = fatal_; farthest_ = cursor_; }
        if (!ok) {
            pending_.clear();
            events_.emit({EventKind::ParseError, {}, error_, span(farthest_), 0, false});
            auto failed = ParseResult::failure(error_, span(farthest_), cursor_);
            for (const auto& f : deepest_list_) {
                auto t = text(f);
                if (std::find(failed.expected_all.begin(), failed.expected_all.end(), t) == failed.expected_all.end()) failed.expected_all.push_back(std::move(t));
            }
            failed.expected = deepest_f_.prefix ? text(deepest_f_) : error_;
            failed.expected_span = deepest_f_.prefix ? span(deepest_) : span(farthest_);
            return failed;
        }
        events_.emit_all(pending_);
        ParseResult done; done.ok = true; done.consumed = cursor_; done.error_span = span(cursor_);
        return done;
    }
};
} // namespace parsetk
