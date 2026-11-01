#pragma once
#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace metavm { namespace machines {
// MetaVM SECD unary/i64 profile v1, not a Lispkit binary format. See docs/CLOSURE_VMS.md.
class SECDMachine {
public:
    static constexpr size_t empty_environment=SIZE_MAX;
    enum class Op { constant, load, closure, apply, ret, select, join,
                    add, sub, mul, div, equal, less_equal, stop };
    struct Instruction {Op op;int64_t immediate=0;size_t a=0,b=0;};
    struct Program {uint32_t version=1;std::vector<std::vector<Instruction>> blocks;size_t entry=0;};
    struct Closure {size_t block,environment;};
    using Value=std::variant<int64_t,Closure>;
    struct Environment {Value value;size_t parent,depth;};
    struct Limits {
        size_t max_blocks=65536,max_instructions=1000000,max_stack_values=65536,max_dump=65536;
        size_t max_environment_cells=1000000,max_environment_depth=4096;
        uint64_t max_steps=1000000;
    };
    // Owned snapshot: closure block/environment indices refer into these tables.
    // Flat index arenas avoid recursive ownership/destruction; no resume API.
    struct Result {Program program;Value value;std::vector<Environment> environments;uint64_t steps;};
    // Borrow immutable inputs; no shared mutable state. Errors are runtime_error
    // with block/instruction location; allocation failure may throw bad_alloc.
    static void verify(const Program&);
    static void verify(const Program&,const Limits&);
    static Result run(const Program&);
    static Result run(const Program&,const Limits&);
};
}}
