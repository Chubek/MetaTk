# SSAVM executable subset

`VMLibrary/SSAVM/ISSA.hpp` implements a bounded **i64, scalar-array ISSA subset**.
Its CEN and simultaneous PHI/PFE model follows TR 03-19 §§2.1–2.4, preserved
in `vm-ssa.pdf`. This is not the paper's untyped 32-bit implementation, its full
instruction set, or a SafeTSA loader. An opt-in definite-assignment/type analyzer
is available for this subset. Calls, floats, conventional SSA-form dominance
certification and value-range analysis remain future work.

## Instructions and text

Results are named by zero-based instruction indices, including instructions that
do not produce a value. References use `(index)`. Optional instruction numbers
must be consecutive. Branches spell their destinations `[index] edge`.
Decimal literals are signed i64; indices are nonnegative. Blank lines and `#`/`;`
comments are accepted. Syntax and verification errors report original lines.

| Text | Operands | Semantics |
|---|---|---|
| `const` | immediate | i64 constant |
| `int_arg` | optional argument index | scalar argument; omitted index uses argument-instruction ordinal |
| `add/sub/mul/div` | two references | Machinery wrapping i64 arithmetic; divide-by-zero traps |
| `and/or` | two references | logical integer nonzero tests, normalized to 0/1; both inputs must be scalars |
| `neg` | one reference | wrapping i64 negation; `INT64_MIN` remains `INT64_MIN` |
| `eq/le` | two references | signed comparison, normalized to 0/1 |
| `beq/bne/blt/ble/bge/bgt` | two references, target, edge | signed conditional branch |
| `goto` | target, edge | unconditional branch |
| `phi` | one or more references | stage the operand selected by CEN |
| `pfe` | none | commit the entire PHI group simultaneously; reset CEN |
| `array` | length reference | zero-filled immutable array version |
| `get` | array and index references | scalar array element |
| `set` | array, index, value references | full array copy with one element replaced |
| `length` | array reference | scalar element count |
| `return` | one reference | return scalar or owned shared immutable array |

`iadd/isub/imul/idiv` and the paper's `newarray/access/update` are accepted aliases.
The formatter emits the canonical names in the table, explicit argument indices,
and consecutive instruction numbers. The added logical operations, negation and
four conditional branches follow appendix A.1 (printed pages 23–25). Logical
AND/OR are intentionally **not bitwise**.

CEN initially equals zero. A taken branch sets its explicit edge; a not-taken
conditional resets CEN to zero, preserving this subset's existing behavior.
Other instructions preserve CEN until PFE. PHIs read previous committed values,
including other PHIs in the same group, before any group writes become visible.
This preserves the figure-5 Fibonacci result of 55 and persistent array versions.

The verifier checks all instructions, even unreachable ones: opcode, arity,
reference and branch bounds, references to non-value-producing instructions,
nonnegative argument indices, final fallthrough and PHI-group structure/entry.
Undefined values, operand types, argument availability, CEN arity and array bounds
are also checked during execution. Structural verification alone does not imply
definite assignment or type safety; use the opt-in analysis below for those checks.

## Limits and API

The original `parse(source)`, `format(program)`, `verify(program)` and
`run(program, args, fuel)` APIs remain. New parse/format/verify overloads accept
`const ISSA::Limits&`; `run_limited(program, limits, args)` accepts the same limits.
Programs are borrowed and never mutated. Results own their array values through
shared pointers and outlive the run. Calls have no shared mutable interpreter
state; callers must not mutate a program during use. Errors are exceptions;
allocation failure may propagate `std::bad_alloc`.

| Limit | Default |
|---|---:|
| Source bytes | 16 MiB |
| Line bytes (before removing comments) | 128 KiB |
| Tokens per line | 16,384 |
| Instructions/result-register slots | 1,000,000 |
| Total operand references | 16,777,216 |
| Elements in one array | 1,000,000 |
| Cumulatively allocated elements per run | 16,777,216 |
| Array allocations per run | 1,000,000 |
| Executed instructions | 1,000,000 |

Zero is a literal ceiling, not unlimited. Array allocation count charges even
empty arrays. Both `array` and `set` charge the full new version's elements and
one allocation. Releasing old versions does not refund these cumulative budgets.
Arithmetic checks subtract remaining capacity before adding usage. Limits count
logical resources, not allocator overhead/RSS. Instruction and operand ceilings
independently bound the execution tables and caller-provided program shape;
source/line/token ceilings apply to parsing. The formatter verifies program
limits but does not apply source/line/token input limits to its output.

```cpp
using I = metavm::machines::ISSA;
I::Limits limits;
limits.max_array_elements = 1024;
limits.max_allocated_elements = 8192;
limits.max_array_allocations = 16;
limits.max_steps = 10000;
auto program = I::parse("const 42\nreturn (0)\n", limits);
auto result = I::run_limited(program, limits);
```

The CLI exposes execution/allocation limits before the filename:

```sh
build/ssavm-run --max-steps 10000 --max-array-elements 1024 \
  --max-total-elements 8192 --max-arrays 16 VMLibrary/SSAVM/fibonacci.ssa
```

`--` ends options; integers after the filename are guest arguments, including
negative values. Exit status is 0 on success, 1 on input/runtime failure and 2
on invalid command-line options. Regression coverage includes all six branches
with both outcomes, i64 edges, source and allocation ceilings, figure-5 PHIs,
and differential factorial/array-Fibonacci adaptations of appendix B.1/B.3.

