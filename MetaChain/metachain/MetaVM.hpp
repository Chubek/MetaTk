#pragma once
#ifndef METAVM_HPP_INCLUDED
#define METAVM_HPP_INCLUDED

// MetaVM.hpp — header-only C++20 virtual-machine toolkit.
// Interpreter-first design with optional trace collection and a pluggable
// native-code JIT backend. No platform-specific executable-memory API is
// required by the core.
//
// The core intentionally does not emit machine code itself. Supply a JitBackend
// implementation to compile traces. This keeps interpreter-only builds portable.
//
// Example:
//   using namespace metavm;
//   enum class Op { Const, Add, Print, Halt };
//   struct Ins { Op op; std::int64_t a=0, b=0; };
//   struct Isa {
//     static constexpr std::size_t register_count = 8;
//     static bool execute(VM<Ins, Isa>& vm, const Ins& i) { ... }
//   };

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace metavm {

enum class Status {
    Ready,
    Running,
    Halted,
    Trapped,
    StepLimit
};

struct SourceLocation {
    std::string file;
    std::uint32_t line = 0;
    std::uint32_t column = 0;
};

struct Diagnostic {
    std::string message;
    std::size_t pc = 0;
    SourceLocation source{};
};

struct Value {
    enum class Kind { Empty, Integer, Floating, Boolean, Pointer };

    Kind kind = Kind::Empty;
    union Storage {
        std::int64_t integer;
        double floating;
        bool boolean;
        void* pointer;
        constexpr Storage() : integer(0) {}
    } data{};

    static Value integer(std::int64_t v) {
        Value x; x.kind = Kind::Integer; x.data.integer = v; return x;
    }
    static Value floating(double v) {
        Value x; x.kind = Kind::Floating; x.data.floating = v; return x;
    }
    static Value boolean(bool v) {
        Value x; x.kind = Kind::Boolean; x.data.boolean = v; return x;
    }
    static Value pointer(void* v) {
        Value x; x.kind = Kind::Pointer; x.data.pointer = v; return x;
    }

    std::int64_t as_integer() const {
        if (kind != Kind::Integer) throw std::runtime_error("Value is not an integer");
        return data.integer;
    }
    double as_floating() const {
        if (kind == Kind::Floating) return data.floating;
        if (kind == Kind::Integer) return static_cast<double>(data.integer);
        throw std::runtime_error("Value is not numeric");
    }
    bool as_boolean() const {
        if (kind != Kind::Boolean) throw std::runtime_error("Value is not a boolean");
        return data.boolean;
    }
};

struct TraceEvent {
    enum class Kind { Instruction, TraceStarted, TraceCompleted, TraceAborted,
                      CompileStarted, CompileCompleted, CompileFailed,
                      NativeEntry, NativeExit };
    Kind kind = Kind::Instruction;
    std::size_t pc = 0;
    std::size_t trace_id = 0;
    std::string message;
};

class TraceListener {
public:
    virtual ~TraceListener() = default;
    virtual void on_trace_event(const TraceEvent&) = 0;
};

class TraceBus {
public:
    using Callback = std::function<void(const TraceEvent&)>;

    void subscribe(TraceListener& listener) {
        listeners_.emplace_back([&listener](const TraceEvent& e) {
            listener.on_trace_event(e);
        });
    }
    void subscribe(Callback callback) { listeners_.push_back(std::move(callback)); }
    void clear() noexcept { listeners_.clear(); }
    std::size_t size() const noexcept { return listeners_.size(); }
    void emit(const TraceEvent& event) const {
        const auto snapshot = listeners_;
        for (const auto& callback : snapshot) callback(event);
    }

private:
    std::vector<Callback> listeners_;
};

template<class Instruction>
struct Program {
    std::vector<Instruction> code;
    std::vector<SourceLocation> source_map;
    std::string name;

    std::size_t size() const noexcept { return code.size(); }
    bool empty() const noexcept { return code.empty(); }

    const SourceLocation* source_at(std::size_t pc) const noexcept {
        return pc < source_map.size() ? &source_map[pc] : nullptr;
    }
};

struct VMOptions {
    std::size_t max_steps = 10'000'000;
    bool collect_trace = false;
    std::size_t hot_threshold = 64;
    std::size_t max_trace_length = 256;
    std::size_t max_stack_values = 1'000'000;
    std::size_t max_traces = 1024;
};

struct Trace {
    std::size_t id = 0;
    std::size_t entry_pc = 0;
    std::vector<std::size_t> pcs;
    bool terminated_by_backedge = false;
};

struct CompiledTrace {
    virtual ~CompiledTrace() = default;
    std::size_t trace_id = 0;
    std::size_t entry_pc = 0;
};

// Optional backend interface. A concrete backend owns its executable memory
// and is responsible for W^X policy, instruction-cache synchronization,
// ABI correctness, and lifetime of generated code.
template<class VMType, class Instruction>
class JitBackend {
public:
    using Compiled = CompiledTrace;
    virtual ~JitBackend() = default;

    virtual std::unique_ptr<Compiled> compile(
        VMType& vm, const Program<Instruction>& program,
        const Trace& trace, Diagnostic& diagnostic) = 0;

    // Return true if the compiled trace ran and selected a valid next PC.
    // The backend must not retain references to transient VM state.
    virtual bool execute(VMType& vm, const Compiled& compiled,
                         std::size_t& next_pc, Diagnostic& diagnostic) = 0;
};

template<class Instruction, class ISA>
class VM {
public:
    using instruction_type = Instruction;
    using isa_type = ISA;
    static constexpr std::size_t register_count = [] {
        if constexpr (requires { ISA::register_count; })
            return static_cast<std::size_t>(ISA::register_count);
        else
            return std::size_t{16};
    }();

    explicit VM(VMOptions options = {}) : options_(options) {}

    void load(Program<Instruction> program) {
        program_ = std::move(program);
        pc_ = 0;
        steps_ = 0;
        status_ = Status::Ready;
        diagnostic_.reset();
        stack_.clear();
        registers_.fill(Value{});
        loop_counts_.clear();
        traces_.clear();
        compiled_.clear();
        active_trace_.reset();
        pc_written_ = false;
    }

    const Program<Instruction>& program() const noexcept { return program_; }

    std::size_t pc() const noexcept { return pc_; }
    void set_pc(std::size_t pc) {
        if (pc > program_.size()) throw std::out_of_range("PC outside program");
        pc_ = pc;
        pc_written_ = true;
    }

    Status status() const noexcept { return status_; }
    std::size_t steps() const noexcept { return steps_; }
    const std::optional<Diagnostic>& diagnostic() const noexcept { return diagnostic_; }

    Value& reg(std::size_t index) { return registers_.at(index); }
    const Value& reg(std::size_t index) const { return registers_.at(index); }

    std::vector<Value>& stack() noexcept { return stack_; }
    const std::vector<Value>& stack() const noexcept { return stack_; }

    VMOptions& options() noexcept { return options_; }
    const VMOptions& options() const noexcept { return options_; }

    TraceBus& trace_bus() noexcept { return trace_bus_; }
    const TraceBus& trace_bus() const noexcept { return trace_bus_; }

    const std::vector<Trace>& traces() const noexcept { return traces_; }
    using OutputSink = std::function<void(const Value&)>;
    void set_output_sink(OutputSink sink) { output_sink_ = std::move(sink); }
    void output(const Value& value) { if (output_sink_) output_sink_(value); }

    void trap(std::string message) {
        Diagnostic d;
        d.message = std::move(message);
        d.pc = pc_;
        if (const auto* src = program_.source_at(pc_)) d.source = *src;
        diagnostic_ = std::move(d);
        status_ = Status::Trapped;
    }

    // ISA contract: static bool ISA::execute(VM&, const Instruction&).
    // Return false to stop execution; call vm.trap(...) for an error.
    // If ISA::execute returns true without calling set_pc, VM advances by one.
    // An explicit jump to the current PC is therefore a real self-loop.
    bool step() {
        if (status_ == Status::Halted || status_ == Status::Trapped ||
            status_ == Status::StepLimit)
            return false;

        if (pc_ >= program_.size()) {
            status_ = Status::Halted;
            return false;
        }

        if (steps_ >= options_.max_steps) {
            status_ = Status::StepLimit;
            return false;
        }

        status_ = Status::Running;
        const std::size_t old_pc = pc_;
        pc_written_ = false;

        try {
            if (options_.collect_trace) {
                trace_bus_.emit(TraceEvent{TraceEvent::Kind::Instruction, old_pc, 0, {}});
                // Observers must not drive the machine. A listener that traps
                // aborts the attempt before the instruction handler runs.
                if (status_ != Status::Running) return false;
                if (active_trace_) active_trace_->pcs.push_back(old_pc);
            }
            ++steps_; // attempted instructions count, including traps
            const bool keep_running = ISA::execute(*this, program_.code[old_pc]);

            if (status_ == Status::Trapped) return false;
            if (!keep_running) {
                if (status_ == Status::Running) status_ = Status::Halted;
                return false;
            }

            // Conventional instruction handlers may change PC for jumps.
            // Otherwise advance to the next instruction.
            if (!pc_written_) ++pc_;
            observe_control_flow(old_pc, pc_);

            if (pc_ >= program_.size() && status_ == Status::Running)
                status_ = Status::Halted;
            return status_ == Status::Running;
        } catch (const std::exception& e) {
            trap(e.what());
            return false;
        } catch (...) {
            trap("Unknown exception from instruction handler");
            return false;
        }
    }

    Status run() {
        if (status_ == Status::Trapped || status_ == Status::StepLimit)
            return status_;

        while (step()) {}
        return status_;
    }

    // Trace collection is passive: it records frequently visited backward
    // branch targets and the bytecode PCs executed from those targets.
    // It does not itself compile or execute native code.
    void observe_control_flow(std::size_t from, std::size_t to) {
        if (!options_.collect_trace) return;
        if (active_trace_ && (to == active_trace_->entry_pc ||
                active_trace_->pcs.size() >= std::max<std::size_t>(1, options_.max_trace_length))) {
            active_trace_->terminated_by_backedge = (to == active_trace_->entry_pc);
            traces_.push_back(*active_trace_);
            const auto id = active_trace_->id;
            active_trace_.reset();
            trace_bus_.emit(TraceEvent{TraceEvent::Kind::TraceCompleted, to, id, {}});
        }
        if (to > from || to >= program_.size()) return;
        auto& count = loop_counts_[to];
        ++count;
        if (count == std::max<std::size_t>(1, options_.hot_threshold) && !active_trace_ && traces_.size() < options_.max_traces) {
            Trace trace;
            trace.id = traces_.size() + 1;
            trace.entry_pc = to;
            active_trace_ = std::move(trace);
            trace_bus_.emit(TraceEvent{TraceEvent::Kind::TraceStarted,
                                       to, active_trace_->id, {}});
        }

    }

    template<class Backend>
    bool compile_trace(Backend& backend, std::size_t trace_index) {
        if (trace_index >= traces_.size()) return false;
        const Trace trace = traces_[trace_index];
        Diagnostic d;
        trace_bus_.emit(TraceEvent{TraceEvent::Kind::CompileStarted,
                                   trace.entry_pc, trace.id, {}});
        std::unique_ptr<CompiledTrace> compiled;
        try { compiled = backend.compile(*this, program_, trace, d); }
        catch (const std::exception& e) { d.message = e.what(); }
        catch (...) { d.message = "unknown exception from JIT compiler"; }
        if (!compiled) {
            trace_bus_.emit(TraceEvent{TraceEvent::Kind::CompileFailed,
                                       trace.entry_pc, trace.id, d.message});
            return false;
        }
        compiled->trace_id = trace.id;
        compiled->entry_pc = trace.entry_pc;
        compiled_[trace.entry_pc] = std::move(compiled);
        trace_bus_.emit(TraceEvent{TraceEvent::Kind::CompileCompleted,
                                   trace.entry_pc, trace.id, {}});
        return true;
    }

    template<class Backend>
    bool try_execute_compiled(Backend& backend) {
        if (status_ == Status::Halted || status_ == Status::Trapped || status_ == Status::StepLimit) return false;
        if (steps_ >= options_.max_steps) { status_ = Status::StepLimit; return false; }
        auto it = compiled_.find(pc_);
        if (it == compiled_.end()) return false;
        Diagnostic d;
        trace_bus_.emit(TraceEvent{TraceEvent::Kind::NativeEntry,
                                   pc_, it->second->trace_id, {}});
        std::size_t next_pc = pc_;
        bool executed = false;
        try { executed = backend.execute(*this, *it->second, next_pc, d); }
        catch (const std::exception& e) { trap(e.what()); return false; }
        catch (...) { trap("unknown exception from JIT backend"); return false; }
        if (status_ == Status::Trapped) return false;
        if (!executed) {
            trace_bus_.emit(TraceEvent{TraceEvent::Kind::TraceAborted,
                                       pc_, it->second->trace_id, d.message});
            return false;
        }
        if (next_pc > program_.size()) {
            trap("JIT backend returned a PC outside the program");
            return false;
        }
        pc_ = next_pc;
        ++steps_; // one native dispatch; backend must enforce its own instruction budget
        status_ = pc_ == program_.size() ? Status::Halted : Status::Running;
        trace_bus_.emit(TraceEvent{TraceEvent::Kind::NativeExit,
                                   pc_, it->second->trace_id, {}});
        return true;
    }

private:
    Program<Instruction> program_;
    VMOptions options_;
    std::size_t pc_ = 0;
    std::size_t steps_ = 0;
    Status status_ = Status::Ready;
    std::optional<Diagnostic> diagnostic_;
    std::array<Value, register_count> registers_{};
    std::vector<Value> stack_;
    TraceBus trace_bus_;
    std::unordered_map<std::size_t, std::size_t> loop_counts_;
    std::vector<Trace> traces_;
    std::unordered_map<std::size_t, std::unique_ptr<CompiledTrace>> compiled_;
    std::optional<Trace> active_trace_;
    bool pc_written_ = false;
    OutputSink output_sink_;
};

// A compact instruction-set helper. Users can specialize or implement their
// own ISA type with a static execute(VM&, Instruction) function.
template<class VMType, class Instruction>
using ExecuteFunction = bool (*)(VMType&, const Instruction&);

template<class Instruction>
class Builder {
public:
    Builder& emit(Instruction instruction, SourceLocation source = {}) {
        program_.code.push_back(std::move(instruction));
        program_.source_map.push_back(std::move(source));
        return *this;
    }

    Builder& name(std::string value) {
        program_.name = std::move(value);
        return *this;
    }

    Program<Instruction> finish() && { return std::move(program_); }

private:
    Program<Instruction> program_;
};

} // namespace metavm

#endif // METAVM_HPP_INCLUDED
