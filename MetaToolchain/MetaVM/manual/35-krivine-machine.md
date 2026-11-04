# Chapter 35 — The Krivine weak-head profile

[Previous](34-secd-machine.md) · [Contents](README.md) · [Next](36-dvi-and-replay.md)

## Terms and programs

`KrivineMachine` implements closed, pure de Bruijn lambda terms with call-by-name
weak-head evaluation. `Program {version, terms, root}` uses explicit in-memory
version 1. `Term {op, a, b}` has three forms:

| Op | `a` | `b` |
|---|---|---|
| `variable` | Zero-based de Bruijn index | Zero |
| `abstraction` | Body term index | Zero |
| `application` | Function term index | Argument term index |

Children must precede their parent, so terms form an ordered DAG. Sharing is
allowed; forward references and cycles are rejected. Variable indices may not
equal `SIZE_MAX`. Verification checks all nodes, even ones outside the root's
reachable subgraph.

The selected root must be closed. Verification computes required environment
depth iteratively: variable requires index+1; abstraction removes one required
binding if present; application takes the maximum requirement of its children.
This is a term representation, not a historical binary format or textual
lambda parser.

## Transition semantics

Execution starts at the root with empty environment and pending-argument stack.

1. An application pushes the argument closure, retaining the current environment,
   then evaluates the function term.
2. An abstraction with a pending argument extends its own current environment
   with that unevaluated argument and enters the body.
3. Variable lookup skips the indexed number of lexical ancestors, then enters
   the selected closure's term **and saved environment**.
4. An abstraction with no pending arguments is a weak-head result and returns.

The machine does not reduce underneath that final abstraction. It does not
substitute all variables into a normalized syntax tree. Repeated argument
evaluation is call-by-name, not memoized call-by-need.

## Complete identity example

```cpp
using K = metavm::machines::KrivineMachine;
using O = K::Op;
K::Program program{1, {
    {O::variable,0},
    {O::abstraction,0},
    {O::application,1,1}
}, 2};
auto result = K::run(program);
if (result.value.term != 1) throw std::runtime_error("Krivine result mismatch");
```

The program represents `(lambda x.x) (lambda x.x)`. A selector can discard an
unevaluated divergent argument and terminate here, whereas an equivalent strict
SECD compilation may exhaust its budgets. That contrast is an intended semantic
test, not an engine-performance discrepancy.

## Results, budgets, and failure behavior

`Closure {term, environment}` indices refer to owned program/environment tables
in `Result`, alongside the step count. Flat environment cells contain a closure,
parent index, and depth; `empty_environment` is `SIZE_MAX`. Returning a closure
preserves its lexical captures after the caller's input program is destroyed.
There is no public resume/apply-result API.

`verify` and `run` have overloads taking Limits. Defaults are 1,000,000 terms,
65,536 pending arguments, 1,000,000 cumulative environment cells, depth 4096,
and 1,000,000 steps. Zero is literal; a bare identity can return without stack
or environment allocation. Applications, abstractions, variable-zero lookup,
each skipped ancestor, and final abstraction observation consume work steps.

Verification/evaluation and arena destruction avoid guest-directed host
recursion. Errors throw `std::runtime_error` with term context; allocation can
throw `std::bad_alloc`. There are no primitive numbers, effects, open-term
evaluation, control operators, full normalization, GC, or binary loader.

**References:** [header](../VMLibrary/Krivine/KrivineMachine.hpp), [closure-machine references](../docs/CLOSURE_VMS.md).
