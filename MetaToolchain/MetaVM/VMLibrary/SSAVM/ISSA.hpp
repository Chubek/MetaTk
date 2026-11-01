#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
namespace metavm { namespace machines {
/* Executable ISSA subset derived from TR 03-19 sections 2.1–2.4.
 * Result register = instruction index. Branches carry a CFG edge number.
 * PHIs stage values until PFE, preventing sequential-copy corruption.
 * Arrays are immutable versions (copy on update), not the paper's optimized
 * reference-count/access ordering implementation. */
class ISSA {
public:
    using Array=std::shared_ptr<const std::vector<int64_t>>;
    using Value=std::variant<int64_t,Array>;
    enum class Op { constant, argument, add, sub, mul, div, equal, less_equal,
                    jump, branch_equal, branch_le, phi, pfe, array, get, set, length, ret,
                    logical_and, logical_or, negate, branch_ne, branch_lt, branch_ge, branch_gt };
    struct Instruction {
        Op op; std::vector<size_t> operands;
        int64_t immediate=0; size_t target=0,edge=0;
    };
    struct Result { Value value; uint64_t steps; };
    // Independent ceilings. Zero is a literal limit, never "unlimited".
    // Allocation counters are cumulative per run, including SET's full copy;
    // array count also charges empty arrays. These are not allocator/RSS bytes.
    struct Limits {
        size_t max_source_bytes=16*1024*1024,max_line_bytes=128*1024,max_tokens_per_line=16384;
        size_t max_instructions=1000000,max_operands=16777216;
        size_t max_array_elements=1000000,max_allocated_elements=16777216,max_array_allocations=1000000;
        uint64_t max_steps=1000000;
    };
    struct VerificationError : std::runtime_error {
        size_t instruction;
        VerificationError(size_t index,const std::string&message)
            :std::runtime_error("ISSA instruction "+std::to_string(index)+": "+message),instruction(index){}
    };
    // Opt-in conservative dataflow analysis. Facts are separated by incoming CEN
    // at basic-block entries; conditional branches explore both successors.
    // Cells = instruction count * stored states; work is logical visits/copies,
    // not elapsed time. Zero is a literal ceiling. See docs/SSAVM.md.
    struct AnalysisLimits {
        size_t max_states=65536,max_cells=16777216;
        uint64_t max_work=67108864;
    };
    enum class ValueKind { none, scalar, array, either };
    struct Analysis {
        std::vector<bool> reachable;
        // Kinds produced at reachable instructions; none for control/unreachable.
        std::vector<ValueKind> results;
        ValueKind return_kind=ValueKind::none;
        size_t states=0;
        uint64_t work=0;
    };
    // Reject possible undefined reads, incompatible operand types and PHI CENs.
    // Does not prove termination, argument availability, arithmetic or bounds.
    // Borrows program. Instruction-local failures throw VerificationError;
    // program-size errors throw runtime_error. Allocation may throw bad_alloc.
    static Analysis analyze(const std::vector<Instruction>& program);
    static Analysis analyze(const std::vector<Instruction>& program,const Limits& limits,
                            const AnalysisLimits& analysis_limits);
    // Parse + analyze, retaining source-line diagnostics. Existing parse/verify/run
    // remain structural/runtime-only. Allocation failure may throw bad_alloc.
    static std::vector<Instruction> parse_checked(const std::string& source);
    static std::vector<Instruction> parse_checked(const std::string& source,const Limits& limits,
                                                 const AnalysisLimits& analysis_limits);
    // Numbered line-oriented ISSA subset. Errors include one-based source lines.
    static std::vector<Instruction> parse(const std::string& source);
    static std::vector<Instruction> parse(const std::string& source,const Limits& limits);
    static std::string format(const std::vector<Instruction>& program);
    static std::string format(const std::vector<Instruction>& program,const Limits& limits);
    static void verify(const std::vector<Instruction>& program);
    static void verify(const std::vector<Instruction>& program,const Limits& limits);
    static Result run(const std::vector<Instruction>& program,
                      const std::vector<int64_t>& args={},uint64_t fuel=1000000);
    static Result run_limited(const std::vector<Instruction>& program,const Limits& limits,
                               const std::vector<int64_t>& args={});
private:
    static std::vector<Instruction> parse_impl(const std::string&,const Limits&,const AnalysisLimits*);
};
}}
