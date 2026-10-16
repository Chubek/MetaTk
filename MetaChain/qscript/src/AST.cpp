// QScript front end: the scannerless QScript grammar (qscript/grammar/
// QScript.grammar, compiled by QScript-Grammar.h onto ParseTk) and the
// translation of its match tree into the QScript AST.
#include "qscript/QScript.h"
#include "qscript/include/QScript-Grammar.h"
#include "qscript/src/Internal.h"
#include <cctype>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>

namespace qscript {
const grammar::Grammar& language_grammar() {
    static const grammar::Grammar g = [] {
        auto compiled = grammar::Grammar::compile(detail::embedded_grammar());
        if (!compiled.ok()) throw std::logic_error("built-in QScript grammar is invalid: " + compiled.error);
        return compiled;
    }();
    return g;
}

namespace {
using grammar::Match;
using dsl::ASTNode;

struct Failure { Diagnostic diagnostic; };

class Builder {
    Options options_;
    std::string file_;
    std::size_t nodes_ = 0;

    [[noreturn]] void fail(std::string message, const Match& at) const {
        throw Failure{{metachain::Stage::Parse, std::move(message), file_, at.line, at.column, at.from}};
    }
    void count(const Match& at) { if (++nodes_ > options_.max_nodes) fail("AST node limit exceeded", at); }
    ASTNode leaf(std::string tag, std::string value, const Match& at) { count(at); return ASTNode(std::move(tag), std::move(value)); }
    ASTNode node(std::string tag, std::vector<ASTNode> children, const Match& at) { count(at); return ASTNode(std::move(tag), std::move(children)); }
    ASTNode located(std::string tag, std::vector<ASTNode> children, const Match& at) {
        children.push_back(leaf("@", detail::encode_location(at.line, at.column, file_), at));
        return node(std::move(tag), std::move(children), at);
    }
    void depth(std::size_t d, const Match& at) const { if (d > options_.max_depth) fail("syntax depth limit exceeded", at); }
    const Match& need(const Match& m, std::string_view name) const {
        auto* c = m.find(name);
        if (!c) fail("internal: missing <" + std::string(name) + "> in <" + m.name + ">", m);
        return *c;
    }

