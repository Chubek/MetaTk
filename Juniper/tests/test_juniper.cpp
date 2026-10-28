#include <juniper/Juniper.hpp>
#include <algorithm>
#include <bit>
#include <iostream>
#include <random>
#include <stdexcept>
#include <unordered_map>

namespace {
int checks = 0;
void require(bool value, const std::string& message) {
    ++checks; if (!value) throw std::runtime_error(message);
}
template<class T> T take(juniper::Result<T> result) {
    if (!result) throw std::runtime_error(juniper::format(*result.error()));
    return std::move(*result.get());
}
using I = std::int64_t;
using U = std::uint64_t;
I wrap(U n) { return std::bit_cast<I>(n); }
I operation(std::string name, const std::vector<I>& a) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (name == "add" || name == "addi") return wrap(static_cast<U>(a[0]) + static_cast<U>(a[1]));
    if (name == "sub") return wrap(static_cast<U>(a[0]) - static_cast<U>(a[1]));
    if (name == "mul") return wrap(static_cast<U>(a[0]) * static_cast<U>(a[1]));
    if (name == "madd") return wrap(static_cast<U>(a[0]) * static_cast<U>(a[1]) + static_cast<U>(a[2]));
    if (name == "neg") return wrap(U{0} - static_cast<U>(a[0]));
    if (name == "bnot") return ~a[0];
    if (name == "lnot") return !a[0];
    if (name == "band") return a[0] & a[1];
    if (name == "bor") return a[0] | a[1];
    if (name == "bxor") return a[0] ^ a[1];
    if (name == "land") return a[0] != 0 && a[1] != 0;
    if (name == "lor") return a[0] != 0 || a[1] != 0;
    if (name == "eq") return a[0] == a[1];
    if (name == "ne") return a[0] != a[1];
    if (name == "lt") return a[0] < a[1];
    if (name == "le") return a[0] <= a[1];
    if (name == "gt") return a[0] > a[1];
    if (name == "ge") return a[0] >= a[1];
    if (name == "div" || name == "mod") {
        if (a[1] == 0 || (a[0] == std::numeric_limits<I>::min() && a[1] == -1)) throw std::runtime_error("trap");
        return name == "div" ? a[0] / a[1] : a[0] % a[1];
    }
    if (name == "shl" || name == "shr") {
        if (a[1] < 0 || a[1] > 63) throw std::runtime_error("trap");
        return name == "shl" ? wrap(static_cast<U>(a[0]) << a[1]) : a[0] >> a[1];
    }
    throw std::runtime_error("unknown op: " + name);
}
struct State {
    std::unordered_map<std::string, I> regs;
    std::vector<std::string> trace;
    I call(const std::string& name) { trace.push_back(name); return static_cast<I>(trace.size()); }
    I expr(const juniper::Expr& e) {
        if (e.kind == METAIL_INT) return e.value;
        if (e.kind == METAIL_VAR) return regs.at(e.name);
        if (e.kind == METAIL_OP && e.name == "select") return expr(e.args[0]) ? expr(e.args[1]) : expr(e.args[2]);
        std::vector<I> args;
        for (const auto& a : e.args) args.push_back(expr(a));
        if (e.kind == METAIL_CALL) return call(e.name);
        return operation(e.name, args);
    }
};
struct Outcome { I value = 0; bool trap = false; std::vector<std::string> trace; };
Outcome interpret(const juniper::Function& fn, const std::vector<I>& arguments) {
    State s;
    for (std::size_t i = 0; i < fn.params.size(); ++i) s.regs[fn.params[i]] = arguments.at(i);
    std::unordered_map<std::string, std::size_t> labels;
    for (std::size_t i = 0; i < fn.statements.size(); ++i) if (fn.statements[i].kind == METAIL_LABEL) labels[fn.statements[i].name] = i;
    try {
        for (std::size_t pc = 0, steps = 0; pc < fn.statements.size() && steps++ < 10000; ++pc) {
            const auto& st = fn.statements[pc];
            if (st.kind == METAIL_RETURN) return {s.expr(st.expr), false, s.trace};
            if (st.kind == METAIL_LET) s.regs[st.name] = s.expr(st.expr);
            if (st.kind == METAIL_EVAL) (void)s.expr(st.expr);
            if (st.kind == METAIL_ASSUME && !s.expr(st.expr)) throw std::runtime_error("trap");
            if (st.kind == METAIL_GOTO) pc = labels.at(st.target);
            if (st.kind == METAIL_BRANCH) pc = labels.at(s.expr(st.expr) ? st.yes : st.no);
        }
        throw std::runtime_error("no return");
    } catch (const std::runtime_error&) { return {0, true, s.trace}; }
}
Outcome interpret(const juniper::MachineFunction& fn, const std::vector<I>& arguments) {
    State s;
    for (std::size_t i = 0; i < fn.params.size(); ++i) s.regs[fn.params[i]] = arguments.at(i);
    std::unordered_map<std::string, std::size_t> labels;
    for (std::size_t i = 0; i < fn.instructions.size(); ++i)
        if (fn.instructions[i].opcode == "LABEL") labels[fn.instructions[i].operands[0].name] = i;
    auto value = [&](const juniper::Operand& o) { return o.kind == juniper::Operand::Kind::immediate ? o.value : s.regs.at(o.name); };
    try {
        for (std::size_t pc = 0, steps = 0; pc < fn.instructions.size() && steps++ < 10000; ++pc) {
            const auto& inst = fn.instructions[pc];
            if (inst.opcode == "LABEL") continue;
            if (inst.opcode == "BR") { pc = labels.at(value(inst.operands[0]) ? inst.operands[1].name : inst.operands[2].name); continue; }
            if (inst.opcode == "JUMP") { pc = labels.at(inst.operands[0].name); continue; }
            if (inst.opcode == "RET") return {value(inst.operands[0]), false, s.trace};
            if (inst.opcode == "ASSUME") { if (!value(inst.operands[0])) throw std::runtime_error("trap"); continue; }
            I result;
            if (inst.opcode == "CALL") {
                for (std::size_t i = 1; i < inst.operands.size(); ++i) (void)value(inst.operands[i]);
                result = s.call(inst.operands[0].name);
            } else {
                std::vector<I> args; for (const auto& a : inst.operands) args.push_back(value(a));
                result = inst.opcode == "CONST" || inst.opcode == "COPY" ? args[0] : operation(inst.opcode, args);
            }
            s.regs[inst.result] = result;
        }
        throw std::runtime_error("no return");
    } catch (const std::runtime_error&) { return {0, true, s.trace}; }
}
void equivalent(const Outcome& a, const Outcome& b) {
    require(a.trap == b.trap && (a.trap || a.value == b.value) && a.trace == b.trace, "machine semantics differ");
}
void check_schedule(const juniper::MachineModule& m) {
    for (const auto& f : m.functions) {
        std::vector<const juniper::Instruction*> ids(f.instructions.size());
        std::map<std::uint64_t, std::map<std::string, unsigned>> occupancy;
        std::map<std::uint64_t, unsigned> issue;
        for (const auto& i : f.instructions) { ids[i.id] = &i; require(i.cycle.has_value(), "cycle missing"); }
        for (const auto& i : f.instructions) {
            require(++issue[*i.cycle] <= m.backend.issue_width, "issue width exceeded");
            require(++occupancy[*i.cycle][i.resource] <= m.backend.resources.at(i.resource), "resource capacity exceeded");
            for (auto d : i.dependencies) require(*i.cycle >= *ids[d]->cycle + ids[d]->latency, "dependency latency violated");
        }
    }
}
void differential(juniper::Compiler& c, const std::string& text, const std::vector<std::vector<I>>& inputs) {
    auto m = take(c.parse(text));
    m.backend.issue_width = 4; m.backend.resources["alu"] = 2;
    auto selected = take(juniper::select(m)); auto scheduled = take(juniper::schedule(selected));
    check_schedule(scheduled);
    require(juniper::emit_machine_json(scheduled) == juniper::emit_machine_json(take(juniper::schedule(scheduled))), "reschedule is not deterministic");
    for (const auto& input : inputs) {
        auto expected = interpret(m.functions[0], input);
        equivalent(expected, interpret(selected.functions[0], input));
        equivalent(expected, interpret(scheduled.functions[0], input));
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "source directory required");
        juniper::Compiler c;
        auto loaded = take(c.load(std::filesystem::path(argv[1]) / "examples/Juniperfile"));
        auto text = take(c.emit(loaded));
        require(take(c.emit(take(c.parse(text)))) == text, "IR round trip failed");
        require(text.find("@") == std::string::npos, "macro survived canonical emission");
        auto expanded = take(juniper::preprocess(std::filesystem::path(argv[1]) / "examples/Juniperfile"));
        require(expanded.dependencies.size() == 3, "transitive dependencies missing");
        juniper::PreprocessOptions tiny; tiny.max_output_bytes = 1;
        auto bounded = juniper::preprocess(std::filesystem::path(argv[1]) / "examples/Juniperfile", tiny);
        require(!bounded && bounded.error()->code == "JUN-E203", "expansion output budget not enforced");
        require(take(c.emit_metail(loaded)).starts_with("module affine;"), "MetaIL import failed");
        require(!take(c.analyze(loaded, "ControlFlowGraph")).empty(), "analysis absent");
        auto optimized = take(c.optimize(take(c.parse("fn f(x) { return add(x,0); }")), "AddZeroRight"));
        require(take(c.emit_metail(optimized)).find("return x;") != std::string::npos, "real MetaIL rewrite did not run");
        auto foreign = take(c.parse(R"(
          extern fn magnitude(i64) -> i64 from "c" symbol "labs";
          extern fn custom(i64) -> i64 from "./libcustom.so" symbol "custom_fn";
          extern fn release(ptr) -> void from "c" symbol "free";
          fn foreign(x) { eval release(0); return custom(magnitude(x)) * 0; }
        )"));
        auto foreign_text = take(c.emit(foreign));
        require(take(c.emit(take(c.parse(foreign_text)))) == foreign_text, "external declarations did not round trip");
        auto foreign_optimized = take(c.optimize(foreign));
        require(foreign_optimized.externals.size() == 3, "optimization dropped imports");
        require(take(c.emit_metail(foreign_optimized)).find("custom(magnitude(x))") != std::string::npos, "external effects eliminated");
        auto foreign_machine = take(juniper::select(foreign_optimized));
        auto foreign_scheduled = take(juniper::schedule(foreign_machine));
        require(foreign_scheduled.externals.size() == 3 && foreign_scheduled.externals[1].symbol == "custom_fn", "machine output lost linkage");
        require(juniper::emit_linkage_json(foreign_scheduled.externals) + '\n' == take(c.linkage(foreign)), "frontend and machine linker manifests differ");
        std::size_t imported_calls = 0;
        for (const auto& inst : foreign_scheduled.functions[0].instructions) if (inst.opcode == "CALL") {
            require(inst.external.has_value() && inst.barrier, "external call is not an indexed effect barrier");
            const auto& f = foreign_scheduled.externals[*inst.external];
            require(inst.operands[0].name == f.symbol, "external alias was not resolved");
            require(f.result != "void" || inst.result.empty(), "void call defines a result");
            ++imported_calls;
        }
        require(imported_calls == 3, "external call count changed");
        auto invalid_external = foreign_machine;
        for (auto& inst : invalid_external.functions[0].instructions) if (inst.external) { inst.external = 999; break; }
        require(!juniper::schedule(invalid_external), "invalid external call index accepted");
        invalid_external = foreign_machine; invalid_external.externals[0].library = "@response/file";
        require(!juniper::schedule(invalid_external), "invalid native-built external metadata accepted");
        require(!c.parse("extern fn f(i64) -> i64 from \"c\"; fn main(){return f();}"), "invalid external arity accepted");
        require(!c.parse("extern fn f() -> void from \"c\"; fn main(){return f();}"), "void result consumed");
        foreign.externals[0].library.push_back('\0');
        require(!c.check(foreign), "NUL in external metadata accepted");
        for (const auto* bad : {"juniper 2; fn f(){return 0;}", "backend b { issue 0; } fn f(){return 0;}",
             "backend b { rule a : add(reg) => ADD; } fn f(){return 0;}",
             "backend b { rule a : select(reg,reg,reg) => S; } fn f(){return 0;}",
             "backend b { rule a : add(div(reg,reg),reg) => BAD; } fn f(){return 0;}",
             "backend b { rule a : imm(2,1) => I; } fn f(){return 0;}",
             "backend b { issue 2; issue 3; } fn f(){return 0;}",
             "backend b { rule a : add(reg,reg) => ADD resource missing; } fn f(){return 0;}",
             "fn f() { goto missing; return 0; }", "fn f() { return add(1); }"})
            require(!c.parse(bad), std::string("invalid source accepted: ") + bad);
        auto no_rules = take(c.parse("backend none {} fn f(x){ return add(x,1); }"));
        require(!juniper::select(no_rules), "uncovered instruction was silently selected");
        auto fused = take(c.parse("backend fused extends portable { rule f : add(mul(reg,reg),reg) => MADD cost 3 latency 4; } fn f(x,y,z){return add(mul(x,y),z);}"));
        auto machine = take(juniper::select(fused));
        require(machine.functions[0].instructions[0].opcode == "MADD" && machine.functions[0].instructions.size() == 2, "minimum-cost fusion failed");
        equivalent(interpret(fused.functions[0], {std::numeric_limits<I>::max(), 7, 9}), interpret(machine.functions[0], {std::numeric_limits<I>::max(), 7, 9}));
        auto immediate = take(juniper::select(loaded));
        require(std::any_of(immediate.functions[0].instructions.begin(), immediate.functions[0].instructions.end(), [](const auto& i) { return i.opcode == "ADDI"; }), "immediate selection failed");
        auto outside = loaded; outside.functions[0].statements.back().expr.args[1].value = 2048;
        auto fallback = take(juniper::select(outside));
        require(std::none_of(fallback.functions[0].instructions.begin(), fallback.functions[0].instructions.end(), [](const auto& i) { return i.opcode == "ADDI"; }), "out-of-range immediate selected");
        differential(c, "fn f(c,d) { return select(c,first(),div(64,d)); }", {{1,0},{0,0},{0,4}});
        differential(c, "fn f(x) { return add(first(),mul(second(),x)); }", {{0},{-1},{9}});
        differential(c, "fn f() { let x=1; let y=add(x,2); let x=3; return add(y,x); }", {{}});
        differential(c, "fn f(x) { let x=add(x,1); let x=mul(x,3); return x; }", {{1},{std::numeric_limits<I>::max()}});
        differential(c, "fn f(x,y) { eval div(x,y); return tick(); }", {{1,0},{2,1},{std::numeric_limits<I>::min(),-1}});
        differential(c, "fn f(x) { assume x; return tick(); }", {{0},{1}});
        differential(c, "fn f(x) { br x,yes,no; yes: return first(); no: return second(); }", {{0},{1}});
        differential(c, "fn f(x) { start: br x,body,done; body: let x=sub(x,1); goto start; done: return x; }", {{0},{5}});
        differential(c, "fn f(x) { return land(0,tick()); }", {{0}}); // logical operators are eager
        differential(c, "fn f(x) { return bnot(neg(lnot(x))); }", {{0},{-1},{std::numeric_limits<I>::min()}});
        differential(c, "fn f(x,y) { return select(x,select(y,first(),second()),third()); }", {{1,1},{1,0},{0,0}});
        std::mt19937_64 random(0x4a554e49504552ULL);
        const std::vector<std::string> ops{"add","sub","mul","band","bor","bxor","eq","ne","lt","le","gt","ge","land","lor","div","mod","shl","shr"};
        for (unsigned i = 0; i < 100; ++i) {
            auto op = ops[random() % ops.size()];
            std::string source = "fn f(x,y) { return " + op + "(add(x,7),mul(y,3)); }";
            differential(c, source, {{wrap(random()),wrap(random())}, {std::numeric_limits<I>::min(),-1}, {std::numeric_limits<I>::max(),0}, {0,21}});
        }
        auto broken = machine; broken.functions[0].instructions[0].id = 99;
        require(!juniper::schedule(broken), "invalid machine IDs accepted");
        auto limited = loaded; limited.backend.issue_width = 0;
        require(!c.check(limited), "native invalid profile accepted");
        std::string embedded = "fn f(){return 0;}";
        embedded.push_back('\0'); embedded += "junk";
        require(!c.parse(embedded), "embedded NUL accepted");
        std::cout << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
