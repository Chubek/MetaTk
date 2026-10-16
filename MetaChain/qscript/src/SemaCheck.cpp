// QScript semantic analysis: `use` resolution, constants, name binding and
// structural checks. lower() consumes the same Analysis.
#include "qscript/QScript.h"
#include "qscript/src/Analysis.h"
#include <fstream>
#include <functional>
#include <sstream>

namespace qscript {
namespace detail {
namespace {
// The hidden core library: helpers the language itself lowers to.
const char* core_source = R"QS(
// `a ** b`: integer power with wrap-around. A negative exponent yields the
// truncated reciprocal: 1 for base 1, +-1 for base -1, 0 otherwise.
fn pow(base, exp) {
    if (exp < 0) {
        if (base == 1) return 1;
        if (base == -1) return (exp & 1) ? -1 : 1;
        return 0;
    }
    let result = 1;
    while (exp > 0) {
        if (exp & 1) result = result * base;
        exp = exp >> 1;
        if (exp > 0) base = base * base;
    }
    return result;
}
)QS";

const std::set<std::string_view> binary_ops{"+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>", "==", "!=", "<", ">", "<=", ">=", "&&", "||", "**"};
bool unary_op(std::string_view t) { return t == "neg" || t == "not" || t == "invert"; }

bool valid_library_name(std::string_view n) {
    if (n.empty()) return false;
    std::size_t start = 0;
    while (true) {
        auto end = n.find("::", start);
        auto part = n.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
        if (part.empty() || !metachain::text::alpha(part[0])) return false;
        for (char c : part) if (!metachain::text::alpha(c) && !metachain::text::digit(c)) return false;
        if (end == std::string_view::npos) return true;
        start = end + 2;
    }
}

class Analyzer {
    const Options& o_;
    Analysis& a_;
    std::string program_file_;

    void error(std::string message, const dsl::ASTNode* at, const Location* fallback = nullptr, metachain::Stage stage = metachain::Stage::Verify) {
        Location l = at ? location(*at) : Location{};
        if (!l.line && fallback) l = *fallback;
        a_.diagnostics.push_back({stage, std::move(message), l.file, l.line, l.column, 0});
    }

