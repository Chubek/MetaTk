#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace metavm { namespace machines {
// Closed, pure de Bruijn lambda terms, weak-head call-by-name profile v1.
// Terms form an ordered DAG: child indices precede their parent. No binary loader.
class KrivineMachine {
public:
    static constexpr size_t empty_environment=SIZE_MAX;
    enum class Op { variable, abstraction, application };
    // variable: a=index, b=0; abstraction: a=body,b=0; application: a=function,b=argument.
    struct Term {Op op;size_t a=0,b=0;};
    struct Program {uint32_t version=1;std::vector<Term> terms;size_t root=0;};
    struct Closure {size_t term,environment;};
    struct Environment {Closure value;size_t parent,depth;};
    struct Limits {
        size_t max_terms=1000000,max_stack=65536,max_environment_cells=1000000,max_environment_depth=4096;
        uint64_t max_steps=1000000;
    };
    // Owns all term/environment data needed to interpret the returned abstraction.
    // No substitution, full normalization or resume API is implied.
    struct Result {Program program;Closure value;std::vector<Environment> environments;uint64_t steps;};
    // All structural nodes are checked; the root must be closed. Iterative APIs
    // borrow immutable inputs and have no shared mutable state. Errors include
    // term indices (runtime_error); allocation failure may throw bad_alloc.
    static void verify(const Program&);
    static void verify(const Program&,const Limits&);
    static Result run(const Program&);
    static Result run(const Program&,const Limits&);
};
}}
