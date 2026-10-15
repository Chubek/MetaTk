#pragma once

// In-process MetaIL JIT. Arithmetic and control flow are emitted as x86-64
// instructions; stack operations and output use exception-contained C++ helpers.
// Other hosts (or hosts denying executable mappings) retain the interpreter.
#include "MetaIL.hpp"
#include "ASMtk.hpp"
#include <cstddef>
#include <cstring>
#include <set>
#include <type_traits>
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace metajit {
enum class Mode { Tracing, Method, Hybrid };
struct Options {
    Mode mode = Mode::Hybrid;
    std::size_t method_threshold = 16;
    std::size_t max_methods = 128;
    // Includes page rounding, across both caches. Zero disables native code.
    std::size_t max_code_bytes = 16u * 1024u * 1024u;
    std::size_t max_source_bytes = 64u * 1024u * 1024u;
};
struct Statistics {
    std::size_t interpreted_instructions = 0, native_instructions = 0;
    std::size_t native_entries = 0, side_exits = 0;
    std::size_t compiled_traces = 0, compiled_methods = 0;
    std::size_t compilation_failures = 0, code_bytes = 0;
};
struct Execution : metail::Execution {
    Statistics statistics{};
    // Compilation failure is nonfatal: the original IL is interpreted instead.
    std::vector<metail::Diagnostic> compilation_diagnostics;
};
inline constexpr bool native_available() noexcept {
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__)
    return true;
#else
    return false;
#endif
}