    std::optional<std::pair<std::string, std::string>> find_library(const std::string& name) {
        if (name == core_unit) return std::make_pair(std::string(core_source), std::string("<core>"));
        if (auto it = o_.libraries.find(name); it != o_.libraries.end()) return std::make_pair(it->second, name + ".qs");
        if (o_.resolver) if (auto text = o_.resolver(name)) return std::make_pair(*text, name + ".qs");
        std::string relative = name;
        for (std::size_t p = relative.find("::"); p != std::string::npos; p = relative.find("::", p)) relative.replace(p, 2, "/");
        relative += ".qs";
        for (const auto& dir : o_.library_paths) {
            const std::string path = dir.empty() ? relative : dir + (dir.back() == '/' ? "" : "/") + relative;
            std::ifstream in(path, std::ios::binary);
            if (!in) continue;
            std::ostringstream text; text << in.rdbuf();
            if (text.str().size() > o_.toolchain.il.max_source_bytes) continue;
            return std::make_pair(text.str(), path);
        }
        if (o_.stdlib) if (auto text = stdlib_source(name)) return std::make_pair(*text, "stdlib/" + relative);
        return std::nullopt;
    }
    void load(const std::string& name, const dsl::ASTNode* at) {
        if (a_.units.count(name)) return;
        if (!valid_library_name(name)) { error("invalid library name: " + name, at); return; }
        if (a_.libraries.size() >= o_.max_libraries) { error("library count limit exceeded", at); return; }
        auto found = find_library(name);
        if (!found) { error("unknown library: " + name, at); return; }
        auto parsed = parse(found->first, o_, found->second);
        if (!parsed) { a_.diagnostics.insert(a_.diagnostics.end(), parsed.diagnostics.begin(), parsed.diagnostics.end()); return; }
        a_.libraries.push_back(std::move(*parsed));
        if (name != core_unit) a_.library_order.push_back(name);
        a_.units[name] = UnitInfo{name, found->second, {}, {}, {}};
        items(name, a_.libraries.back().root);
    }
    void items(const std::string& unit, const dsl::ASTNode& root) {
        auto prefix = unit.empty() ? std::string() : unit + "::";
        for (const auto& c : root.children()) {
            const auto tag = c.tag();
            if (c.is_leaf()) { error("expected statement AST", &c); continue; }
            if (tag == "use") {
                for (std::size_t i = 0; i < arity(c); ++i) {
                    const auto& n = c.child(i);
                    if (!n.is_leaf() || n.tag() != "name") { error("invalid use AST", &c); continue; }
                    const auto& lib = n.value();
                    if (lib == unit) { error("library " + unit + " cannot use itself", &c); continue; }
                    auto& imports = a_.units[unit].imports;
                    if (std::find(imports.begin(), imports.end(), lib) == imports.end()) imports.push_back(lib);
                    load(lib, &c);
                }
            } else if (tag == "fn") {
                if (arity(c) != 3 || !c.child(0).is_leaf() || c.child(0).tag() != "name" || c.child(1).tag() != "params" || c.child(1).is_leaf() || c.child(2).tag() != "block") {
                    error("invalid fn AST", &c); continue;
                }
                const auto& short_name = c.child(0).value();
                if (!metachain::text::identifier(short_name) || short_name.find('.') != std::string::npos || short_name.find('$') != std::string::npos) { error("invalid function name: " + short_name, &c); continue; }
                Function f; f.name = prefix + short_name; f.unit = unit; f.decl = &c; f.body = &c.child(2);
                std::set<std::string> seen;
                for (const auto& p : c.child(1).children()) {
                    if (!p.is_leaf() || p.tag() != "name") { error("invalid parameter AST", &c); continue; }
                    if (!seen.insert(p.value()).second) error("duplicate parameter: " + p.value(), &c);
                    f.params.push_back(p.value());
                }
                if (f.params.size() > max_variables) error("function " + short_name + " has more than 16 parameters", &c);
                auto& u = a_.units[unit];
                if (u.functions.count(short_name)) { error("duplicate function: " + short_name, &c); continue; }
                if (u.constants.count(short_name)) { error(short_name + " is already a constant", &c); continue; }
                u.functions[short_name] = f.name;
                a_.functions[f.name] = std::move(f);
            } else if (tag == "const") {
                if (arity(c) != 2 || !c.child(0).is_leaf() || c.child(0).tag() != "name") { error("invalid const AST", &c); continue; }
                const auto& short_name = c.child(0).value();
                auto& u = a_.units[unit];
                if (u.constants.count(short_name)) { error("duplicate constant: " + short_name, &c); continue; }
                if (u.functions.count(short_name)) { error(short_name + " is already a function", &c); continue; }
                u.constants[short_name] = prefix + short_name;
                a_.constants[prefix + short_name] = Constant{prefix + short_name, unit, &c, std::nullopt, false};
            } else if (unit.empty()) {
                a_.statements.push_back(&c);
            } else {
                error("library " + unit + ": only fn, const and use are allowed at top level", &c);
            }
        }
    }
    void scan_power(const dsl::ASTNode& root) {
        std::vector<const dsl::ASTNode*> work{&root};
        while (!work.empty() && !a_.uses_power) {
            const auto* n = work.back(); work.pop_back();
            if (n->tag() == "**") a_.uses_power = true;
            if (!n->is_leaf()) for (const auto& c : n->children()) work.push_back(&c);
        }
    }

