// QScript declaration analysis: `use` resolution, functions and their types,
// constants, grammars, and name lookup. Bodies are checked and typed by the
// lowering (AST2IR.cpp), which consumes the same Analysis.
#include "qscript/QScript.h"
#include "qscript/include/QScript-Grammar.h"
#include "qscript/src/Analysis.h"
#include "qscript/src/Internal.h"
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

// Identifier used for a grammar inside grammar sources ("geom::Calc" -> "geom__Calc").
std::string internal_grammar_name(std::string name) {
    std::string out;
    for (std::size_t i = 0; i < name.size(); ++i) {
        if (name[i] == ':' && i + 1 < name.size() && name[i + 1] == ':') { out += "__"; ++i; }
        else out += name[i];
    }
    return out;
}

// Return-path analysis for functions whose result is not an integer. Falling off
// their end traps at run time, so the check rejects a body that can reach its end
// on some path. It is conservative: a statement whose exit cannot be shown does not
// terminate, which can reject a program that would never fall through, but never
// accepts one that can.
bool is_constant_true(const dsl::ASTNode& n) {
    if (!n.is_leaf() || n.tag() != "integer") return false;
    const std::string_view v = n.value();
    return !v.empty() && v.find_first_not_of("0123456789") == std::string_view::npos
        && v.find_first_not_of('0') != std::string_view::npos;
}

// Whether a loop body has a break that leaves that loop, or a continue when
// `continues_leave` (a do loop tests its condition after continue; a while or for
// loop with a constant-true test goes straight back into its body). Nested loops
// keep their own break and continue statements.
bool leaves_loop(const dsl::ASTNode& n, bool continues_leave) {
    const std::string_view tag = n.tag();
    if (tag == "break") return true;
    if (tag == "continue") return continues_leave;
    if (tag == "while" || tag == "do" || tag == "for" || tag == "foreach") return false;
    if (n.is_leaf()) return false;
    for (std::size_t i = 0; i < detail::arity(n); ++i) if (leaves_loop(n.child(i), continues_leave)) return true;
    return false;
}

// Whether control can never reach the end of statement `n`. `die_builtin` is true
// when `die(...)` names the built-in, which always traps.
bool terminates(const dsl::ASTNode& n, bool die_builtin) {
    const std::string_view tag = n.tag();
    if (tag == "return" || tag == "exit") return true;
    if (n.is_leaf()) return false;
    if (tag == "block") {
        for (std::size_t i = 0; i < detail::arity(n); ++i) if (terminates(n.child(i), die_builtin)) return true;
        return false;
    }
    if (tag == "if") return detail::arity(n) == 3 && terminates(n.child(1), die_builtin) && terminates(n.child(2), die_builtin);
    if (tag == "while") return is_constant_true(n.child(0)) && !leaves_loop(n.child(1), false);
    if (tag == "for") return is_constant_true(n.child(1)) && !leaves_loop(n.child(3), false);
    if (tag == "do") return terminates(n.child(0), die_builtin) && !leaves_loop(n.child(0), true);
    if (tag == "expr" && die_builtin) {
        const auto& e = n.child(0);
        return !e.is_leaf() && e.tag() == "call" && detail::arity(e) >= 1 && e.child(0).value() == "die";
    }
    return false;
}