namespace detail {
struct Runtime;
// Only this standard-layout ABI frame is addressed by generated code.
struct Frame {
    std::array<std::int64_t, 32> registers{};
    std::size_t pc = 0, steps = 0, max_steps = 0;
    std::size_t status = static_cast<std::size_t>(metavm::Status::Running);
    std::int64_t value = 0;
    std::size_t observe = 0;
    Runtime* runtime = nullptr;
};
static_assert(std::is_standard_layout_v<Frame>);
struct Runtime {
    const metail::Program& program;
    const metavm::VMOptions& options;
    Execution& result;
    metavm::TraceBus& bus;
    std::vector<std::int64_t> stack;
};
inline void trap(Frame& f, std::string message) {
    f.status = static_cast<std::size_t>(metavm::Status::Trapped);
    const auto* source = f.runtime->program.source_at(f.pc);
    f.runtime->result.diagnostics.push_back({std::move(message), f.pc, source ? *source : metavm::SourceLocation{}});
}
// These helpers must never unwind through generated machine code.
inline void instruction_event(Frame* f) noexcept {
    try { f->runtime->bus.emit({metavm::TraceEvent::Kind::Instruction, f->pc, 0, {}}); }
    catch (const std::exception& e) { trap(*f, e.what()); }
    catch (...) { trap(*f, "unknown exception from instruction observer"); }
}
inline void arithmetic_error(Frame* f) noexcept {
    trap(*f, "invalid division, remainder, or shift");
}
inline void complex_instruction(Frame* f) noexcept {
    using metail::Op;
    auto& rt = *f->runtime;
    const auto& i = rt.program.code[f->pc];
    try {
        switch (i.op) {
        case Op::Push: case Op::Call:
            if (rt.stack.size() >= rt.options.max_stack_values) { trap(*f, "stack limit exceeded"); return; }
            rt.stack.push_back(i.op == Op::Push ? f->registers[i.a] : static_cast<std::int64_t>(f->pc+1));
            f->pc = i.op == Op::Call ? static_cast<std::size_t>(i.immediate) : f->pc+1;
            break;
        case Op::Pop: case Op::Ret: {
            if (rt.stack.empty()) { trap(*f, "stack underflow"); return; }
            const auto value = rt.stack.back(); rt.stack.pop_back();
            if (i.op == Op::Pop) { f->registers[i.a] = value; ++f->pc; }
            else {
                if (value < 0 || static_cast<std::uint64_t>(value) > rt.program.size()) { trap(*f, "invalid return address"); return; }
                f->pc = static_cast<std::size_t>(value);
            }
            break;
        }
        case Op::Print: rt.result.output.push_back(f->registers[i.a]); ++f->pc; break;
        default: trap(*f, "invalid IL opcode"); break;
        }
    } catch (const std::exception& e) { trap(*f, e.what()); }
    catch (...) { trap(*f, "unknown exception from instruction handler"); }
}
inline void interpret(Frame& f) {
    using metail::Op;
    if (f.observe) instruction_event(&f);
    if (f.status != static_cast<std::size_t>(metavm::Status::Running)) return;
    ++f.steps;
    const auto& i = f.runtime->program.code[f.pc];
    auto& r = f.registers;
    if (metail::binary(i.op)) {
        auto value = metail::evaluate(i.op, r[i.b], r[i.c]);
        if (!value) { arithmetic_error(&f); return; }
        r[i.a] = *value; ++f.pc; return;
    }
    switch (i.op) {
    case Op::Nop: ++f.pc; break;
    case Op::Const: r[i.a] = i.immediate; ++f.pc; break;
    case Op::Mov: r[i.a] = r[i.b]; ++f.pc; break;
    case Op::Jmp: f.pc = static_cast<std::size_t>(i.immediate); break;
    case Op::Jz: case Op::Jnz:
        f.pc = ((r[i.a] == 0) == (i.op == Op::Jz)) ? static_cast<std::size_t>(i.immediate) : f.pc+1;
        break;
    case Op::Halt: f.value = r[i.a]; f.status = static_cast<std::size_t>(metavm::Status::Halted); break;
    default: complex_instruction(&f); break;
    }
}
struct Code {
    void* memory = nullptr;
    std::size_t size = 0, id = 0, entry = 0;
    bool trace = false;
    std::vector<std::size_t> pcs;
    Code() = default;
    Code(const Code&) = delete;
    Code& operator=(const Code&) = delete;
    ~Code() {
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__)
        if (memory) ::munmap(memory, size);
#endif
    }
    // Generated entries have no Clang function-type metadata before their
    // address. Disable that one check at the native-call boundary only.
#if defined(__clang__)
    __attribute__((no_sanitize("function")))
#endif
    void execute(Frame& frame) const {
        using Function = void (*)(Frame*);
        reinterpret_cast<Function>(memory)(&frame);
    }
};
inline std::unique_ptr<Code> allocate(std::span<const std::uint8_t> bytes,
        std::size_t budget, std::string& error) {
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__)
    const auto page = ::sysconf(_SC_PAGESIZE);
    if (page <= 0 || bytes.empty()) { error = "invalid executable mapping size"; return {}; }
    const auto p = static_cast<std::size_t>(page);
    if (bytes.size() > std::numeric_limits<std::size_t>::max()-(p-1)) { error = "JIT code size overflow"; return {}; }
    const auto size = ((bytes.size()+p-1)/p)*p;
    if (size > budget) { error = "JIT code cache limit exceeded"; return {}; }
    auto code = std::make_unique<Code>();
    void* memory = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) { error = "cannot allocate JIT memory"; return {}; }
    code->memory = memory; code->size = size;
    std::memcpy(memory, bytes.data(), bytes.size());
    __builtin___clear_cache(static_cast<char*>(memory), static_cast<char*>(memory)+bytes.size());
    if (::mprotect(memory, size, PROT_READ | PROT_EXEC) != 0) { error = "cannot protect JIT memory executable"; return {}; }
    return code;
#else
    (void)bytes; (void)budget;
    error = "native JIT requires Linux x86-64"; return {};
#endif
}

