# SECD and Krivine executable profiles

Two additional catalog entries now execute native abstract-machine programs:
`SECDMachine` and `KrivineMachine`, in their respective `VMLibrary` directories.
Both are dependency-free C++17 APIs with explicitly defined **MetaVM in-memory
profile version 1**. They are not binary decoders or full historical language
runtimes. Catalog C factories and `.mvm`/`.vmweave` companions remain portable
Machinery examples; their result of 42 does not exercise these native APIs.

## Semantic sources and profile boundaries

- P. J. Landin, *The Mechanical Evaluation of Expressions*, The Computer Journal
  6(4), 308–320 (1964), [DOI](https://doi.org/10.1093/comjnl/6.4.308).
  The stack/environment/control/dump model and lexical closure application are
  summarized by the [SECD instruction reference](https://en.wikipedia.org/w/index.php?title=SECD_machine&oldid=1370767946).
- The de Bruijn **App / Abs / Zero / Succ** rules in the
  [Krivine transition reference](https://en.wikipedia.org/w/index.php?title=Krivine_machine&oldid=1352912226)
  (following Curien's presentation) specify the call-by-name machine used here.
  See also J.-L. Krivine, *A call-by-name lambda-calculus machine*, Higher-Order
  and Symbolic Computation 20, 199–207 (2007),
  [DOI](https://doi.org/10.1007/s10990-007-9018-9).

The local choices below are deliberate profile definitions, not claims of a
universal opcode encoding: unary SECD arguments rather than parameter lists,
zero-based lexical addresses, i64 numeric operations, integer conditions,
indexed control blocks, and ordered term DAGs. No historical byte values are
invented. Unknown versions/opcodes and malformed operands are rejected.

## SECD unary/i64 profile v1

Header: `VMLibrary/SECD/SECDMachine.hpp` (also included by `SECD.hpp`). A `Program`
contains `version`, a vector of instruction-vector `blocks`, and an `entry` block
index. Every block is nonempty and ends with exactly one RET, JOIN or STOP. Those
terminators cannot occur earlier in the block. All block references and opcode
values are checked, including in unreachable blocks; unused operands must be zero.
There is no static stack/type proof; execution checks dynamic uses.

`Instruction {op, immediate, a, b}` has the following semantics. The rightmost
stack value is the top; ordinary instructions advance within their block.

| `Op` | Operands | Effect |
|---|---|---|
| `constant` | immediate i64 | Push the integer |
| `load` | a = lexical depth | Push binding at depth a; zero is current unary argument |
| `closure` | a = block | Push code block and current lexical environment |
| `apply` | none | Consume `[..., argument, closure]`; create unary binding and call |
| `ret` | none | Return exactly one active-stack value through a call continuation |
| `select` | a = true block, b = false block | Pop integer condition; save selection continuation; run chosen arm |
| `join` | none | Resume selection continuation with exactly one net additional stack value |
| `add/sub/mul/div` | none | Replace `[..., lhs, rhs]` with i64 result |
| `equal/less_equal` | none | Signed integer comparison, normalized to 0 or 1 |
| `stop` | none | Return exactly one value with an empty dump |

AP extends the **closure's captured environment**, not the caller's environment.
The callee starts with an empty active stack. Caller stack values remain saved
below the active stack base and count toward the aggregate stack limit. RET
restores caller environment/control/base and leaves its result on that stack.
SEL's arms share the current stack/environment. JOIN requires the matching dump
kind, unchanged environment/base and a net stack increase of one. RET cannot
consume a selection continuation; JOIN cannot consume a call continuation.

Arguments are evaluated before AP (strict/call-by-value); callers emit their
evaluation order explicitly. SELECT treats integer zero as false and every other
integer as true, unlike Machinery's truthy integer zero. Arithmetic uses
`mvm_integer_eval`: add/subtract/multiply wrap i64, division truncates toward zero,
`INT64_MIN / -1` wraps to `INT64_MIN`, and division by zero traps.

```cpp
using S = metavm::machines::SECDMachine;
using O = S::Op;
S::Program p{1, {
    {{O::constant,42}, {O::closure,0,1}, {O::apply}, {O::stop}},
    {{O::load}, {O::ret}}
}, 0};
auto result = S::run(p);
// std::get<int64_t>(result.value) == 42
```

Values are integers or `Closure {block, environment}`. The result owns a copy of
the program and its flat `Environment` arena. Parent indices use
`empty_environment == SIZE_MAX` for the empty environment; cells store a binding,
parent and depth. Escaping closures retain captured bindings after calls return.
Lists, NIL, DUM/RAP, mutable recursive environments, I/O and Lispkit binary formats
are absent. Self-application remains possible, so execution can diverge.

## Krivine closed-term profile v1

Header: `VMLibrary/Krivine/KrivineMachine.hpp` (also included by `Krivine.hpp`).
`Program {version, terms, root}` holds an ordered DAG of `Term {op, a, b}`:

| `Op` | a | b |
|---|---|---|
| `variable` | zero-based de Bruijn index | zero |
| `abstraction` | body node index | zero |
| `application` | function node index | argument node index |

Children must strictly precede their parent. Sharing is allowed; cyclic ASTs and
forward references are rejected. Variable indices may not equal SIZE_MAX.
Verification iteratively computes required environment depth for each node:
variable = index+1; abstraction = max(body requirement−1,0); application = max of
child requirements. The selected root must require zero bindings (be closed).
All nodes undergo structural validation even when not reachable from the root.

Execution starts with the root, empty environment and empty argument stack:

1. Application pushes the unevaluated argument with its environment, then enters
   the function with the current environment.
2. An abstraction with an argument extends its current environment by that argument
   closure, then enters the body. With no arguments it returns immediately.
3. Variable lookup skips the specified number of environment cells, then enters
   the selected closure's term **and saved environment**.

This is weak-head call-by-name, not call-by-need: there is no memoization and no
reduction beneath an abstraction with an empty argument stack. The result is an
abstraction closure, not a substituted or fully normalized term. It owns its
program copy and environment arena, so captured free variables of the returned
abstraction can be inspected even after the input program is destroyed.

```cpp
using K = metavm::machines::KrivineMachine;
using O = K::Op;
K::Program p{1, {{O::variable,0}, {O::abstraction,0},
                 {O::application,1,1}}, 2}; // (lambda x.x) (lambda x.x)
auto result = K::run(p);
// result.value.term == 1, the identity abstraction
```

There are no primitive integers, effects, open-term evaluation, control operators,
full normalization or external format loader in this profile.

## Limits, ownership and errors

Both APIs provide `verify(program)`, `verify(program, limits)`, `run(program)` and
`run(program, limits)`. Inputs are borrowed without mutation; concurrent use of
immutable programs is supported. Results own their data, including code. Result
snapshots are for inspection; there is no resume/apply-result API. Invalid input,
dynamic traps and exhausted limits throw `std::runtime_error`, with block/PC or
term location for instruction-specific failures. Allocation failure may propagate
`std::bad_alloc`. There is no partial-success result.

| Limit | SECD default | Krivine default |
|---|---:|---:|
| Code blocks | 65,536 | — |
| Total instructions / terms | 1,000,000 | 1,000,000 |
| Stack values (SECD includes saved callers) / pending arguments | 65,536 | 65,536 |
| Dump continuations | 65,536 | — |
| Cumulatively allocated environment cells | 1,000,000 | 1,000,000 |
| Depth of any environment chain | 4,096 | 4,096 |
| Execution steps | 1,000,000 | 1,000,000 |

Zero is a literal ceiling. Unused resources need not be allocated: a bare Krivine
identity can return with zero argument-stack/environment capacity. SECD charges
one step per executed instruction plus each skipped lexical ancestor. Krivine
charges application, abstraction, variable-zero lookup, each skipped ancestor,
and the final abstraction observation. Verification is iterative and bounded by
code counts, independently of execution fuel.

Environment arenas are append-only and are not garbage-collected; the cumulative
cell ceiling bounds retained storage. Code, stacks, dump, cells and depth are
bounded independently. These are logical limits, not exact RSS limits: containers,
verification tables and the result's program copy add overhead. Flat indices avoid
recursive host evaluation and recursive ownership destruction, even for divergent
programs or deeply nested syntax.

`closure-vm-demo` (source `driver/closure_demo.cpp`) runs an escaping SECD adder
closure and Krivine identity application. Expected output:
`secd=42 krivine-term=1`. It is a host API example, not an input-language parser.
`tests/closure_vms.cpp` covers lexical capture/shadowing, strict versus lazy
behavior, typed dump restoration, i64 edges, malformed programs, exact resource
boundaries and divergence. A 64-program terminating selector family is compiled
to SECD and compared with Krivine and known results. Deep syntax exercises
iterative verification/destruction. Both native APIs are tested after SDK relocation.