class Analyzer {
    const Options& o_;
    Analysis& a_;
    std::size_t literals_ = 0;

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
            auto text=read_bounded(in,o_.toolchain.il.max_source_bytes);
            if (!text) throw std::runtime_error("library read failed or source size limit exceeded: " + path);
            return std::make_pair(std::move(*text), path);
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
        a_.units[name] = UnitInfo{name, found->second, {}, {}, {}, {}};
        items(name, a_.libraries.back().root);
    }
    // A parameter or result type annotation. Unknown spellings are reported.
    Type type_of(const dsl::ASTNode& n, const dsl::ASTNode* at) {
        if (n.is_leaf() && n.tag() == "type") {
            if (n.value().empty()) return Type::integer();
            if (auto t = parse_type(n.value())) return *t;
        }
        error("unknown type: " + (n.is_leaf() ? n.value() : std::string("?")), at);
        return Type::integer();
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
                if (arity(c) != 4 || !c.child(0).is_leaf() || c.child(0).tag() != "name" || c.child(1).tag() != "params" ||
                    c.child(1).is_leaf() || c.child(2).tag() != "type" || c.child(3).tag() != "block") {
                    error("invalid fn AST", &c); continue;
                }
                const auto& short_name = c.child(0).value();
                if (!metachain::text::identifier(short_name) || short_name.find('.') != std::string::npos || short_name.find('$') != std::string::npos) { error("invalid function name: " + short_name, &c); continue; }
                Function f; f.name = prefix + short_name; f.unit = unit; f.decl = &c; f.body = &c.child(3);
                f.result = type_of(c.child(2), &c);
                if (auto existing=a_.functions.find(f.name); existing!=a_.functions.end() && existing->second.host_id) {
                    error("function conflicts with host binding: " + f.name, &c); continue;
                }
                std::set<std::string> seen;
                for (const auto& p : c.child(1).children()) {
                    if (p.is_leaf() || p.tag() != "param" || p.size() != 2 || !p.child(0).is_leaf()) { error("invalid parameter AST", &c); continue; }
                    const auto& pname = p.child(0).value();
                    if (!seen.insert(pname).second) error("duplicate parameter: " + pname, &c);
                    f.params.push_back(pname);
                    f.param_types.push_back(type_of(p.child(1), &c));
                }
                if (f.params.size() > max_variables) error("function " + short_name + " has more than 16 parameters", &c);
                auto& u = a_.units[unit];
                if (u.functions.count(short_name)) { error("duplicate function: " + short_name, &c); continue; }
                if (u.constants.count(short_name)) { error(short_name + " is already a constant", &c); continue; }
                if (u.grammars.count(short_name)) { error(short_name + " is already a grammar", &c); continue; }
                u.functions[short_name] = f.name;
                a_.functions[f.name] = std::move(f);
                a_.emitted.push_back(prefix + short_name);
            } else if (tag == "const") {
                if (arity(c) != 2 || !c.child(0).is_leaf() || c.child(0).tag() != "name") { error("invalid const AST", &c); continue; }
                const auto& short_name = c.child(0).value();
                auto& u = a_.units[unit];
                if (u.constants.count(short_name)) { error("duplicate constant: " + short_name, &c); continue; }
                if (u.functions.count(short_name)) { error(short_name + " is already a function", &c); continue; }
                if (u.grammars.count(short_name)) { error(short_name + " is already a grammar", &c); continue; }
                u.constants[short_name] = prefix + short_name;
                a_.constants[prefix + short_name] = Constant{prefix + short_name, unit, &c, std::nullopt, false};
            } else if (tag == "grammar") {
                if (arity(c) != 3 || !c.child(0).is_leaf() || !c.child(1).is_leaf() || !c.child(2).is_leaf()) { error("invalid grammar AST", &c); continue; }
                const auto& short_name = c.child(0).value();
                auto& u = a_.units[unit];
                if (u.grammars.count(short_name)) { error("duplicate grammar: " + short_name, &c); continue; }
                if (u.functions.count(short_name)) { error(short_name + " is already a function", &c); continue; }
                if (u.constants.count(short_name)) { error(short_name + " is already a constant", &c); continue; }
                GrammarDecl g;
                g.name = prefix + short_name;
                g.internal = internal_grammar_name(g.name);
                g.unit = unit; g.decl = &c;
                g.parent = c.child(1).value();
                g.body = c.child(2).value();
                const auto key = g.name;
                u.grammars[short_name] = key;
                a_.grammars[key] = std::move(g);
            } else if (unit.empty()) {
                a_.statements.push_back(&c);
            } else {
                error("library " + unit + ": only fn, const, grammar and use are allowed at top level", &c);
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

    // ---- grammars -----------------------------------------------------
    // Build the complete source of a grammar and compile it, once. Parents
    // are resolved first; a cycle or an unknown parent is an error.
    void resolve_grammar(const std::string& key) {
        auto it = a_.grammars.find(key);
        if (it == a_.grammars.end()) return;
        GrammarDecl& g = it->second;
        if (g.state == 2) return;
        const dsl::ASTNode* at = g.decl;
        if (g.state == 1) { error("grammar " + g.name + " inherits from itself", at); g.state = 2; return; }
        g.state = 1;
        std::string source;
        std::string parent_key;
        if (!g.parent.empty()) {
            std::string why;
            parent_key = a_.grammar(g.unit, g.parent, why);
            if (parent_key.empty()) {
                error(why.empty() ? "unknown parent grammar " + g.parent : why, at);
                g.state = 2;
                return;
            }
            resolve_grammar(parent_key);
            const auto& parent = a_.grammars.at(parent_key);
            if (parent.state != 2 || !parent.compiled) { g.state = 2; return; }
            source = parent.source + "\n";
        }
        source += "grammar " + g.internal;
        if (!parent_key.empty()) source += " is " + a_.grammars.at(parent_key).internal;
        source += " {" + g.body + "}";
        g.source = source;
        auto compiled = qs_grammar::compile(source);
        if (!compiled.ok()) {
            error("invalid grammar " + g.name + ": " + compiled.error, at);
            g.state = 2;
            return;
        }
        g.compiled = std::make_shared<const qs_grammar>(std::move(compiled));
        g.state = 2;
    }
    // Anonymous grammar literals: found anywhere in the program or libraries.
    void collect_literals(const dsl::ASTNode& root, const std::string& unit) {
        std::vector<const dsl::ASTNode*> work{&root};
        while (!work.empty()) {
            const auto* n = work.back(); work.pop_back();
            if (n->is_leaf()) continue;
            if (n->tag() == "grammar-lit" && n->size() >= 1 && n->child(0).is_leaf()) {
                const std::string key = "#" + std::to_string(++literals_);
                GrammarDecl g;
                g.unit = unit; g.decl = n; g.body = n->child(0).value();
                g.internal = "__qs_anon" + std::to_string(literals_);
                a_.grammars[key] = std::move(g);
                a_.grammar_literals[n] = key;
            }
            for (const auto& c : n->children()) work.push_back(&c);
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
        static const std::set<std::string_view> binary_ops{"+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>", "==", "!=", "<", ">", "<=", ">=", "&&", "||", "**", "~"};
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
            if (tag == "~") { error("string concatenation is not allowed in constant expressions", at); return std::nullopt; }
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

public:
    Analyzer(const Options& o, Analysis& a) : o_(o), a_(a) {}
    void run(const AST& ast) {
        if (ast.root.tag() != "program" || ast.root.is_leaf()) { error("expected program AST", nullptr); return; }
        a_.units[""] = UnitInfo{};
        std::set<std::int64_t> ids;
        for (const auto& [name, host] : o_.hosts) {
            if (!valid_library_name(name) || host.arity > max_variables || host.id < 0 || !host.invoke || !ids.insert(host.id).second) {
                error("invalid host binding: " + name, nullptr); continue;
            }
            Function f; f.name = name; f.host_id = host.id; f.host_arity = host.arity;
            for (std::size_t n=0; n<host.arity; ++n) { f.params.push_back("arg" + std::to_string(n)); f.param_types.push_back(Type::integer()); }
            a_.functions[name] = std::move(f);
            a_.units[""].functions[name] = name;
        }
        items("", ast.root);
        collect_literals(ast.root, "");
        // `**` lowers to a call of the hidden core library.
        for (const auto* s : a_.statements) scan_power(*s);
        for (const auto& [q, f] : a_.functions) if (f.body) scan_power(*f.body);
        if (a_.uses_power) load(core_unit, nullptr);
        for (auto& [u, info] : a_.units) if (!u.empty() && u != core_unit && a_.uses_power) info.imports.push_back(core_unit);
        if (a_.uses_power) a_.units[""].imports.push_back(core_unit);
        for (auto& [q, c] : a_.constants) constant_value(q, c.decl);
        // Grammars: validate every declaration and literal.
        for (auto& [key, g] : a_.grammars) resolve_grammar(key);
        // Functions defined by libraries are compiled too, so a library's
        // errors are reported even when the program does not call them.
        for (auto& [q, f] : a_.functions) {
            if (f.host_id) continue;
            const bool die_builtin = !a_.functions.count("die");
            if (f.body && !f.result.is_int() && !terminates(*f.body, die_builtin))
                error("function " + q + " ended without returning a value on some path", f.decl);
            if (!std::count(a_.emitted.begin(), a_.emitted.end(), q)) a_.emitted.push_back(q);
        }
    }
};
} // namespace

std::string Analysis::function(const std::string& unit, const std::string& name, std::string& why) const {
    if (auto f = functions.find(name); f != functions.end() && f->second.host_id) return name;
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
std::string Analysis::grammar(const std::string& unit, const std::string& name, std::string& why) const {
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
        auto g = target->second.grammars.find(short_name);
        if (g == target->second.grammars.end() || (short_name[0] == '_' && lib != unit)) { why = "undefined grammar: " + name; return {}; }
        return g->second;
    }
    if (auto g = u->second.grammars.find(name); g != u->second.grammars.end()) return g->second;
    std::vector<std::string> found;
    if (!name.empty() && name[0] != '_')
        for (const auto& lib : u->second.imports) {
            auto target = units.find(lib);
            if (target == units.end()) continue;
            if (auto g = target->second.grammars.find(name); g != target->second.grammars.end()) found.push_back(g->second);
        }
    if (found.size() == 1) return found[0];
    if (found.empty()) { why = "undefined grammar: " + name; return {}; }
    why = "ambiguous grammar " + name + ":";
    for (const auto& g : found) why += " " + g;
    why += " (qualify it)";
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
} // namespace qscript
