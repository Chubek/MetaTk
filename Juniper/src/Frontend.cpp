#include "Internal.hpp"
#include <charconv>
#include <cctype>
#include <set>

namespace juniper::detail {
namespace {
struct Failure { Diagnostic diagnostic; };
class Reader {
    std::string_view source_;
    std::string file_;
    std::size_t pos_ = 0, start_ = 0, pattern_nodes_ = 0;
    std::string token_;
public:
    Reader(std::string_view s, std::string file) : source_(s), file_(std::move(file)) { next(); }
    const std::string& token() const { return token_; }
    std::size_t start() const { return start_; }
    [[noreturn]] void fail(std::string message) const {
        Diagnostic d{"JUN-E101", file_, std::move(message), 1, 1};
        for (std::size_t i = 0; i < start_; ++i) {
            if (source_[i] == '\n') { ++d.line; d.column = 1; } else ++d.column;
        }
        throw Failure{std::move(d)};
    }
    void next() {
        while (pos_ < source_.size()) {
            char c = source_[pos_];
            if (std::isspace(static_cast<unsigned char>(c))) { ++pos_; continue; }
            if (source_.substr(pos_, 2) == "//" || source_.substr(pos_, 2) == "--" || c == '#') {
                auto n = source_.find('\n', pos_); pos_ = n == std::string_view::npos ? source_.size() : n + 1; continue;
            }
            break;
        }
        start_ = pos_;
        if (pos_ == source_.size()) { token_.clear(); return; }
        if (source_.substr(pos_, 2) == "=>") { token_ = "=>"; pos_ += 2; return; }
        auto word = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
        ++pos_;
        if (word(source_[start_]) || source_[start_] == '-')
            while (pos_ < source_.size() && word(source_[pos_])) ++pos_;
        token_ = std::string(source_.substr(start_, pos_ - start_));
    }
    bool accept(std::string_view s) { if (token_ != s) return false; next(); return true; }
    void expect(std::string_view s) { if (!accept(s)) fail("expected '" + std::string(s) + "', found '" + token_ + "'"); }
    std::string word() {
        if (!identifier(token_)) fail("expected identifier, found '" + token_ + "'");
        auto out = token_; next(); return out;
    }
    std::int64_t integer() {
        std::int64_t n = 0;
        auto [p, ec] = std::from_chars(token_.data(), token_.data() + token_.size(), n);
        if (ec != std::errc{} || p != token_.data() + token_.size() || token_.empty()) fail("expected signed-64 decimal integer");
        next(); return n;
    }
    std::uint32_t positive(std::uint32_t maximum) {
        auto n = integer();
        if (n <= 0 || n > maximum) fail("integer must be in 1.." + std::to_string(maximum));
        return static_cast<std::uint32_t>(n);
    }
    Pattern pattern(unsigned depth = 1) {
        if (depth > 32 || ++pattern_nodes_ > 16384) fail("pattern depth/node budget exceeded");
        auto name = word();
        if (name == "reg") return Pattern::reg();
        if (name == "imm") {
            if (!accept("(")) return Pattern::imm();
            auto lo = integer(); expect(","); auto hi = integer(); expect(")");
            if (lo > hi) fail("immediate minimum exceeds maximum");
            return Pattern::imm(lo, hi);
        }
        expect("(");
        std::vector<Pattern> args;
        if (!accept(")")) {
            do { args.push_back(pattern(depth + 1)); if (args.size() > 3) fail("operator arity exceeds 3"); } while (accept(","));
            expect(")");
        }
        return Pattern::operation(std::move(name), std::move(args));
    }
    Backend backend() {
        Backend b;
        b.name = word();
        if (accept("extends")) {
            expect("portable"); auto name = b.name; b = Backend::portable(); b.name = std::move(name);
        }
        expect("{");
        bool issue_seen = false;
        std::set<std::string> resources;
        while (!accept("}")) {
            if (token_.empty()) fail("unterminated backend");
            if (accept("issue")) {
                if (issue_seen) fail("duplicate issue declaration");
                issue_seen = true; b.issue_width = positive(64); expect(";");
            } else if (accept("resource")) {
                auto name = word();
                if (!resources.insert(name).second) fail("duplicate resource '" + name + "'");
                b.resources[name] = positive(64); expect(";");
                if (b.resources.size() > 64) fail("resource limit exceeded");
            } else if (accept("rule")) {
                Rule r; r.name = word(); expect(":"); r.pattern = pattern(); expect("=>"); r.opcode = word();
                std::set<std::string> attributes;
                while (!accept(";")) {
                    auto attr = word();
                    if (!attributes.insert(attr).second) fail("duplicate rule attribute '" + attr + "'");
                    if (attr == "cost") r.cost = positive(1000000);
                    else if (attr == "latency") r.latency = positive(1000000);
                    else if (attr == "resource") r.resource = word();
                    else fail("unknown rule attribute '" + attr + "'");
                }
                b.rules.push_back(std::move(r));
                if (b.rules.size() > 1024) fail("rule limit exceeded");
            } else fail("expected issue, resource, rule, or '}'");
        }
        return b;
    }
};
} // namespace

Result<std::pair<Backend, std::string>> extract_backend(std::string_view source, const std::string& file) {
    if (source.size() > 8 * 1024 * 1024 || source.find('\0') != std::string_view::npos || file.find('\0') != std::string::npos)
        return error("JUN-E103", "source exceeds 8 MiB or contains NUL", file);
    try {
        Reader r(source, file);
        if (r.accept("juniper")) { if (r.integer() != 1) r.fail("unsupported Juniper IR version (expected 1)"); r.expect(";"); }
        Backend backend = Backend::portable();
        if (r.accept("backend")) backend = r.backend();
        auto valid = validate_backend(backend); if (!valid) { auto d = *valid.error(); d.file = file; return d; }
        std::string body(source);
        for (std::size_t i = 0; i < r.start(); ++i) if (body[i] != '\n' && body[i] != '\r') body[i] = ' ';
        return std::make_pair(std::move(backend), std::move(body));
    } catch (const Failure& f) { return f.diagnostic; }
}
} // namespace juniper::detail
