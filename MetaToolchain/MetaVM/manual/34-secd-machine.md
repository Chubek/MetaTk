# Chapter 34 — The SECD closure-machine profile

[Previous](33-forth-subset.md) · [Contents](README.md) · [Next](35-krivine-machine.md)

## Explicit in-memory format version 1

`SECDMachine` implements a unary/i64 lexical-closure subset. A `Program`
contains `version=1`, a vector of instruction-vector `blocks`, and an entry
block index. `Instruction` is `{op, immediate, a, b}`. A Value is either i64
or `Closure {block, environment}`. This is MetaVM's declared representation,
not a Lispkit binary loader or a guessed universal SECD opcode encoding.

Every block is nonempty and ends with exactly one RET, JOIN, or STOP; those
terminators may not appear earlier. Verification checks all blocks, versions,
opcode/operand fields and references, including unused code. Unused operands
must be zero. Stack/type properties are checked dynamically, not proved by
a complete static SECD type system.

## Instruction reference

| Op | Operands and behavior |
|---|---|
| `constant` | Push immediate i64 |
| `load` | `a`: lexical depth; push binding, depth zero is current argument |
| `closure` | `a`: code block; capture current environment |
| `apply` | Consume `[..., argument, closure]`, enter closure with unary binding |
| `ret` | Return exactly one active result through a call continuation |
| `select` | `a`: true block, `b`: false block; pop integer condition |
| `join` | Restore matching selection continuation with one net extra value |
| `add/sub/mul/div` | Binary i64 arithmetic |
| `equal/less_equal` | Signed comparison, normalized integer 0/1 |
| `stop` | Return one value with an empty dump |

Integer zero is false for selection, unlike Machinery truthiness. Arithmetic
wraps i64 with the documented division-by-zero and minimum-integer edge rules.

## Lexical capture and the typed dump

AP extends the **closure's captured environment**, not the caller's environment.
Arguments are already evaluated, so this is strict/call-by-value application.
The callee starts a fresh active stack; saved caller values remain in the
aggregate stack budget. RET restores caller control, environment, and stack base.

SEL shares the current stack/environment with an arm and saves a selection
continuation. JOIN requires that continuation kind, unchanged environment/base,
and one net extra stack value. RET cannot consume a selection continuation,
and JOIN cannot consume a call continuation. These distinctions prevent nested
control flow from accidentally restoring the wrong machine state.

## Complete native example

```cpp
using S = metavm::machines::SECDMachine;
using O = S::Op;
S::Program program{1, {
    {{O::constant,42}, {O::closure,0,1}, {O::apply}, {O::stop}},
    {{O::load}, {O::ret}}
}, 0};
auto result = S::run(program);
if (std::get<int64_t>(result.value) != 42)
    throw std::runtime_error("SECD result mismatch");
```

## Ownership, limits, and exclusions

`verify`/`run` accept optional `Limits`. Result owns a program snapshot, value,
flat environment arena, and step count. Closure indices refer into that owned
snapshot; `empty_environment` is `SIZE_MAX`. Results support inspection, not
a resume/apply-result interface. Immutable input can be used concurrently.

Defaults: 65,536 blocks; 1,000,000 instructions; 65,536 aggregate stack values;
65,536 dump entries; 1,000,000 cumulative environment cells; depth 4096;
1,000,000 steps. Zero is a literal ceiling. Instructions and skipped lexical
ancestors consume steps. Append-only flat arenas avoid recursive ownership
destruction but are not garbage-collected. Logical budgets are not exact RSS.

Errors include block/instruction context and throw `std::runtime_error`;
allocation may throw `std::bad_alloc`. Lists, NIL, DUM/RAP, mutable recursive
environments, I/O, GC, and historical images remain absent.

**References:** [header](../VMLibrary/SECD/SECDMachine.hpp), [semantic sources](../docs/CLOSURE_VMS.md).
