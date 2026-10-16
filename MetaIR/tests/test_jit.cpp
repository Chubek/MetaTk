#include "MetaChain.hpp"
#include <cstdlib>
#include <iostream>
#include <random>

static unsigned checks = 0;
#define CHECK(...) do { ++checks; if (!(__VA_ARGS__)) { std::cerr << __FILE__ << ':' << __LINE__ << ": " #__VA_ARGS__ "\n"; std::exit(1); } } while (false)

static metail::Program parse(std::string_view source) {
    auto p = metail::parse(source, "jit-test.mcil"); CHECK(p); return p.program;
}
static void equivalent(const metail::Execution& a, const metail::Execution& b) {
    CHECK(a.status == b.status && a.steps == b.steps && a.output == b.output);
    CHECK(a.value == b.value);
    CHECK(a.diagnostics.size() == b.diagnostics.size());
    for (std::size_t n=0; n<a.diagnostics.size(); ++n) {
        CHECK(a.diagnostics[n].message == b.diagnostics[n].message);
        CHECK(a.diagnostics[n].instruction == b.diagnostics[n].instruction);
        CHECK(a.diagnostics[n].source.file == b.diagnostics[n].source.file);
        CHECK(a.diagnostics[n].source.line == b.diagnostics[n].source.line);
    }
}
static metajit::Options options(metajit::Mode mode) {
    metajit::Options o; o.mode = mode; o.method_threshold = 1; return o;
}
static void basic_tests() {
    const auto p = parse("const r0, 1\nconst r1, 10\nconst r2, 1\nloop: mul r0, r0, r1\nsub r1, r1, r2\njnz r1, loop\nprint r0\nhalt r0\n");
    metavm::VMOptions vm; vm.hot_threshold = 1;
    for (auto mode : {metajit::Mode::Tracing, metajit::Mode::Method, metajit::Mode::Hybrid}) {
        auto r = metajit::run(p, options(mode), vm);
        equivalent(metail::run(p, vm), r); CHECK(r && r.value == 3628800);
        CHECK(r.compilation_diagnostics.empty());
        CHECK(r.statistics.interpreted_instructions+r.statistics.native_instructions == r.steps);
        if (metajit::native_available()) {
            CHECK(r.statistics.native_entries > 0 && r.statistics.native_instructions > 0);
            if (mode == metajit::Mode::Tracing) CHECK(r.statistics.compiled_traces == 1 && r.statistics.side_exits > 0);
            else CHECK(r.statistics.compiled_methods == 1 && r.statistics.interpreted_instructions == 0);
        }
    }
    const auto high_registers = parse("mov r30, r31\nconst r31, -9223372036854775808\nmov r0, r31\nnop\nadd r31, r0, r30\nprint r31\nhalt r31\n");
    equivalent(metail::run(high_registers), metajit::run(high_registers, options(metajit::Mode::Method)));
    metachain::Options facade; facade.jit = options(metajit::Mode::Method);
    auto result = metachain::Toolchain(facade).jit("const r0, 42\nhalt r0\n");
    CHECK(result && result->value == 42);
    CHECK(!metachain::Toolchain().jit("jmp missing\n"));
    facade.il.max_instructions = 1;
    CHECK(!metachain::Toolchain(facade).jit(p));
}
static void control_flow_tests() {
    const auto calls = parse("const r0, 0\nconst r1, 40\nconst r2, 1\nloop: call f\nsub r1, r1, r2\njnz r1, loop\nprint r0\nhalt r0\nf: push r1\ncall g\npop r1\nret\ng: add r0, r0, r2\nret\n");
    const auto recursive = parse("const r0, 0\nconst r1, 12\nconst r2, 1\ncall f\nhalt r0\nf: jz r1, done\nadd r0, r0, r1\nsub r1, r1, r2\ncall f\ndone: ret\n");
    const auto guards = parse("const r0, 30\nconst r1, 1\nconst r2, 15\nloop: sub r0, r0, r1\neq r3, r0, r2\njnz r3, skip\nprint r0\nskip: jnz r0, loop\nhalt r0\n");
    metavm::VMOptions vm; vm.hot_threshold = 1;
    for (auto mode : {metajit::Mode::Tracing, metajit::Mode::Method, metajit::Mode::Hybrid}) {
        auto o = options(mode);
        if (mode == metajit::Mode::Hybrid) o.method_threshold = 3;
        for (const auto& p : {calls, recursive, guards}) {
            auto r = metajit::run(p, o, vm); equivalent(metail::run(p, vm), r);
            CHECK(r && r.compilation_diagnostics.empty());
        }
    }
    // Short traces must side-exit with the correct PC, even at length zero
    // (normalized to one). Disabling recording leaves a normal interpreter.
    for (std::size_t length : {0u, 1u, 2u, 256u}) {
        vm.max_trace_length = length;
        auto r = metajit::run(guards, options(metajit::Mode::Tracing), vm);
        equivalent(metail::run(guards, vm), r);
    }
    vm.max_traces = 0;
    auto disabled = metajit::run(guards, options(metajit::Mode::Tracing), vm);
    CHECK(disabled && disabled.statistics.native_entries == 0);
    const auto nested = parse("const r0, 6\nconst r1, 1\nouter: const r2, 4\ninner: add r3, r3, r1\nsub r2, r2, r1\njnz r2, inner\nsub r0, r0, r1\njnz r0, outer\nhalt r3\n");
    vm.max_traces = 8; vm.max_trace_length = 7;
    equivalent(metail::run(nested, vm), metajit::run(nested, options(metajit::Mode::Tracing), vm));
    // Return to exactly program.size() and fallthrough both halt without a Halt.
    for (const auto& p : {parse("const r0, 3\npush r0\nret\n"), parse("nop\n")})
        equivalent(metail::run(p), metajit::run(p, options(metajit::Mode::Method)));
}
static void limit_and_trap_tests() {
    const auto loop = parse("again: jmp again\n");
    const auto counted = parse("const r0, 4\nconst r1, 1\nloop: sub r0, r0, r1\njnz r0, loop\nhalt r0\n");
    const auto stack = parse("const r0, 7\nloop: push r0\njmp loop\n");
    const auto recursion = parse("again: call again\n");
    for (auto mode : {metajit::Mode::Tracing, metajit::Mode::Method, metajit::Mode::Hybrid}) {
        auto o = options(mode);
        for (const auto& p : {loop, counted, stack, recursion, parse("const r0, 42\nhalt r0\n")}) {
            for (std::size_t budget=0; budget<25; ++budget) {
                metavm::VMOptions vm; vm.max_steps = budget; vm.hot_threshold = 1; vm.max_stack_values = 3;
                equivalent(metail::run(p, vm), metajit::run(p, o, vm));
            }
        }
        for (const auto& p : {
                parse("pop r0\nhalt r0\n"), parse("ret\n"),
                parse("const r0, -1\npush r0\nret\n"), parse("const r0, 99\npush r0\nret\n"),
                parse("const r0, 1\nconst r1, 0\ndiv r0, r0, r1\nhalt r0\n"),
                parse("const r0, -9223372036854775808\nconst r1, -1\nmod r0, r0, r1\nhalt r0\n"),
                parse("const r0, 1\nconst r1, 64\nshl r0, r0, r1\nhalt r0\n")})
            equivalent(metail::run(p), metajit::run(p, o));
    }
    // The error occurs after native trace entry, not just in warmup.
    const auto late_trap = parse("const r0, 8\nconst r1, 1\nloop: sub r0, r0, r1\ndiv r2, r1, r0\njnz r0, loop\nhalt r0\n");
    metavm::VMOptions vm; vm.hot_threshold = 1;
    auto r = metajit::run(late_trap, options(metajit::Mode::Tracing), vm);
    equivalent(metail::run(late_trap, vm), r);
    if (metajit::native_available()) CHECK(r.statistics.native_entries > 0);
}
static void cache_and_fallback_tests() {
    const auto p = parse("const r0, 42\nhalt r0\n");
    auto o = options(metajit::Mode::Method); o.method_threshold = 2;
    metajit::Engine engine(o); CHECK(engine.load(p));
    auto first = engine.run(); CHECK(first && first.statistics.native_entries == 0);
    auto second = engine.run(); CHECK(second && second.value == 42);
    if (metajit::native_available()) CHECK(second.statistics.compiled_methods == 1 && second.statistics.native_entries == 1);
    CHECK(engine.load(parse("const r0, -7\nhalt r0\n")));
    CHECK(engine.statistics().code_bytes == 0 && engine.traces().empty());
    CHECK(engine.run().value == -7 && engine.run().value == -7);
    CHECK(!engine.compile_method(99));
    CHECK(engine.load(p));
    CHECK(engine.compile_method() == metajit::native_available());
    equivalent(metail::run(p), engine.run());
    if (metajit::native_available()) {
        auto bounded = options(metajit::Mode::Method);
        bounded.max_code_bytes = engine.statistics().code_bytes; // exactly one mapping
        auto called = parse("call f\nhalt r0\nf: const r0, 42\nret\n");
        auto r = metajit::run(called, bounded);
        equivalent(metail::run(called), r);
        CHECK(r.statistics.compiled_methods == 1 && r.statistics.compilation_failures == 1);
        CHECK(r.statistics.code_bytes == bounded.max_code_bytes);
    }
    auto malformed = p; malformed.code[0].a = 32;
    CHECK(!engine.load(malformed)); CHECK(!engine.run()); CHECK(!engine.compile_method());
    CHECK(!metajit::Engine().run());
    metail::Limits limits; limits.max_instructions = 1;
    CHECK(!metajit::run(p, {}, {}, limits));
    for (unsigned kind=0; kind<3; ++kind) {
        auto tiny = options(metajit::Mode::Method);
        if (kind == 0) tiny.max_code_bytes = 0;
        if (kind == 1) tiny.max_source_bytes = 1;
        if (kind == 2) tiny.max_methods = 0;
        metajit::Engine fallback(tiny); CHECK(fallback.load(p));
        auto r = fallback.run(); equivalent(metail::run(p), r);
        CHECK(r && r.statistics.native_entries == 0);
        CHECK(r.statistics.code_bytes <= tiny.max_code_bytes);
        const auto failures = r.statistics.compilation_failures;
        CHECK(fallback.run().statistics.compilation_failures == failures);
        if (metajit::native_available() && kind != 2) CHECK(!r.compilation_diagnostics.empty());
    }
    auto tiny_trace = options(metajit::Mode::Tracing); tiny_trace.max_code_bytes = 1;
    metajit::Engine trace_engine(tiny_trace);
    auto loop = parse("loop: jmp loop\n"); CHECK(trace_engine.load(loop));
    metavm::VMOptions vm; vm.max_steps = 30; vm.hot_threshold = 1;
    auto traced = trace_engine.run(vm); equivalent(metail::run(loop, vm), traced);
    CHECK(trace_engine.traces().size() == 1 && traced.statistics.native_entries == 0);
    CHECK(trace_engine.run(vm).statistics.compilation_failures == traced.statistics.compilation_failures);
    CHECK(trace_engine.load(p)); CHECK(trace_engine.traces().empty());
    CHECK(trace_engine.run().value == 42);
}
static void observer_tests() {
    auto p = parse("const r0, 3\nconst r1, 1\nloop: sub r0, r0, r1\njnz r0, loop\nprint r0\nhalt r0\n");
    metavm::VMOptions vm; vm.collect_trace = true; vm.hot_threshold = 1;
    std::vector<std::size_t> expected;
    metail::VM interpreter(vm); interpreter.load(p);
    interpreter.trace_bus().subscribe([&](const auto& e) { if (e.kind == metavm::TraceEvent::Kind::Instruction) expected.push_back(e.pc); });
    interpreter.run();
    for (auto mode : {metajit::Mode::Tracing, metajit::Mode::Method}) {
        metajit::Engine engine(options(mode)); CHECK(engine.load(p));
        std::vector<std::size_t> observed;
        std::size_t native_entries = 0, native_exits = 0;
        engine.trace_bus().subscribe([&](const auto& e) {
            if (e.kind == metavm::TraceEvent::Kind::Instruction) observed.push_back(e.pc);
            if (e.kind == metavm::TraceEvent::Kind::NativeEntry) ++native_entries;
            if (e.kind == metavm::TraceEvent::Kind::NativeExit) ++native_exits;
        });
        CHECK(engine.run(vm)); CHECK(observed == expected && native_entries == native_exits);
    }
    metajit::Engine throwing(options(metajit::Mode::Method)); CHECK(throwing.load(p));
    throwing.trace_bus().subscribe([](const auto& e) {
        if (e.kind == metavm::TraceEvent::Kind::Instruction) throw std::runtime_error("observer failed");
    });
    auto trapped = throwing.run(vm);
    CHECK(!trapped && trapped.status == metavm::Status::Trapped && trapped.steps == 0);
    CHECK(trapped.diagnostics.front().message == "observer failed");
    metajit::Engine halt_observer(options(metajit::Mode::Method));
    CHECK(halt_observer.load(parse("const r0, 42\nhalt r0\n")));
    halt_observer.trace_bus().subscribe([](const auto& e) {
        if (e.kind == metavm::TraceEvent::Kind::Instruction && e.pc == 1) throw std::runtime_error("halt observer failed");
    });
    auto at_halt = halt_observer.run(vm);
    CHECK(!at_halt && at_halt.value == 42 && at_halt.steps == 1);
}
static void differential_tests() {
    std::mt19937_64 random(0x1234abcd);
    for (unsigned trial=0; trial<160; ++trial) {
        metail::Builder b;
        for (unsigned n=0; n<8; ++n) b.constant(n, std::bit_cast<std::int64_t>(random()));
        b.emit({metail::Op::Print, 0});
        for (unsigned n=0; n<20; ++n) {
            const auto op = static_cast<metail::Op>(static_cast<unsigned>(metail::Op::Add)+random()%12);
            b.binary(op, random()%8, random()%8, random()%8);
        }
        b.halt(random()%8);
        auto p = b.finish(); CHECK(p);
        auto r = metajit::run(p.program, options(metajit::Mode::Method));
        equivalent(metail::run(p.program), r); CHECK(r.compilation_diagnostics.empty());
        if (metajit::native_available()) CHECK(r.statistics.native_entries == 1);
    }
    // Explicit edge values and aliased operands for every arithmetic opcode.
    const std::int64_t values[] = {INT64_MIN, INT64_MAX, -1, 0, 1, 63, 64};
    for (unsigned op=static_cast<unsigned>(metail::Op::Add); op<=static_cast<unsigned>(metail::Op::Lt); ++op) {
        for (auto a : values) for (auto b : values) {
            metail::Builder builder;
            builder.constant(0, a).constant(1, b).binary(static_cast<metail::Op>(op), 0, 0, 1).halt(0);
            auto p = builder.finish(); CHECK(p);
            equivalent(metail::run(p.program), metajit::run(p.program, options(metajit::Mode::Method)));
        }
    }
}
int main() {
    basic_tests(); control_flow_tests(); limit_and_trap_tests(); cache_and_fallback_tests();
    observer_tests(); differential_tests();
    std::cout << checks << " JIT checks passed\n";
}
