#include "qscript/QScript.h"
#include "qscript/src/Internal.h"
#include "qscript/src/FFIInternal.h"
#include "qscript/src/Runtime.h"

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
Result<std::vector<std::uint8_t>> build(const Unit& unit, Options o) {
    auto assembly = select(unit, o); if (!assembly) return {std::nullopt, std::move(assembly.diagnostics)};
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
// Execute `p` with the host bindings already installed in `o`.
Result<metail::Execution> execute(const metail::Program& p, Options o, bool jit) {
    auto tc=metachain::Toolchain(std::move(o.toolchain));
    Result<metail::Execution> executed;
    if (jit) {
        auto native=tc.jit(p);
        executed.diagnostics=std::move(native.diagnostics);
        if (native.value) executed.value=static_cast<metail::Execution>(std::move(*native.value));
    } else executed=tc.run(p);
    translate(p, executed);
    return executed;
}
// Integers printed through the session (see Session::printed_integers) are
// reported in Execution::output, after any integers the VM printed itself.
void adopt_printed_integers(Result<metail::Execution>& out, const detail::Session& session) {
    if (!out.value) return;
    const auto& ints = session.printed_integers();
    out->output.insert(out->output.end(), ints.begin(), ints.end());
}
Result<metail::Execution> failure(std::string message) {
    Result<metail::Execution> out;
    out.diagnostics.push_back({metachain::Stage::Verify, std::move(message), {}, 0, 0, 0});
    return out;
}
} // namespace

Result<metail::Execution> run(const metail::Program& p, Options o) {
    const bool jit=o.jit;
    try { o=detail::runtime_options(std::move(o)); }
    catch (const std::invalid_argument& e) { return failure(e.what()); }
    return execute(p, std::move(o), jit);
}
Result<metail::Execution> run(const Unit& unit, Options o) {
    const bool jit=o.jit;
    detail::Session session(unit, o);
    try { o=detail::runtime_options(o, &session); }
    catch (const std::invalid_argument& e) { return failure(e.what()); }
    auto out=execute(unit.program, std::move(o), jit);
    adopt_printed_integers(out, session);
    return out;
}
namespace {
Result<metail::Execution> execute_call(const Unit& unit, const FunctionInfo& function,
                                     const std::vector<std::int64_t>& args, Options o, detail::Session& session) {
    Result<metail::Execution> out;
    const auto* f = &function;
    if (f->arity>16 || f->entry>=unit.program.size()) return failure("invalid function metadata");
    // Verify before relocating: malformed public Unit values may otherwise
    // overflow branch immediates or enter the generated argument stub.
    for (const auto& e:metail::verify(unit.program,o.toolchain.il))
        out.diagnostics.push_back({metachain::Stage::Verify,e.message,e.source.file,e.source.line,e.source.column,e.instruction});
    if (!out.diagnostics.empty()) return out;
    const auto extra=2*args.size()+3;
    if (unit.program.size()>o.toolchain.il.max_instructions || extra>o.toolchain.il.max_instructions-unit.program.size())
        return failure("instruction limit exceeded by call stub");
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
    const bool jit=o.jit;
    try { o=detail::runtime_options(o, &session); }
    catch (const std::invalid_argument& e) { return failure(e.what()); }
    out = execute(p, std::move(o), jit);
    adopt_printed_integers(out, session);
    for (auto& d : out.diagnostics) if (d.offset) --d.offset;
    return out;
}
const FunctionInfo* find_function(const Unit& unit, std::string_view name) {
    for (const auto& f : unit.functions) if (f.name == name) return &f;
    return nullptr;
}
} // namespace

Result<metail::Execution> call(const Unit& unit, std::string_view function, const std::vector<std::int64_t>& args, Options o) {
    const auto* f = find_function(unit, function);
    if (!f) return failure("no such function: " + std::string(function));
    if (args.size() != f->arity) return failure("function " + f->name + " expects " + std::to_string(f->arity) + " arguments");
    // Preserve the legacy integer ABI; typed calls are additive.
    for (const auto& t : f->parameter_types)
        if (t != "int") return failure("function " + f->name + " takes " + t + " arguments; qs_call passes integers only");
    if (f->result_type != "int") return failure("function " + f->name + " returns " + f->result_type + "; qs_call returns integers only");
    detail::Session session(unit, o);
    return execute_call(unit, *f, args, std::move(o), session);
}

Result<CallResult> call_value(const Unit& unit, std::string_view function, std::span<const Value> args, Options o) {
    const auto fail = [](std::string message, metachain::Stage stage = metachain::Stage::Verify) -> Result<CallResult> {
        return {std::nullopt, {{stage, std::move(message), {}, 0, 0, 0}}};
    };
    const auto* f = find_function(unit, function);
    if (!f) return fail("no such function: " + std::string(function));
    if (args.size() != f->arity) return fail("function " + f->name + " expects " + std::to_string(f->arity) + " arguments");
    if (f->arity > 16 || f->entry >= unit.program.size() || (!f->parameter_types.empty() && f->parameter_types.size() != f->arity))
        return fail("invalid function metadata");
    const auto result_type = detail::parse_type(f->result_type);
    if (!result_type) return fail("invalid function result type");
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view expected = f->parameter_types.empty() ? "int" : std::string_view(f->parameter_types[i]);
        if (!detail::parse_type(expected)) return fail("invalid function parameter type");
        if (args[i].type() != expected)
            return fail("argument " + std::to_string(i + 1) + " of " + f->name + " expects " + std::string(expected) + ", got " + std::string(args[i].type()));
    }
    try {
        detail::Session session(unit, o);
        std::vector<std::int64_t> values;
        for (const auto& arg : args) values.push_back(session.import_value(arg));
        auto executed = execute_call(unit, *f, values, std::move(o), session);
        Result<CallResult> result;
        result.diagnostics = std::move(executed.diagnostics);
        if (executed.value) {
            // Retain statistics/output on traps, but never export an invalid
            // handle from a run that did not return normally.
            Value value;
            if (executed->status == metavm::Status::Halted) value = session.export_value(executed->value, *result_type);
            if (!result_type->is_int()) executed->value = 0; // statistics must not leak a dead heap handle
            result.value = CallResult{std::move(value), std::move(*executed.value)};
        }
        return result;
    } catch (const std::exception& e) {
        return fail(e.what(), metachain::Stage::Execute);
    }
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