    // ---- expressions --------------------------------------------------
    struct Expr { ASTNode node; std::size_t height = 1; };
    Expr make(std::string tag, std::vector<Expr> kids, const Match& at, bool locate = false) {
        std::size_t h = 0; std::vector<ASTNode> children;
        for (auto& k : kids) { h = std::max(h, k.height); children.push_back(std::move(k.node)); }
        if (h + 1 > options_.max_depth) fail("AST depth limit exceeded", at);
        return {locate ? located(std::move(tag), std::move(children), at) : node(std::move(tag), std::move(children), at), h + 1};
    }
    Expr integer(std::int64_t v, const Match& at) { return {leaf("integer", std::to_string(v), at), 1}; }
    static std::string clean_number(std::string_view text) {
        std::string r;
        for (char c : text) if (c != '_') r += c;
        return r;
    }
    Expr number(const Match& m, bool negative) {
        auto text = clean_number(m.text);
        auto value = metachain::text::signed_integer(negative ? "-" + text : text);
        if (!value) fail(negative ? "integer outside signed 64-bit range" : "invalid signed 64-bit integer", m);
        return integer(*value, m);
    }
    Expr character(const Match& m) {
        const auto& t = m.text;  // includes the quotes
        std::int64_t v = 0;
        if (t.size() >= 3 && t[1] != '\\') v = static_cast<unsigned char>(t[1]);
        else if (t.size() >= 4) {
            switch (t[2]) {
            case 'n': v = '\n'; break; case 'r': v = '\r'; break; case 't': v = '\t'; break;
            case '0': v = 0; break; case '\\': v = '\\'; break; case '\'': v = '\''; break; case '"': v = '"'; break;
            case 'x': v = std::stoll(t.substr(3, 2), nullptr, 16); break;
            default: fail("invalid character escape", m);
            }
        }
        return integer(v, m);
    }
    Expr term(const Match& t, std::size_t d, bool negate_literal) {
        if (t.children.empty()) fail("internal: empty term", t);
        const auto& m = t.children.front();
        if (m.name == "number") return number(m, negate_literal);
        if (m.name == "char") return character(m);
        if (m.name == "true") return integer(1, m);
        if (m.name == "false") return integer(0, m);
        if (m.name == "var") return {leaf("name", m.text, m), 1};
        if (m.name == "group") return expr(need(m, "expr"), d + 1);
        if (m.name == "call") {
            std::vector<Expr> kids;
            kids.push_back({leaf("name", need(m, "qname").text, m), 1});
            if (auto* args = m.find("args"))
                for (const auto* a : args->all("expr")) kids.push_back(expr(*a, d + 1));
            return make("call", std::move(kids), m, true);
        }
        fail("internal: unknown term <" + m.name + ">", m);
    }
    static int precedence(std::string_view op) {
        if (op == "||") return 1;
        if (op == "&&") return 2;
        if (op == "|") return 3;
        if (op == "^") return 4;
        if (op == "&") return 5;
        if (op == "==" || op == "!=") return 6;
        if (op == "<" || op == "<=" || op == ">" || op == ">=") return 7;
        if (op == "<<" || op == ">>") return 8;
        if (op == "+" || op == "-") return 9;
        if (op == "*" || op == "/" || op == "%") return 10;
        if (op == "**") return 12;
        return 0;
    }
    static constexpr int prefix_precedence = 11;  // tighter than * but looser than **
    Expr binary(const Match& b, std::size_t d) {
        // Shunting-yard over the flat operand/infix sequence. Prefix operators
        // are pushed as unary operators so that -2**2 == -(2**2).
        struct Op { std::string tag; int prec; bool unary; bool right; const Match* at; };
        std::vector<Expr> out; std::vector<Op> ops;
        auto reduce = [&] {
            Op op = std::move(ops.back()); ops.pop_back();
            if (op.unary) {
                if (out.empty()) fail("internal: operand stack underflow", *op.at);
                auto a = std::move(out.back()); out.pop_back();
                out.push_back(make(op.tag, {std::move(a)}, *op.at));
            } else {
                if (out.size() < 2) fail("internal: operand stack underflow", *op.at);
                auto rhs = std::move(out.back()); out.pop_back();
                auto lhs = std::move(out.back()); out.pop_back();
                out.push_back(make(op.tag, {std::move(lhs), std::move(rhs)}, *op.at));
            }
        };
        for (const auto& c : b.children) {
            if (c.name == "operand") {
                std::vector<const Match*> prefixes;
                for (const auto& p : c.children) if (p.name == "prefix") prefixes.push_back(&p);
                const auto& t = need(c, "term");
                // Fold -<literal> so INT64_MIN is writable; only when the
                // magnitude does not fit positively (keeps -2**2 == -4).
                bool fold = false;
                if (!prefixes.empty() && prefixes.back()->sym == "-" && !t.children.empty() && t.children[0].name == "number") {
                    auto text = clean_number(t.children[0].text);
                    fold = !metachain::text::signed_integer(text);
                }
                if (fold) prefixes.pop_back();
                for (const auto* p : prefixes) {
                    const auto& s = p->sym;
                    if (s == "+") continue;
                    ops.push_back({s == "-" ? "neg" : (s == "!" || s == "not") ? "not" : "invert", prefix_precedence, true, true, p});
                }
                out.push_back(term(t, d, fold));
            } else if (c.name == "infix") {
                std::string tag = c.sym == "or" ? "||" : c.sym == "and" ? "&&" : c.sym;
                const int prec = precedence(tag);
                const bool right = tag == "**";
                while (!ops.empty() && (ops.back().prec > prec || (ops.back().prec == prec && !right))) reduce();
                ops.push_back({tag, prec, false, right, &c});
            }
        }
        while (!ops.empty()) reduce();
        if (out.size() != 1) fail("internal: malformed operator expression", b);
        return std::move(out.back());
    }
    Expr expr(const Match& e, std::size_t d) {
        depth(d, e);
        const Match& t = e.find("ternary") ? *e.find("ternary") : e;
        Expr cond = binary(need(t, "binary"), d);
        if (auto* then = t.find("then")) {
            auto a = expr(*then, d + 1);
            auto b = expr(need(t, "else"), d + 1);
            return make("?:", {std::move(cond), std::move(a), std::move(b)}, t);
        }
        return cond;
    }
    ASTNode expression(const Match& e, std::size_t d) { return expr(e, d).node; }
    ASTNode negate(ASTNode n, const Match& at) { return node("not", {std::move(n)}, at); }

