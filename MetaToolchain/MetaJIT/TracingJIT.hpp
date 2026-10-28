#ifndef METAJIT_TRACING_JIT_HPP
#define METAJIT_TRACING_JIT_HPP
#include "OSR.hpp"
#include "Runtime.hpp"
#include <memory>

namespace metajit {
struct TraceOptions {
    std::uint64_t hot_threshold = 3;
    std::size_t max_trace_instructions = 256, max_traces = 64;
    std::size_t max_code_bytes = 8 * 1024 * 1024;
    bool native = true; // false selects the same reference interpreter exclusively
};
struct TraceStats {
    std::uint64_t interpreted = 0, native = 0, compiled = 0, osr_entries = 0;
    std::uint64_t guard_exits = 0, budget_exits = 0, trap_exits = 0;
};
/** Hot-loop path recorder and native trace compiler. Owns an immutable Program,
 * RX code and bounded profiling state. No external native pointer is exposed.
 * Host calls are interpreter boundaries, never speculated or duplicated.
 * Guards side-exit to exact materialized VM state; budget is instruction-exact.
 * Mutating methods require external synchronization and reject callback reentry.
 * Independent instances can run concurrently. Unsupported hosts use the VM.
 * Methods throw for bad input, allocation or OS failure; semantic traps are
 * reported in Frame. Native backend: Linux x86-64 System V only. */
class TracingJIT {
public:
    explicit TracingJIT(metavm::Program program, TraceOptions options = {});
    ~TracingJIT();
    TracingJIT(const TracingJIT &) = delete;
    TracingJIT &operator=(const TracingJIT &) = delete;
    const metavm::Program &program() const noexcept;
    const TraceStats &stats() const noexcept;
    static bool native_supported() noexcept;
    metavm::Stop run(metavm::Frame &, std::uint64_t budget, const metavm::Host &host = {});
    metavm::Frame execute(const std::vector<std::int64_t> &arguments,
                         std::uint64_t budget, const metavm::Host &host = {});
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace metajit
#endif
