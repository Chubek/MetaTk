#include "Internal.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>
#include <sstream>
#include <stdexcept>

namespace juniper {
Pattern Pattern::reg() { return {}; }
Pattern Pattern::imm(std::int64_t lo, std::int64_t hi) {
    Pattern p; p.kind = Kind::immediate; p.minimum = lo; p.maximum = hi; return p;
}
Pattern Pattern::operation(std::string op, std::vector<Pattern> args) {
    Pattern p; p.kind = Kind::operation; p.op = std::move(op); p.args = std::move(args); return p;
}
Backend Backend::portable() {
    Backend b;
    b.rules.push_back({"literal", Pattern::imm(), "CONST", 1, 1, "alu"});
    for (const auto* op : {"add", "sub", "mul", "div", "mod", "band", "bor", "bxor", "shl", "shr",
                           "eq", "ne", "lt", "le", "gt", "ge", "land", "lor", "neg", "bnot", "lnot"}) {
        std::string opcode(op);
        std::transform(opcode.begin(), opcode.end(), opcode.begin(),
            [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        b.rules.push_back({std::string("base_") + op,
            Pattern::operation(op, std::vector<Pattern>(static_cast<std::size_t>(metail_operator_arity(op)))),
            opcode, 2, static_cast<std::uint32_t>(std::string_view(op) == "mul" ? 3 : 1), "alu"});
    }
    return b;
}
std::string format(const Diagnostic& d) {
    std::ostringstream out;
    if (!d.file.empty()) {
        out << d.file;
        if (d.line) { out << ':' << d.line; if (d.column) out << ':' << d.column; }
        out << ": ";
    }
    return out.str() + d.code + ": " + d.message;
}

namespace detail {
bool identifier(std::string_view s) {
    auto alpha = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    if (s.empty() || !alpha(s.front())) return false;
    return std::all_of(s.begin(), s.end(), [&](char c) { return alpha(c) || (c >= '0' && c <= '9'); });
}
std::string quote(std::string_view s) {
    constexpr char hex[] = "0123456789abcdef";
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c < 0x20 || c >= 0x7f) {
            out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15];
        } else out += static_cast<char>(c);
    }
    return out + '"';
}

static bool valid_pattern(const Pattern& p, unsigned depth, std::size_t& count) {
    if (depth > 32 || ++count > 16384) return false;
    if (p.kind == Pattern::Kind::reg) return p.args.empty();
    if (p.kind == Pattern::Kind::immediate) return p.args.empty() && p.minimum <= p.maximum;
    if (p.kind != Pattern::Kind::operation || !identifier(p.op) || p.op == "select" ||
        metail_operator_arity(p.op.c_str()) != static_cast<int>(p.args.size())) return false;
    // Covering a nested trap with one instruction could postpone that trap past
    // the evaluation of a later captured call. Keep traps at cover roots.
    if (depth > 1 && (p.op == "div" || p.op == "mod" || p.op == "shl" || p.op == "shr")) return false;
    return std::all_of(p.args.begin(), p.args.end(), [&](const Pattern& a) { return valid_pattern(a, depth + 1, count); });
}
Result<bool> validate_backend(const Backend& b) {
    if (!identifier(b.name) || b.issue_width == 0 || b.issue_width > 64 || b.resources.size() > 64 || b.rules.size() > 1024)
        return error("JUN-E102", "invalid backend name, issue width (1..64), resource or rule limit");
    if (!b.resources.contains("alu") || !b.resources.contains("control"))
        return error("JUN-E102", "backend requires alu and control resources for structural instructions");
    for (const auto& [name, capacity] : b.resources)
        if (!identifier(name) || capacity == 0 || capacity > 64)
            return error("JUN-E102", "invalid resource '" + name + "': capacity must be 1..64");
    std::set<std::string> names;
    std::size_t nodes = 0;
    for (const auto& r : b.rules) {
        if (!identifier(r.name) || !identifier(r.opcode) || !names.insert(r.name).second ||
            r.pattern.kind == Pattern::Kind::reg || !valid_pattern(r.pattern, 1, nodes) ||
            !b.resources.contains(r.resource) || r.cost == 0 || r.cost > 1000000 || r.latency == 0 || r.latency > 1000000)
            return error("JUN-E102", "invalid or duplicate rule '" + r.name + "' (no lazy select or nested traps in patterns)");
        if (r.opcode == "COPY" || r.opcode == "CALL" || r.opcode == "RET" || r.opcode == "ASSUME" ||
            r.opcode == "LABEL" || r.opcode == "BR" || r.opcode == "JUMP")
            return error("JUN-E102", "structural opcode is reserved: " + r.opcode);
    }
    return true;
}
Result<bool> validate_structure(const Module& m) {
    auto backend = validate_backend(m.backend);
    if (!backend) return *backend.error();
    std::size_t items = 0, bytes = 0;
    auto text = [&](const std::string& s) {
        if (s.size() > 8 * 1024 * 1024 - bytes || s.find('\0') != std::string::npos) return false;
        bytes += s.size(); return true;
    };
    auto expression = [&](auto&& self, const Expr& e, unsigned depth) -> bool {
        if (++items > 200000 || depth > 256 || !text(e.name)) return false;
        if (e.kind != METAIL_INT && e.kind != METAIL_VAR && e.kind != METAIL_OP && e.kind != METAIL_CALL) return false;
        if ((e.kind == METAIL_INT || e.kind == METAIL_VAR) && !e.args.empty()) return false;
        if (e.args.size() > 200000 - items) return false;
        for (const auto& a : e.args) if (!self(self, a, depth + 1)) return false;
        return true;
    };
    bool ok = text(m.name) && m.functions.size() <= 200000;
    for (const auto& f : m.functions) {
        if (!ok || ++items > 200000 || !text(f.name) || f.params.size() > 200000 - items) { ok = false; break; }
        items += f.params.size();
        for (const auto& p : f.params) ok = ok && text(p);
        for (const auto& s : f.statements) {
            if (++items > 200000 || !text(s.name) || !text(s.target) || !text(s.yes) || !text(s.no)) { ok = false; break; }
            switch (s.kind) {
            case METAIL_LET: case METAIL_RETURN: case METAIL_EVAL: case METAIL_ASSUME: case METAIL_BRANCH:
                ok = ok && expression(expression, s.expr, 1); break;
            case METAIL_LABEL: case METAIL_GOTO: break;
            default: ok = false;
            }
            if (!ok) break;
        }
    }
    if (!ok) return error("JUN-E103", "invalid IR kind, embedded NUL, or IR budget exceeded (256 depth, 200000 items, 8 MiB strings)");
    return true;
}
static std::string pattern_text(const Pattern& p) {
    if (p.kind == Pattern::Kind::reg) return "reg";
    if (p.kind == Pattern::Kind::immediate)
        return "imm(" + std::to_string(p.minimum) + ", " + std::to_string(p.maximum) + ')';
    std::string out = p.op + '(';
    for (std::size_t i = 0; i < p.args.size(); ++i) { if (i) out += ", "; out += pattern_text(p.args[i]); }
    return out + ')';
}
std::string emit_backend(const Backend& b) {
    std::ostringstream out;
    out << "juniper 1;\nbackend " << b.name << " {\n  issue " << b.issue_width << ";\n";
    for (const auto& [name, n] : b.resources) out << "  resource " << name << ' ' << n << ";\n";
    for (const auto& r : b.rules)
        out << "  rule " << r.name << " : " << pattern_text(r.pattern) << " => " << r.opcode
            << " cost " << r.cost << " latency " << r.latency << " resource " << r.resource << ";\n";
    return out.str() + "}\n";
}
const metail_expr* Bridge::expression(const Expr& e) {
    expressions.emplace_back();
    auto& out = expressions.back();
    out.kind = e.kind; out.value = e.value;
    if (e.kind == METAIL_OP) out.op = e.name.c_str();
    else if (e.kind == METAIL_VAR || e.kind == METAIL_CALL) out.name = e.name.c_str();
    arguments.emplace_back();
    auto& args = arguments.back();
    for (const auto& a : e.args) args.push_back(expression(a));
    out.args = args.data(); out.arg_count = args.size();
    return &out;
}
Bridge::Bridge(const Module& m) : params(m.functions.size()), statements(m.functions.size()) {
    functions.reserve(m.functions.size());
    for (std::size_t i = 0; i < m.functions.size(); ++i) {
        const auto& f = m.functions[i];
        for (const auto& p : f.params) params[i].push_back(p.c_str());
        for (const auto& s : f.statements) {
            metail_stmt o{};
            o.kind = s.kind; o.name = s.name.c_str(); o.target = s.target.c_str(); o.yes = s.yes.c_str(); o.no = s.no.c_str();
            if (s.kind == METAIL_LET || s.kind == METAIL_RETURN || s.kind == METAIL_EVAL || s.kind == METAIL_ASSUME || s.kind == METAIL_BRANCH)
                o.expr = expression(s.expr);
            statements[i].push_back(o);
        }
        functions.push_back({f.name.c_str(), params[i].data(), params[i].size(), statements[i].data(), statements[i].size()});
    }
    module = {m.name.empty() ? nullptr : m.name.c_str(), functions.data(), functions.size()};
}
static Expr import_expression(const metail_expr& e) {
    Expr out; out.kind = e.kind; out.value = e.value;
    if (e.kind == METAIL_OP) out.name = e.op;
    else if (e.kind == METAIL_VAR || e.kind == METAIL_CALL) out.name = e.name;
    for (std::size_t i = 0; i < e.arg_count; ++i) out.args.push_back(import_expression(*e.args[i]));
    return out;
}
Module import_module(const metail_module& m, Backend b) {
    Module out; out.name = m.name ? m.name : ""; out.backend = std::move(b);
    for (std::size_t i = 0; i < m.function_count; ++i) {
        const auto& f = m.functions[i];
        Function fn; fn.name = f.name;
        for (std::size_t j = 0; j < f.param_count; ++j) fn.params.emplace_back(f.params[j]);
        for (std::size_t j = 0; j < f.statement_count; ++j) {
            const auto& s = f.statements[j];
            Statement stmt; stmt.kind = s.kind;
            stmt.name = s.name ? s.name : ""; stmt.target = s.target ? s.target : "";
            stmt.yes = s.yes ? s.yes : ""; stmt.no = s.no ? s.no : "";
            if (s.expr) stmt.expr = import_expression(*s.expr);
            fn.statements.push_back(std::move(stmt));
        }
        out.functions.push_back(std::move(fn));
    }
    return out;
}
} // namespace detail