inline std::string lower(const metail::Program& program, const std::vector<std::size_t>& pcs,
        bool trace, std::size_t source_limit) {
    using metail::Op;
    std::ostringstream out;
    auto bounded = [&] {
        if (out.tellp() < 0 || static_cast<std::uint64_t>(out.tellp()) > source_limit)
            throw std::runtime_error("JIT assembly size limit exceeded");
    };
    auto field = [](std::size_t offset) { return "[r12+"+std::to_string(offset)+"]"; };
    const auto pc_slot = field(offsetof(Frame, pc)), steps = field(offsetof(Frame, steps));
    const auto status = field(offsetof(Frame, status));
    auto reg = [&](unsigned n) { return field(offsetof(Frame, registers)+n*sizeof(std::int64_t)); };
    auto set_pc = [&](std::size_t pc) { out << "mov rax, " << pc << "\nmov " << pc_slot << ", rax\n"; };
    auto set_status = [&](metavm::Status s) { out << "mov rax, " << static_cast<std::size_t>(s) << "\nmov " << status << ", rax\n"; };
    auto call = [&](auto function) { out << "mov rdi, r12\nmov rax, " << reinterpret_cast<std::uintptr_t>(function) << "\ncall rax\n"; };
    auto label = [](std::size_t n) { return ".jit_"+std::to_string(n); };
    std::map<std::size_t, std::size_t> positions;
    for (std::size_t n=0; n<pcs.size(); ++n) positions.emplace(pcs[n], n);
    // r12 is callee-saved. One push also aligns rsp to 16 before helper calls.
    // ENDBR64 permits entry on hosts enforcing indirect branch tracking.
    out << ".byte 243, 15, 30, 250\npush r12\nmov r12, rdi\n";
    if (!trace) {
        out << "mov rax, " << pc_slot << '\n';
        for (std::size_t n=0; n<pcs.size(); ++n) {
            out << "mov rcx, " << pcs[n] << "\ncmp rax, rcx\nje " << label(n) << '\n'; bounded();
        }
        out << "jmp .jit_exit\n";
    }
    for (std::size_t n=0; n<pcs.size(); ++n) {
        const auto pc = pcs[n]; const auto& i = program.code[pc];
        auto destination = [&](std::size_t next) {
            if (!trace) {
                auto it = positions.find(next);
                return it == positions.end() ? std::string(".jit_exit") : label(it->second);
            }
            if (n+1 < pcs.size() && next == pcs[n+1]) return label(n+1);
            if (n+1 == pcs.size() && next == pcs.front()) return label(0);
            return std::string(".jit_exit"); // branch guard / side exit
        };
        auto transfer = [&](std::size_t next) { set_pc(next); out << "jmp " << destination(next) << '\n'; };
        out << label(n) << ":\n"; set_pc(pc);
        out << "mov rax, " << steps << "\nmov rcx, " << field(offsetof(Frame, max_steps))
            << "\ncmp rax, rcx\njae .jit_limit\nmov rax, " << field(offsetof(Frame, observe))
            << "\ncmp rax, 0\nje .jit_observed_" << n << '\n';
        call(&instruction_event);
        out << "mov rax, " << status << "\ncmp rax, " << static_cast<std::size_t>(metavm::Status::Running)
            << "\njne .jit_exit\n.jit_observed_" << n << ":\nmov rax, " << steps << "\ninc rax\nmov " << steps << ", rax\n";
        if (metail::binary(i.op)) {
            out << "mov rax, " << reg(i.b) << "\nmov rcx, " << reg(i.c) << '\n';
            switch (i.op) {
            case Op::Add: case Op::Sub: case Op::And: case Op::Or: case Op::Xor:
                out << metail::name(i.op) << " rax, rcx\n"; break;
            case Op::Mul: out << "imul rax, rcx\n"; break;
            case Op::Div: case Op::Mod:
                out << "cmp rcx, 0\nje .jit_arithmetic\ncmp rcx, -1\njne .jit_div_" << n
                    << "\nmov rdx, -9223372036854775808\ncmp rax, rdx\nje .jit_arithmetic\n.jit_div_" << n << ":\ncqo\nidiv rcx\n";
                if (i.op == Op::Mod) out << "mov rax, rdx\n";
                break;
            case Op::Shl: case Op::Shr:
                out << "cmp rcx, 0\njl .jit_arithmetic\ncmp rcx, 63\njg .jit_arithmetic\n" << metail::name(i.op) << " rax, cl\n";
                break;
            case Op::Eq: case Op::Lt:
                out << "cmp rax, rcx\nmov rax, 0\n" << (i.op == Op::Eq ? "jne" : "jge") << " .jit_compare_" << n
                    << "\nmov rax, 1\n.jit_compare_" << n << ":\n"; break;
            default: break;
            }
            out << "mov " << reg(i.a) << ", rax\n"; transfer(pc+1);
        } else switch (i.op) {
        case Op::Nop: transfer(pc+1); break;
        case Op::Const: case Op::Mov:
            out << "mov rax, " << (i.op == Op::Const ? std::to_string(i.immediate) : reg(i.b)) << "\nmov " << reg(i.a) << ", rax\n";
            transfer(pc+1); break;
        case Op::Jmp: transfer(static_cast<std::size_t>(i.immediate)); break;
        case Op::Jz: case Op::Jnz:
            out << "mov rax, " << reg(i.a) << "\ncmp rax, 0\n" << (i.op == Op::Jz ? "je" : "jne") << " .jit_taken_" << n << '\n';
            transfer(pc+1); out << ".jit_taken_" << n << ":\n"; transfer(static_cast<std::size_t>(i.immediate)); break;
        case Op::Halt:
            out << "mov rax, " << reg(i.a) << "\nmov " << field(offsetof(Frame, value)) << ", rax\n";
            set_status(metavm::Status::Halted); out << "jmp .jit_exit\n"; break;
        default:
            call(&complex_instruction);
            if (i.op != Op::Call && i.op != Op::Ret) {
                out << "mov rax, " << status << "\ncmp rax, " << static_cast<std::size_t>(metavm::Status::Running) << "\njne .jit_exit\n";
                transfer(pc+1);
            } else out << "jmp .jit_exit\n"; // dispatch callee / dynamic return
            break;
        }
        bounded();
    }
    out << ".jit_arithmetic:\n"; call(&arithmetic_error); out << "jmp .jit_exit\n.jit_limit:\n";
    set_status(metavm::Status::StepLimit);
    out << ".jit_exit:\npop r12\nret\n"; bounded();
    return out.str();
}
} // namespace detail

