#include "qscript/QScript.h"
#include "qscript/src/Internal.h"

namespace qscript {
Result<Unit> finish(Unit unit, Options o) {
    // Optimization rewrites instructions in place and scheduling only
    // reorders inside basic blocks, so function entry points stay valid.
    auto p = std::move(unit.program);
    if (o.toolchain.optimize) {
        auto optimized = optimize(std::move(p), o);
        if (!optimized) return {std::nullopt, std::move(optimized.diagnostics)};
        p = std::move(*optimized);
    }
    auto scheduled = schedule(std::move(p), o);
    if (!scheduled) return {std::nullopt, std::move(scheduled.diagnostics)};
    unit.program = std::move(*scheduled);
    return {std::move(unit), {}};
}
Result<Unit> compile_unit(std::string_view source, Options o, std::string file) {
    auto ast = parse(source, o, file);
    if (!ast) return {std::nullopt, std::move(ast.diagnostics)};
    auto unit = lower(*ast, o);
    if (!unit) return unit;
    unit->program.name = std::move(file);
    return finish(std::move(*unit), o);
}
Result<metail::Program> compile(std::string_view source, Options o, std::string file) {
    auto unit = compile_unit(source, std::move(o), std::move(file));
    if (!unit) return {std::nullopt, std::move(unit.diagnostics)};
    return {std::move(unit->program), {}};
}
Result<std::vector<std::uint8_t>> build(const metail::Program& p, Options o) {
    auto assembly = select(p, o); if (!assembly) return {std::nullopt, std::move(assembly.diagnostics)};
    auto object = assemble(*assembly, true, o); if (!object) return object;
    return link({*object}, o);
}

namespace {
// QScript's assert lowers to `const r31, 0; div r31, r31, r31`.
bool assert_trap(const metail::Program& p, std::size_t pc) {
    if (pc == 0 || pc >= p.size()) return false;
    const auto& d = p.code[pc];
    const auto& c = p.code[pc - 1];
    return d.op == metail::Op::Div && d.a == 31 && d.b == 31 && d.c == 31 && c.op == metail::Op::Const && c.a == 31 && c.immediate == 0;
}
void translate(const metail::Program& p, Result<metail::Execution>& out) {
    for (auto& d : out.diagnostics) {
        if (d.stage == metachain::Stage::Execute && d.message == "invalid division, remainder, or shift" && assert_trap(p, d.offset))
            d.message = "assertion failed";
    }
}
} // namespace
Result<metail::Execution> run(const metail::Program& p, Options o) {
    auto executed = metachain::Toolchain(o.toolchain).run(p);
    translate(p, executed);
    return executed;
}
Result<metail::Execution> call(const Unit& unit, std::string_view function, const std::vector<std::int64_t>& args, Options o) {
    Result<metail::Execution> out;
    const FunctionInfo* f = nullptr;
    for (const auto& candidate : unit.functions) if (candidate.name == function) f = &candidate;
    if (!f) { out.diagnostics.push_back({metachain::Stage::Execute, "no such function: " + std::string(function), {}, 0, 0, 0}); return out; }
    if (args.size() != f->arity) {
        out.diagnostics.push_back({metachain::Stage::Execute, "function " + f->name + " expects " + std::to_string(f->arity) + " arguments", {}, 0, 0, 0});
        return out;
    }
    // Relocate by one instruction and enter through a stub:
    //   jmp stub; <program>; stub: const/push args; call f; halt r30
    metail::Program p;
    p.name = unit.program.name;
    p.code.reserve(unit.program.size() + 2 * args.size() + 3);
    p.code.push_back({metail::Op::Jmp, 0, 0, 0, 0});
    for (auto i : unit.program.code) {
        if (metail::branch(i.op)) ++i.immediate;
        p.code.push_back(i);
    }
    p.code[0].immediate = static_cast<std::int64_t>(p.code.size());
    for (auto v : args) {
        p.code.push_back({metail::Op::Const, 16, 0, 0, v});
        p.code.push_back({metail::Op::Push, 16, 0, 0, 0});
    }
    p.code.push_back({metail::Op::Call, 0, 0, 0, static_cast<std::int64_t>(f->entry + 1)});
    p.code.push_back({metail::Op::Halt, 30, 0, 0, 0});
    if (!unit.program.source_map.empty()) {
        p.source_map.push_back({});
        p.source_map.insert(p.source_map.end(), unit.program.source_map.begin(), unit.program.source_map.end());
        p.source_map.resize(p.code.size());
    }
    out = run(p, o);
    for (auto& d : out.diagnostics) if (d.offset) --d.offset;
    return out;
}

const char* grammar_source() { return detail::embedded_grammar(); }
std::vector<std::string> stdlib_names() {
    std::size_t count = 0;
    const auto* libs = detail::embedded_libraries(&count);
    std::vector<std::string> r;
    for (std::size_t i = 0; i < count; ++i) r.emplace_back(libs[i].name);
    return r;
}
std::optional<std::string> stdlib_source(std::string_view name) {
    std::size_t count = 0;
    const auto* libs = detail::embedded_libraries(&count);
    for (std::size_t i = 0; i < count; ++i) if (name == libs[i].name) return std::string(libs[i].source);
    return std::nullopt;
}
} // namespace qscript