    // ---- statements ---------------------------------------------------
    ASTNode name(const Match& m) { return leaf("name", m.text, m); }
    ASTNode assignment(const Match& a, std::size_t d) {
        const auto& target = need(a, "name");
        const auto& op = need(a, "assignop").sym;
        ASTNode value = expression(need(a, "expr"), d + 1);
        if (op != "=") {
            const auto bin = op.substr(0, op.size() - 1);
            value = node(bin, {name(target), std::move(value)}, a);
        }
        return located("assign", {name(target), std::move(value)}, a);
    }
    ASTNode simple(const Match& s, std::size_t d) {
        const auto& m = s.children.front();
        if (m.name == "assign") return assignment(m, d);
        if (m.name == "incdec") {
            const auto& target = need(m, "name");
            auto one = leaf("integer", "1", m);
            return located("assign", {name(target), node(need(m, "incop").text == "++" ? "+" : "-", {name(target), std::move(one)}, m)}, m);
        }
        if (m.name == "let") return let(m, d);
        return located("expr", {expression(m, d + 1)}, s);
    }
    ASTNode let(const Match& m, std::size_t d) {
        return located("let", {name(need(m, "name")), expression(need(m, "expr"), d + 1)}, m);
    }
    ASTNode with_modifier(ASTNode stmt, const Match& s, std::size_t d) {
        auto* mod = s.find("modifier");
        if (!mod) return stmt;
        auto cond = expression(need(*mod, "expr"), d + 1);
        if (mod->has("unless")) cond = negate(std::move(cond), *mod);
        return located("if", {std::move(cond), std::move(stmt)}, *mod);
    }
    ASTNode block(const Match& b, std::size_t d) {
        std::vector<ASTNode> body;
        for (const auto* s : b.all("statement")) body.push_back(statement(*s, d + 1));
        return node("block", std::move(body), b);
    }
    ASTNode statement(const Match& s, std::size_t d) {
        depth(d, s);
        const auto& sym = s.sym;
        auto cond = [&](const Match& m) { return expression(need(need(m, "cond"), "expr"), d + 1); };
        if (sym == "block") return block(need(s, "block"), d);
        if (sym == "let") return let(need(s, "let"), d);
        if (sym == "empty") return node("block", {}, s);
        if (sym == "if" || sym == "unless") {
            std::vector<ASTNode> children;
            auto c = cond(s);
            if (sym == "unless") c = negate(std::move(c), s);
            ASTNode otherwise;
            bool has_else = false;
            if (auto* e = s.find("else")) { otherwise = statement(*e, d + 1); has_else = true; }
            auto elsifs = s.all("elsif");
            for (auto it = elsifs.rbegin(); it != elsifs.rend(); ++it) {
                std::vector<ASTNode> kids{cond(**it), statement(need(**it, "statement"), d + 1)};
                if (has_else) kids.push_back(std::move(otherwise));
                otherwise = located("if", std::move(kids), **it); has_else = true;
            }
            children.push_back(std::move(c));
            children.push_back(statement(need(s, "then"), d + 1));
            if (has_else) children.push_back(std::move(otherwise));
            return located("if", std::move(children), s);
        }
        if (sym == "while" || sym == "until") {
            auto c = cond(s);
            if (sym == "until") c = negate(std::move(c), s);
            return located("while", {std::move(c), statement(need(s, "statement"), d + 1)}, s);
        }
        if (sym == "do") {
            auto c = cond(s);
            if (s.has("until")) c = negate(std::move(c), s);
            return located("do", {block(need(s, "block"), d + 1), std::move(c)}, s);
        }
        if (sym == "for") {
            ASTNode init = node("block", {}, s), step = node("block", {}, s);
            if (auto* i = s.find("init")) {
                const auto& m = i->children.front();
                init = m.name == "let" ? let(m, d + 1) : simple(m, d + 1);
            }
            ASTNode test = s.find("test") ? expression(need(s, "test"), d + 1) : leaf("integer", "1", s);
            if (auto* st = s.find("step")) step = simple(*st, d + 1);
            return located("for", {std::move(init), std::move(test), std::move(step), statement(need(s, "statement"), d + 1)}, s);
        }
        if (sym == "loop") return located("while", {leaf("integer", "1", s), block(need(s, "block"), d + 1)}, s);
        if (sym == "break" || sym == "continue") return with_modifier(located(sym, {}, s), s, d);
        if (sym == "return" || sym == "exit") {
            auto value = s.find("expr") ? expression(need(s, "expr"), d + 1) : leaf("integer", "0", s);
            return with_modifier(located(sym, {std::move(value)}, s), s, d);
        }
        if (sym == "print") {
            std::vector<ASTNode> values;
            for (const auto* e : s.all("expr")) values.push_back(expression(*e, d + 1));
            return with_modifier(located("print", std::move(values), s), s, d);
        }
        if (sym == "assert") return with_modifier(located("assert", {expression(need(s, "expr"), d + 1)}, s), s, d);
        if (sym == "simple") return with_modifier(simple(need(s, "simple"), d), s, d);
        fail("internal: unknown statement <" + sym + ">", s);
    }
    ASTNode item(const Match& i) {
        const auto& m = i.children.front();
        if (m.name == "statement") return statement(m, 0);
        if (m.name == "use") {
            std::vector<ASTNode> libs;
            for (const auto* l : m.all("libname")) libs.push_back(leaf("name", l->text, *l));
            return located("use", std::move(libs), m);
        }
        if (m.name == "const") return located("const", {name(need(m, "name")), expression(need(m, "expr"), 1)}, m);
        if (m.name == "fn") {
            std::vector<ASTNode> params;
            if (auto* p = m.find("params")) for (const auto* id : p->all("ident")) params.push_back(name(*id));
            return located("fn", {name(need(m, "name")), node("params", std::move(params), m), block(need(m, "block"), 1)}, m);
        }
        fail("internal: unknown item <" + m.name + ">", m);
    }
public:
    Builder(Options o, std::string f) : options_(std::move(o)), file_(std::move(f)) {
        options_.max_depth = std::min<std::size_t>(options_.max_depth, 256);
    }
    Result<AST> run(std::string_view source) {
        Result<AST> r;
        try {
            if (source.size() > options_.toolchain.il.max_source_bytes)
                throw Failure{{metachain::Stage::Parse, "source size limit exceeded", file_, 0, 0, 0}};
            grammar::ParseOptions po;
            po.max_input_bytes = options_.toolchain.il.max_source_bytes;
            auto parsed = language_grammar().parse(source, "TOP", po);
            if (!parsed) {
                std::string message = parsed.error;
                if (message == "parse operation or depth limit exceeded") message = "syntax depth or complexity limit exceeded";
                else if (!parsed.expected.empty()) {
                    // Closing punctuation is nearly always what is missing.
                    for (std::string_view closer : {"')'", "'}'", "';'", "':'"})
                        if (std::find(parsed.expected.begin(), parsed.expected.end(), closer) != parsed.expected.end()) {
                            message = "expected " + std::string(closer); break;
                        }
                    if (message == parsed.error && parsed.expected.size() > 4) message = "syntax error";
                    if (message == "expected '''") message = "expected closing quote";
                    const auto& f = parsed.found;
                    if (f.empty()) message += " at end of input";
                    else {
                        std::size_t n = 1;
                        if (std::isalnum(static_cast<unsigned char>(f[0])) || f[0] == '_')
                            while (n < f.size() && (std::isalnum(static_cast<unsigned char>(f[n])) || f[n] == '_')) ++n;
                        message += " before \"" + f.substr(0, n) + "\"";
                    }
                }
                throw Failure{{metachain::Stage::Parse, message, file_, parsed.line, parsed.column, parsed.offset}};
            }
            const Match& top = *parsed.match;
            std::vector<ASTNode> items;
            for (const auto* i : top.all("item")) items.push_back(item(*i));
            r.value = AST{node("program", std::move(items), top)};
            if (!visit(*r, [](const auto&) { return true; }, options_.max_depth)) {
                r.value.reset();
                throw Failure{{metachain::Stage::Parse, "AST depth limit exceeded", file_, 0, 0, 0}};
            }
        } catch (const Failure& e) {
            r.value.reset();
            r.diagnostics.push_back(e.diagnostic);
        } catch (const std::exception& e) {
            r.value.reset();
            r.diagnostics.push_back({metachain::Stage::Parse, e.what(), file_, 0, 0, 0});
        }
        return r;
    }
};
} // namespace

Result<AST> parse(std::string_view source, Options o, std::string file) {
    return Builder(std::move(o), std::move(file)).run(source);
}
} // namespace qscript