// An engine owns a verified program and its executable mappings. load() clears
// all code and profiling data; run() starts fresh VM state but keeps hot counts
// and code, allowing repeated method invocations to become hot. Not reentrant
// or thread-safe: use separate engines for concurrent execution.
class Engine {
    Options options_;
    metail::Program program_;
    std::vector<metail::Diagnostic> errors_, compilation_errors_;
    Statistics statistics_{};
    metavm::TraceBus bus_;
    std::vector<metavm::Trace> traces_;
    std::map<std::size_t, std::unique_ptr<detail::Code>> trace_code_, method_code_;
    std::map<std::size_t, detail::Code*> method_pcs_;
    std::map<std::size_t, std::size_t> loops_, calls_;
    std::set<std::size_t> entries_, tried_methods_, tried_traces_;
    std::size_t next_id_ = 1;

    std::unique_ptr<detail::Code> compile(const std::vector<std::size_t>& pcs, bool trace, std::size_t id) {
        const auto entry = pcs.front();
        bus_.emit({metavm::TraceEvent::Kind::CompileStarted, entry, id, trace ? "trace" : "method"});
        std::string error;
        std::unique_ptr<detail::Code> code;
        try {
            if (!native_available()) error = "native JIT requires Linux x86-64";
            else {
                auto source = detail::lower(program_, pcs, trace, options_.max_source_bytes);
                asmtk::Options assembly;
                assembly.max_source_bytes = options_.max_source_bytes;
                assembly.max_output_bytes = options_.max_code_bytes-statistics_.code_bytes;
                auto bytes = asmtk::Assembler(assembly).assemble(source);
                if (!bytes) error = bytes.diagnostics.empty() ? "JIT assembly failed" : bytes.diagnostics.front().message;
                else code = detail::allocate(bytes.bytes, assembly.max_output_bytes, error);
            }
        } catch (const std::exception& e) { error = e.what(); }
        catch (...) { error = "unknown exception from JIT compiler"; }
        if (!code) {
            ++statistics_.compilation_failures;
            const auto* source = program_.source_at(entry);
            compilation_errors_.push_back({error, entry, source ? *source : metavm::SourceLocation{}});
            bus_.emit({metavm::TraceEvent::Kind::CompileFailed, entry, id, error});
            return {};
        }
        code->id = id; code->entry = entry; code->trace = trace; code->pcs = pcs;
        bus_.emit({metavm::TraceEvent::Kind::CompileCompleted, entry, id, trace ? "trace" : "method"});
        statistics_.code_bytes += code->size;
        if (trace) ++statistics_.compiled_traces; else ++statistics_.compiled_methods;
        return code;
    }
public:
    explicit Engine(Options options = {}) : options_(options) {}
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    const Options& options() const noexcept { return options_; }
    const metail::Program& program() const noexcept { return program_; }
    const Statistics& statistics() const noexcept { return statistics_; }
    const std::vector<metavm::Trace>& traces() const noexcept { return traces_; }
    const std::vector<metail::Diagnostic>& diagnostics() const noexcept { return errors_; }
    const std::vector<metail::Diagnostic>& compilation_diagnostics() const noexcept { return compilation_errors_; }
    metavm::TraceBus& trace_bus() noexcept { return bus_; }

