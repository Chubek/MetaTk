# Chapter 29 — SSAVM execution and immutable arrays

[Previous](28-chip8-rom-validation.md) · [Contents](README.md) · [Next](30-ssavm-analysis.md)

## The implemented machine

`metavm::machines::ISSA` implements a bounded i64 scalar/immutable-array ISSA
subset based on TR 03-19 sections 2.1–2.4. It is not the paper's untyped
32-bit implementation, the complete appendix ISA, or a SafeTSA binary loader.
Programs are vectors of `Instruction {op, operands, immediate, target, edge}`.
The static instruction index identifies its result register, including indices
occupied by instructions that do not produce values.

`Value` is `variant<int64_t, Array>`, with `Array` a shared pointer to a const
vector of i64. `Result` owns its value and reports executed steps. Programs are
borrowed and not mutated; independent calls share no interpreter state.

## CEN, PHI, and simultaneous assignment

CEN is the current CFG edge number, initially zero. A taken branch assigns its
explicit edge; conditional fallthrough resets it to zero. Ordinary instructions
retain CEN. A PHI stages its operand selected by CEN. PFE commits the entire
group simultaneously and resets CEN.

Every PHI in a group reads old committed values, including another PHI's old
value. Sequentially overwriting PHI destinations would corrupt loop recurrences.
The supplied figure-5 Fibonacci program depends on this distinction and returns
55. Branching into the middle of a PHI/PFE group is structurally invalid.

Reading an undefined result traps. This machine's persistent instruction-result
registers are not a proof that the program meets conventional SSA dominance.

## Scalars and arrays

Arithmetic uses wrapping Machinery i64 semantics; comparisons and logical
AND/OR normalize to 0/1. Logical AND/OR are integer-nonzero tests, **not bitwise
operations**. Negation wraps, including `INT64_MIN`.

`array` creates a zero-filled immutable version. `set` copies the full vector
and changes one element; the old version stays intact. `get` and `length`
inspect versions. This preserves persistent operational behavior without the
paper's optimized reference-count/access-order representation.

## API and budgets

`run(program, args, fuel)` retains the original convenience API.
`run_limited(program, limits, args)` configures independent ceilings:

| Limit field | Default |
|---|---:|
| `max_source_bytes` | 16 MiB |
| `max_line_bytes` | 128 KiB |
| `max_tokens_per_line` | 16,384 |
| `max_instructions` | 1,000,000 |
| `max_operands` | 16,777,216 |
| `max_array_elements` | 1,000,000 |
| `max_allocated_elements` | 16,777,216 cumulative |
| `max_array_allocations` | 1,000,000 cumulative |
| `max_steps` | 1,000,000 |

Zero is a literal ceiling. Empty arrays still charge an allocation. Both
creation and SET charge the full new version's size; releasing old versions
does not refund cumulative budgets. These limits count logical resources,
not exact RSS. Fuel does not replace allocation limits.

```cpp
using I = metavm::machines::ISSA;
auto program = I::parse("const 42\nreturn (0)\n");
I::Limits limits;
limits.max_steps = 10;
auto result = I::run_limited(program, limits);
auto answer = std::get<int64_t>(result.value);
if (answer != 42) throw std::runtime_error("SSAVM result mismatch");
```

Check the variant kind when the program may return an array. Structural,
dynamic, and exhausted-limit errors throw; allocation may propagate
`std::bad_alloc`. No partial-success result is returned.

**References:** [ISSA.hpp](../VMLibrary/SSAVM/ISSA.hpp), [preserved paper](../docs/vm-ssa.pdf).