void Compiler::ContextDeleter::operator()(metail_context* c) const noexcept { metail_destroy(c); }
Compiler::Compiler(const char* home) {
    char error[1024]{};
    std::string installed_home;
    if (!home && !std::getenv("METAIL_HOME")) {
        std::error_code ec;
        auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (!ec) {
            auto root = exe.parent_path() / "../share/metail";
            if (std::filesystem::is_regular_file(root / "lua/metail/cli.lua", ec) && !ec) {
                installed_home = root.string(); home = installed_home.c_str();
            }
        }
    }
    context_.reset(metail_create(home, error, sizeof(error)));
    if (!context_) throw std::runtime_error(error);
}
Result<Module> Compiler::parse(std::string_view source, const std::string& file) {
    if (!context_) return detail::error("JUN-E104", "moved-from compiler");
    auto extracted = detail::extract_backend(source, file);
    if (!extracted) return *extracted.error();
    metail_module* raw = nullptr;
    if (metail_parse(context_.get(), extracted.get()->second.c_str(), file.c_str(), &raw) != METAIL_OK)
        return detail::error("JUN-E100", metail_last_error(context_.get()), file);
    std::unique_ptr<metail_module, decltype(&metail_module_free)> owned(raw, metail_module_free);
    return detail::import_module(*raw, std::move(extracted.get()->first));
}
Result<Module> Compiler::load(const std::filesystem::path& path, const PreprocessOptions& options) {
    auto source = preprocess(path, options);
    if (!source) return *source.error();
    return parse(source.get()->text, path.string());
}
Result<bool> Compiler::check(const Module& m) {
    if (!context_) return detail::error("JUN-E104", "moved-from compiler");
    auto valid = detail::validate_structure(m);
    if (!valid) return *valid.error();
    detail::Bridge bridge(m);
    if (metail_module_check(context_.get(), &bridge.module) != METAIL_OK)
        return detail::error("JUN-E100", metail_last_error(context_.get()));
    return true;
}
Result<std::string> Compiler::emit_metail(const Module& m) {
    auto valid = check(m); if (!valid) return *valid.error();
    detail::Bridge bridge(m);
    char* raw = nullptr;
    if (metail_module_emit(context_.get(), &bridge.module, &raw) != METAIL_OK)
        return detail::error("JUN-E100", metail_last_error(context_.get()));
    std::unique_ptr<char, decltype(&metail_free)> owned(raw, metail_free);
    return std::string(raw);
}
Result<std::string> Compiler::emit(const Module& m) {
    auto body = emit_metail(m); if (!body) return *body.error();
    return detail::emit_backend(m.backend) + *body.get();
}
Result<Module> Compiler::optimize(const Module& m, const std::string& passes, int rounds) {
    auto valid = check(m); if (!valid) return *valid.error();
    if (passes.find('\0') != std::string::npos) return detail::error("JUN-E104", "NUL in pass list");
    detail::Bridge bridge(m);
    metail_module* raw = nullptr;
    if (metail_module_optimize(context_.get(), &bridge.module, passes.c_str(), rounds, &raw, nullptr) != METAIL_OK)
        return detail::error("JUN-E100", metail_last_error(context_.get()));
    std::unique_ptr<metail_module, decltype(&metail_module_free)> owned(raw, metail_module_free);
    return detail::import_module(*raw, m.backend);
}
Result<std::string> Compiler::analyze(const Module& m, const std::string& passes) {
    auto valid = check(m); if (!valid) return *valid.error();
    if (passes.find('\0') != std::string::npos) return detail::error("JUN-E104", "NUL in pass list");
    detail::Bridge bridge(m);
    char* raw = nullptr;
    if (metail_module_analyze(context_.get(), &bridge.module, passes.c_str(), &raw) != METAIL_OK)
        return detail::error("JUN-E100", metail_last_error(context_.get()));
    std::unique_ptr<char, decltype(&metail_free)> owned(raw, metail_free);
    return std::string(raw);
}
} // namespace juniper