    bool load(metail::Program program, metail::Limits limits = {}) {
        method_pcs_.clear(); trace_code_.clear(); method_code_.clear();
        traces_.clear(); loops_.clear(); calls_.clear(); entries_.clear();
        tried_methods_.clear(); tried_traces_.clear(); compilation_errors_.clear();
        statistics_ = {}; next_id_ = 1;
        program_ = std::move(program); errors_ = metail::verify(program_, limits);
        if (!errors_.empty()) return false;
        entries_.insert(0);
        for (const auto& i : program_.code)
            if (i.op == metail::Op::Call) entries_.insert(static_cast<std::size_t>(i.immediate));
        return true;
    }
    // Compile the reachable CFG, excluding callees, without waiting for heat.
    // Call/return side exits let the dispatcher select other compiled methods.
    bool compile_method(std::size_t entry = 0) {
        if (!errors_.empty() || entry >= program_.size()) return false;
        if (method_code_.contains(entry)) return true;
        if (tried_methods_.contains(entry) || tried_methods_.size() >= options_.max_methods) return false;
        tried_methods_.insert(entry);
        std::vector<std::size_t> pending{entry}, pcs;
        std::set<std::size_t> visited;
        while (!pending.empty()) {
            const auto pc = pending.back(); pending.pop_back();
            if (pc >= program_.size() || !visited.insert(pc).second) continue;
            pcs.push_back(pc);
            const auto& i = program_.code[pc];
            if (i.op != metail::Op::Halt && i.op != metail::Op::Ret && i.op != metail::Op::Jmp) pending.push_back(pc+1);
            if (i.op == metail::Op::Jmp || i.op == metail::Op::Jz || i.op == metail::Op::Jnz)
                pending.push_back(static_cast<std::size_t>(i.immediate));
        }
        auto code = compile(pcs, false, next_id_++);
        if (!code) return false;
        auto* ptr = code.get();
        method_code_[entry] = std::move(code);
        for (auto pc : pcs) method_pcs_[pc] = ptr;
        return true;
    }
    Execution run(const metavm::VMOptions& vm_options = {}) {
        Execution result;
        if (program_.empty() || !errors_.empty()) {
            result.status = metavm::Status::Trapped; result.diagnostics = errors_;
            if (result.diagnostics.empty()) result.diagnostics.push_back({"no program loaded", 0, {}});
            result.statistics = statistics_; result.compilation_diagnostics = compilation_errors_; return result;
        }
        detail::Runtime runtime{program_, vm_options, result, bus_, {}};
        detail::Frame f;
        f.max_steps = vm_options.max_steps; f.observe = vm_options.collect_trace; f.runtime = &runtime;
        std::optional<metavm::Trace> active;
        auto running = [&] { return f.status == static_cast<std::size_t>(metavm::Status::Running); };
        try {
            while (running()) {
                if (f.pc >= program_.size()) { f.status = static_cast<std::size_t>(metavm::Status::Halted); break; }
                if (f.steps >= f.max_steps) { f.status = static_cast<std::size_t>(metavm::Status::StepLimit); break; }
                if (!active && native_available() && options_.mode != Mode::Tracing && entries_.contains(f.pc) && !tried_methods_.contains(f.pc)) {
                    auto& count = calls_[f.pc];
                    if (count < std::max<std::size_t>(1, options_.method_threshold)) ++count;
                    if (count >= std::max<std::size_t>(1, options_.method_threshold)) compile_method(f.pc);
                }
                detail::Code* code = nullptr;
                if (!active) {
                    if (auto it = trace_code_.find(f.pc); it != trace_code_.end()) code = it->second.get();
                    else if (auto method = method_pcs_.find(f.pc); method != method_pcs_.end()) code = method->second;
                }
                if (code) {
                    bus_.emit({metavm::TraceEvent::Kind::NativeEntry, f.pc, code->id, code->trace ? "trace" : "method"});
                    const auto before = f.steps;
                    code->execute(f);
                    ++statistics_.native_entries; statistics_.native_instructions += f.steps-before;
                    if (code->trace && running()) ++statistics_.side_exits;
                    bus_.emit({metavm::TraceEvent::Kind::NativeExit, f.pc, code->id, {}});
                    continue;
                }
                const auto old_pc = f.pc;
                const auto op = program_.code[old_pc].op;
                if (active) active->pcs.push_back(old_pc);
                const auto before = f.steps;
                detail::interpret(f); statistics_.interpreted_instructions += f.steps-before;
                if (active && (!running() || f.pc == active->entry_pc || active->pcs.size() >= std::max<std::size_t>(1, vm_options.max_trace_length) || op == metail::Op::Call || op == metail::Op::Ret)) {
                    active->terminated_by_backedge = running() && f.pc == active->entry_pc;
                    auto trace = std::move(*active); active.reset();
                    traces_.push_back(trace); tried_traces_.insert(trace.entry_pc);
                    bus_.emit({metavm::TraceEvent::Kind::TraceCompleted, f.pc, trace.id, {}});
                    if (running() && native_available()) {
                        auto compiled = compile(trace.pcs, true, trace.id);
                        if (compiled) trace_code_[trace.entry_pc] = std::move(compiled);
                    }
                }
                const bool branch = op == metail::Op::Jmp || op == metail::Op::Jz || op == metail::Op::Jnz;
                if (running() && options_.mode != Mode::Method && branch && f.pc <= old_pc && f.pc < program_.size()) {
                    auto& count = loops_[f.pc];
                    const auto threshold = std::max<std::size_t>(1, vm_options.hot_threshold);
                    if (count < threshold) ++count;
                    if (!active && count >= threshold && traces_.size() < vm_options.max_traces && !tried_traces_.contains(f.pc)) {
                        active = metavm::Trace{next_id_++, f.pc, {}, false};
                        bus_.emit({metavm::TraceEvent::Kind::TraceStarted, f.pc, active->id, {}});
                    }
                }
            }
        } catch (const std::exception& e) { detail::trap(f, e.what()); }
        catch (...) { detail::trap(f, "unknown exception from JIT engine"); }
        if (active) {
            try { bus_.emit({metavm::TraceEvent::Kind::TraceAborted, f.pc, active->id, "execution ended while recording"}); }
            catch (const std::exception& e) { detail::trap(f, e.what()); }
            catch (...) { detail::trap(f, "unknown exception from trace observer"); }
        }
        // metail::run exposes the live Halt operand even if its dispatch was
        // prevented by the step budget or an instruction observer.
        if (f.pc < program_.size() && program_.code[f.pc].op == metail::Op::Halt)
            f.value = f.registers[program_.code[f.pc].a];
        result.status = static_cast<metavm::Status>(f.status); result.value = f.value; result.steps = f.steps;
        if (result.status == metavm::Status::StepLimit) result.diagnostics.push_back({"execution step limit exceeded", f.pc, {}});
        result.statistics = statistics_; result.compilation_diagnostics = compilation_errors_;
        return result;
    }
    Execution run(const metail::Program& program, const metavm::VMOptions& vm_options = {}, metail::Limits limits = {}) {
        load(program, limits); return run(vm_options);
    }
};
inline Execution run(const metail::Program& program, Options options = {},
        const metavm::VMOptions& vm_options = {}, metail::Limits limits = {}) {
    Engine engine(options); return engine.run(program, vm_options, limits);
}
} // namespace metajit