    // ---- constants ----------------------------------------------------
    std::optional<std::int64_t> power(std::int64_t base, std::int64_t exp) {
        if (exp < 0) return base == 1 ? 1 : base == -1 ? ((exp & 1) ? -1 : 1) : 0;
        std::uint64_t result = 1, b = static_cast<std::uint64_t>(base);
        while (exp > 0) { if (exp & 1) result *= b; exp >>= 1; if (exp > 0) b *= b; }
        return static_cast<std::int64_t>(result);
    }
    std::optional<std::int64_t> evaluate(const std::string& unit, const dsl::ASTNode& n, const dsl::ASTNode* at, std::size_t depth = 0) {
        if (depth > 256) { error("constant expression too deep", at); return std::nullopt; }
        const auto tag = n.tag();
        if (n.is_leaf() && tag == "integer") {
            auto v = metachain::text::signed_integer(n.value());
            if (!v) error("invalid integer in constant", at);
            return v;
        }
        if (n.is_leaf() && tag == "name") {
            std::string why;
            auto q = a_.constant(unit, n.value(), why);
            if (q.empty()) { error(why.empty() ? "undefined constant: " + n.value() : why, at); return std::nullopt; }
            return constant_value(q, at);
        }
        if (tag == "call") { error("function calls are not allowed in constant expressions", at); return std::nullopt; }
        if (unary_op(tag) && arity(n) == 1) {
            auto v = evaluate(unit, n.child(0), at, depth + 1);
            if (!v) return v;
            if (tag == "neg") return static_cast<std::int64_t>(0 - static_cast<std::uint64_t>(*v));
            if (tag == "not") return *v == 0 ? 1 : 0;
            return ~*v;
        }
        if (tag == "?:" && arity(n) == 3) {
            auto c = evaluate(unit, n.child(0), at, depth + 1);
            if (!c) return c;
            return evaluate(unit, n.child(*c ? 1 : 2), at, depth + 1);
        }
        if (binary_ops.count(tag) && arity(n) == 2) {
            auto x = evaluate(unit, n.child(0), at, depth + 1);
            if (!x) return x;
            if (tag == "&&" && !*x) return 0;
            if (tag == "||" && *x) return 1;
            auto y = evaluate(unit, n.child(1), at, depth + 1);
            if (!y) return y;
            if (tag == "&&" || tag == "||") return *y ? 1 : 0;
            if (tag == "**") return power(*x, *y);
            static const std::map<std::string_view, metail::Op> ops{{"+", metail::Op::Add}, {"-", metail::Op::Sub}, {"*", metail::Op::Mul}, {"/", metail::Op::Div}, {"%", metail::Op::Mod},
                {"&", metail::Op::And}, {"|", metail::Op::Or}, {"^", metail::Op::Xor}, {"<<", metail::Op::Shl}, {">>", metail::Op::Shr}, {"==", metail::Op::Eq}, {"<", metail::Op::Lt}};
            std::optional<std::int64_t> r;
            if (tag == "!=") r = *x != *y;
            else if (tag == ">") r = *x > *y;
            else if (tag == "<=") r = *x <= *y;
            else if (tag == ">=") r = *x >= *y;
            else r = metail::evaluate(ops.at(tag), *x, *y);
            if (!r) error("invalid division, remainder, or shift in constant expression", at);
            return r;
        }
        error("invalid constant expression", at);
        return std::nullopt;
    }
    std::optional<std::int64_t> constant_value(const std::string& q, const dsl::ASTNode* at) {
        auto& c = a_.constants.at(q);
        if (c.value) return c.value;
        if (c.evaluating) { error("constant " + q + " depends on itself", at); return std::nullopt; }
        c.evaluating = true;
        c.value = evaluate(c.unit, c.decl->child(1), c.decl);
        c.evaluating = false;
        return c.value;
    }