## Opt-in static analysis

`ISSA::analyze(program)` first performs structural verification, then a bounded
forward dataflow analysis from instruction zero with CEN zero and all result
registers undefined. `analyze(program, limits, analysis_limits)` accepts separate
program and analysis ceilings. Both borrow the instruction vector and return an
owned `Analysis`; no program or interpreter state is changed. Concurrent analysis
of an immutable program is supported. No global mutable analysis state exists.

The analysis rejects a reachable operation if:

- An operand may be undefined on any incoming abstract path.
- Its possible value kinds include a kind the operation cannot consume.
- A reachable CEN selects beyond a PHI's operand list.

Arithmetic, comparisons, logical operations, lengths and array elements produce
i64 scalars. Branches consume scalars without producing a result.
Array creation/update produce arrays. PHIs copy the selected
operand's kind; returns accept either kind. Array indices/lengths/elements must
be scalar. These are this interpreter's tagged i64/array types, not the paper's
untyped 32-bit words.

Basic-block entry states are distinguished by **(block, incoming CEN)**. Facts
are may-sets of undefined/scalar/array, joined by union and propagated to a fixed
point. Overwriting a result replaces its fact for that path. Conditional branches
explore both successors; taken edges set their explicit CEN and fallthrough sets
zero, matching this subset's existing semantics. Ordinary instructions retain
CEN. Each PHI group reads the old facts and commits all selected facts together
at PFE, then resets CEN. This is essential for figure 5's old-value dependency.
Only reachable PHI alternatives are read; unused alternatives still undergo the
structural reference checks.

`Analysis` contains:

| Field | Meaning |
|---|---|
| `reachable` | Per-instruction conservative reachability, including PHIs and PFE |
| `results` | Per-instruction `ValueKind::{none,scalar,array,either}` |
| `return_kind` | Union of kinds at reachable returns; `none` if no return is reachable |
| `states` | Number of distinct block/CEN entry states stored |
| `work` | Charged logical analysis work |

`none` in `results` means unreachable or no value produced, not an undefined
runtime value. A PHI may produce `either` and be returned safely; consuming it in
a scalar-only or array-only operation fails unless the relevant entry-state facts
establish the required kind. Errors use `VerificationError::instruction` and
zero-based operand/reference indices in the message. No partial report is returned.

`parse_checked(source)` and `parse_checked(source, limits, analysis_limits)` parse
and analyze before returning the ordinary instruction vector. Their diagnostics
retain original one-based source lines, including comments/blank lines. Existing
`parse`, `verify`, `format`, `run`, and `run_limited` retain their previous behavior;
runtime checks remain enabled even for analyzed programs.

```cpp
using I = metavm::machines::ISSA;
I::Limits limits;
I::AnalysisLimits analysis_limits;
analysis_limits.max_cells = 1024 * 1024;
auto program = I::parse_checked(source, limits, analysis_limits);
auto report = I::analyze(program, limits, analysis_limits);
auto result = I::run_limited(program, limits, arguments);
```

### Analysis limits and precision

| Ceiling | Default |
|---|---:|
| `max_states` (block/CEN pairs) | 65,536 |
| `max_cells` (instructions × stored states) | 16,777,216 |
| `max_work` (logical analysis work) | 67,108,864 |

Zero is a literal ceiling. State/cell budgets are checked before adding a fact
table; multiplication is checked using division. CEN values are sparse keys, so a
large edge number does not allocate an edge-indexed array. Work charges each
instruction/selected operand visit and each fact-table initialization, copy or
merge; setup and summary scans are also charged. Structural verification is
separately bounded by `Limits::max_instructions/max_operands`. Work units are not
CPU cycles or a wall-clock deadline, and may change with the analysis algorithm.

Stored facts use one byte per cell plus state/container overhead. Temporary fact
and PHI buffers, block maps and result summaries add O(instructions) memory.
These are logical ceilings, not exact RSS accounting; allocation failure can
still propagate `std::bad_alloc`. A straight-line program uses one state rather
than one table per instruction. Highly connected programs can still exhaust the
cell/work budget and fail explicitly.

Successful analysis establishes defined, compatible operands and valid PHI
selections for the unchanged program's reachable execution paths, assuming the
interpreter semantics described here. It does **not** establish termination,
presence of host arguments, nonzero divisors, valid array sizes/indices, allocation
success or sufficient execution fuel. Runtime traps remain possible. Both arms
of even a constant condition are explored; predicate/value correlations can cause
conservative rejection. Unreachable code is structurally checked but not type-
checked. The result is definite-assignment analysis of the interpreter's persistent
result registers, not a certificate of conventional SSA dominance or SafeTSA form.

### Check-only CLI

```sh
build/ssavm-run --check --max-analysis-states 4096 \
  --max-analysis-cells 1048576 --max-analysis-work 8388608 \
  VMLibrary/SSAVM/fibonacci.ssa
# verified
```

`--check` analyzes without executing and accepts no guest arguments. It can verify
an infinite loop; execution fuel/allocation limits are not proof obligations.
Analysis-limit options affect check mode only. Exit status is 0 on success,
1 on parse/analysis/input failure, and 2 on option/usage errors.