    // ---- bodies -------------------------------------------------------
    struct Body {
        std::string unit;
        std::vector<std::set<std::string>> scopes;
        std::size_t live = 0, loops = 0;
        Location where;
        std::set<std::string>* calls = nullptr;
    };
    bool defined(const Body& b, const std::string& n) const {
        for (auto it = b.scopes.rbegin(); it != b.scopes.rend(); ++it) if (it->count(n)) return true;
        return false;
    }
    void expr(Body& b, const dsl::ASTNode& n, std::size_t d) {
        if (d > std::min<std::size_t>(o_.max_depth, 256)) { error("AST resource limit exceeded", nullptr, &b.where); return; }
        const auto tag = n.tag();
        if (n.is_leaf() && tag == "integer") {
            if (!metachain::text::signed_integer(n.value())) error("invalid integer literal", nullptr, &b.where);
            return;
        }
        if (n.is_leaf() && tag == "name") {
            if (defined(b, n.value())) return;
            std::string why;
            if (a_.constant(b.unit, n.value(), why).empty()) error(why.empty() ? "undefined variable: " + n.value() : why, nullptr, &b.where);
            return;
        }
        if (tag == "call" && !n.is_leaf() && arity(n) >= 1 && n.child(0).is_leaf() && n.child(0).tag() == "name") {
            Location saved = b.where;
            if (auto l = location(n); l.line) b.where = l;
            std::string why;
            auto q = a_.function(b.unit, n.child(0).value(), why);
            if (q.empty()) error(why, nullptr, &b.where);
            else {
                const auto& f = a_.functions.at(q);
                if (f.params.size() != arity(n) - 1)
                    error("function " + n.child(0).value() + " expects " + std::to_string(f.params.size()) + " argument" + (f.params.size() == 1 ? "" : "s") + ", got " + std::to_string(arity(n) - 1), nullptr, &b.where);
                if (b.calls) b.calls->insert(q);
            }
            for (std::size_t i = 1; i < arity(n); ++i) expr(b, n.child(i), d + 1);
            b.where = saved;
            return;
        }
        const bool unary = unary_op(tag);
        if (n.is_leaf() || (!unary && !binary_ops.count(tag) && tag != "?:") || arity(n) != (unary ? 1u : tag == "?:" ? 3u : 2u)) {
            error("invalid expression AST", nullptr, &b.where); return;
        }
        if (tag == "**" && b.calls) b.calls->insert(std::string(core_unit) + "::pow");
        for (std::size_t i = 0; i < arity(n); ++i) expr(b, n.child(i), d + 1);
    }
    void scoped(Body& b, const dsl::ASTNode& n, std::size_t d) {
        b.scopes.emplace_back();
        stmt(b, n, d);
        b.live -= b.scopes.back().size(); b.scopes.pop_back();
    }
    void stmt(Body& b, const dsl::ASTNode& n, std::size_t d) {
        if (d > std::min<std::size_t>(o_.max_depth, 256)) { error("AST resource limit exceeded", nullptr, &b.where); return; }
        if (n.is_leaf()) { error("expected statement AST", nullptr, &b.where); return; }
        if (auto l = location(n); l.line) b.where = l;
        const auto tag = n.tag();
        const auto k = arity(n);
        if (tag == "program" || tag == "block") {
            b.scopes.emplace_back();
            for (std::size_t i = 0; i < k; ++i) stmt(b, n.child(i), d + 1);
            b.live -= b.scopes.back().size(); b.scopes.pop_back();
            return;
        }
        if ((tag == "let" || tag == "assign") && k == 2 && n.child(0).is_leaf() && n.child(0).tag() == "name") {
            const auto& name = n.child(0).value();
            expr(b, n.child(1), d + 1);
            if (!metachain::text::identifier(name)) { error("invalid variable name", nullptr, &b.where); return; }
            if (tag == "let") {
                if (a_.units[b.unit].constants.count(name)) error(name + " is already a constant", nullptr, &b.where);
                else if (!b.scopes.back().insert(name).second) error("duplicate variable: " + name, nullptr, &b.where);
                else if (++b.live > max_variables) error("QScript supports at most 16 simultaneously live variables", nullptr, &b.where);
            } else if (!defined(b, name)) {
                std::string why;
                if (!a_.constant(b.unit, name, why).empty()) error("cannot assign to constant: " + name, nullptr, &b.where);
                else error("assignment to undefined variable: " + name, nullptr, &b.where);
            }
            return;
        }
        if ((tag == "return" || tag == "exit" || tag == "assert" || tag == "expr") && k == 1) { expr(b, n.child(0), d + 1); return; }
        if (tag == "print" && k >= 1) { for (std::size_t i = 0; i < k; ++i) expr(b, n.child(i), d + 1); return; }
        if ((tag == "break" || tag == "continue") && k == 0) {
            if (!b.loops) error(std::string(tag) + " outside a loop", nullptr, &b.where);
            return;
        }
        if ((tag == "while" && k == 2) || (tag == "if" && (k == 2 || k == 3))) {
            expr(b, n.child(0), d + 1);
            if (tag == "while") ++b.loops;
            for (std::size_t i = 1; i < k; ++i) scoped(b, n.child(i), d + 1);
            if (tag == "while") --b.loops;
            return;
        }
        if (tag == "do" && k == 2) {
            ++b.loops; scoped(b, n.child(0), d + 1); --b.loops;
            expr(b, n.child(1), d + 1);
            return;
        }
        if (tag == "for" && k == 4) {
            b.scopes.emplace_back();
            stmt(b, n.child(0), d + 1);
            expr(b, n.child(1), d + 1);
            ++b.loops;
            scoped(b, n.child(3), d + 1);
            --b.loops;
            stmt(b, n.child(2), d + 1);
            b.live -= b.scopes.back().size(); b.scopes.pop_back();
            return;
        }
        error("invalid statement AST", nullptr, &b.where);
    }
public:
    Analyzer(const Options& o, Analysis& a) : o_(o), a_(a) {}
    void run(const AST& ast) {
        if (ast.root.tag() != "program" || ast.root.is_leaf()) { error("expected program AST", nullptr); return; }
        a_.units[""] = UnitInfo{};
        items("", ast.root);
        // `**` lowers to a call of the hidden core library.
        for (const auto* s : a_.statements) scan_power(*s);
        for (const auto& [q, f] : a_.functions) scan_power(*f.body);
        if (a_.uses_power) load(core_unit, nullptr);
        for (auto& [u, info] : a_.units) if (!u.empty() && u != core_unit && a_.uses_power) info.imports.push_back(core_unit);
        if (a_.uses_power) a_.units[""].imports.push_back(core_unit);
        for (auto& [q, c] : a_.constants) constant_value(q, c.decl);
        std::map<std::string, std::set<std::string>> graph;
        {
            Body b; b.calls = &graph[""];
            b.scopes.emplace_back();
            for (const auto* s : a_.statements) stmt(b, *s, 1);
        }
        for (auto& [q, f] : a_.functions) {
            Body b; b.unit = f.unit; b.calls = &graph[q];
            b.where = location(*f.decl);
            b.scopes.emplace_back(f.params.begin(), f.params.end());
            b.live = f.params.size();
            stmt(b, *f.body, 1);
        }
        // Reachability: the program, plus every function the program defines
        // (callable from the host), plus everything they call.
        std::vector<std::string> work{""};
        for (const auto& [q, f] : a_.functions) if (f.unit.empty()) work.push_back(q);
        std::set<std::string> seen;
        while (!work.empty()) {
            auto q = work.back(); work.pop_back();
            if (!seen.insert(q).second) continue;
            if (!q.empty()) a_.used_functions.insert(q);
            for (const auto& callee : graph[q]) work.push_back(callee);
        }
    }
};
} // namespace

std::string Analysis::function(const std::string& unit, const std::string& name, std::string& why) const {
    auto u = units.find(unit);
    if (u == units.end()) { why = "internal: unknown unit"; return {}; }
    const auto split = name.rfind("::");
    if (split != std::string::npos) {
        const auto lib = name.substr(0, split), short_name = name.substr(split + 2);
        if (lib != unit && std::find(u->second.imports.begin(), u->second.imports.end(), lib) == u->second.imports.end()) {
            why = "library " + lib + " is not imported here (add: use " + lib + ";)"; return {};
        }
        auto target = units.find(lib);
        if (target == units.end()) { why = "unknown library: " + lib; return {}; }
        auto f = target->second.functions.find(short_name);
        if (f == target->second.functions.end() || (short_name[0] == '_' && lib != unit)) { why = "undefined function: " + name; return {}; }
        return f->second;
    }
    if (auto f = u->second.functions.find(name); f != u->second.functions.end()) return f->second;
    std::vector<std::string> found;
    if (!name.empty() && name[0] != '_')
        for (const auto& lib : u->second.imports) {
            auto target = units.find(lib);
            if (target == units.end() || lib == core_unit) continue;
            if (auto f = target->second.functions.find(name); f != target->second.functions.end()) found.push_back(f->second);
        }
    if (found.size() == 1) return found[0];
    if (found.empty()) { why = "undefined function: " + name; return {}; }
    why = "ambiguous function " + name + ":";
    for (const auto& f : found) why += " " + f;
    why += " (qualify the call)";
    return {};
}
std::string Analysis::constant(const std::string& unit, const std::string& name, std::string& why) const {
    auto u = units.find(unit);
    if (u == units.end()) { why = "internal: unknown unit"; return {}; }
    const auto split = name.rfind("::");
    if (split != std::string::npos) {
        const auto lib = name.substr(0, split), short_name = name.substr(split + 2);
        if (lib != unit && std::find(u->second.imports.begin(), u->second.imports.end(), lib) == u->second.imports.end()) {
            why = "library " + lib + " is not imported here (add: use " + lib + ";)"; return {};
        }
        auto target = units.find(lib);
        if (target == units.end()) { why = "unknown library: " + lib; return {}; }
        auto c = target->second.constants.find(short_name);
        if (c == target->second.constants.end() || (short_name[0] == '_' && lib != unit)) { why = "undefined constant: " + name; return {}; }
        return c->second;
    }
    if (auto c = u->second.constants.find(name); c != u->second.constants.end()) return c->second;
    std::vector<std::string> found;
    if (!name.empty() && name[0] != '_')
        for (const auto& lib : u->second.imports) {
            auto target = units.find(lib);
            if (target == units.end()) continue;
            if (auto c = target->second.constants.find(name); c != target->second.constants.end()) found.push_back(c->second);
        }
    if (found.size() == 1) return found[0];
    if (found.size() > 1) {
        // Identical constants from several libraries (INT_MAX, say) agree.
        bool same = true;
        for (const auto& q : found) {
            auto a = constants.find(q), b = constants.find(found[0]);
            if (a == constants.end() || b == constants.end() || !a->second.value || a->second.value != b->second.value) same = false;
        }
        if (same) return found[0];
        why = "ambiguous name " + name + ":";
        for (const auto& c : found) why += " " + c;
        why += " (qualify it)";
    }
    return {};
}
Analysis analyze(const AST& ast, const Options& o) {
    Analysis a;
    try {
        Analyzer(o, a).run(ast);
    } catch (const std::exception& e) {
        a.diagnostics.push_back({metachain::Stage::Verify, std::string("internal analysis failure: ") + e.what(), {}, 0, 0, 0});
    }
    return a;
}
} // namespace detail

std::vector<Diagnostic> sema_check(const AST& ast, Options o) {
    return detail::analyze(ast, o).diagnostics;
}
} // namespace qscript
