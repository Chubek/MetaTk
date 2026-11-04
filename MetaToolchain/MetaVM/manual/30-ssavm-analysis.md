# Chapter 30 — SSAVM text, verification, and analysis

[Previous](29-ssavm-execution.md) · [Contents](README.md) · [Next](31-drmvm-graphics.md)

## Text syntax reference

Instructions occupy separate lines. Optional explicit numbers must be
consecutive from zero. `(index)` refers to a result; `[index] edge` names a
branch destination and CEN. Comments start with `#`/`;`. Literals are decimal
i64; indices are nonnegative. Blank/comment lines retain their place in source
diagnostics.

| Text | Operands |
|---|---|
| `const` | Immediate |
| `int_arg` | Optional host-argument index |
| `add/sub/mul/div/and/or/eq/le` | Two references |
| `neg`, `length`, `return` | One reference |
| `beq/bne/blt/ble/bge/bgt` | Two references, target, edge |
| `goto` | Target, edge |
| `phi` | One or more references |
| `pfe` | None |
| `array` | Length reference |
| `get` | Array/index references |
| `set` | Array/index/value references |

Accepted aliases include `iadd/isub/imul/idiv` and `newarray/access/update`.
An omitted `int_arg` index uses its argument-instruction ordinal. `format`
emits canonical spellings, explicit argument indices, and instruction numbers.

## Structural checking versus opt-in analysis

`parse`, `verify`, and `format` check structure and configured program shape,
including all unreachable instructions, reference bounds/kinds, final
fallthrough, and PHI entry/group rules. They do not by themselves prove that
a reachable referenced result was assigned or has the required scalar/array
kind. Runtime checks remain in place.

`analyze(program)` adds a bounded forward definite-assignment/type analysis.
States are distinguished by **basic block and incoming CEN**, not just PC.
Facts merge possible undefined/scalar/array kinds to a fixed point. Conditional
branches explore both successors even for constant conditions. PHIs read old
facts and commit together at PFE.

Analysis rejects a reachable operation that may read undefined data, consume
an incompatible kind, or select a PHI operand outside its CEN range. Unselected
PHI alternatives still receive structural checks but are not wrongly read as
executed values. Sparse CEN keys avoid allocating an array sized by the largest
edge number.

## APIs and returned facts

`parse_checked(source, limits, analysis_limits)` parses and analyzes while
preserving original source-line diagnostics. `analyze` returns owned
`reachable`, `results`, `return_kind`, `states`, and `work` fields. Result kinds
are `none`, `scalar`, `array`, and `either`; `none` denotes unreachable/no result,
not an initialized runtime nil. `VerificationError::instruction` identifies
instruction-local analysis failures.

Independent defaults are `max_states=65536`, `max_cells=16777216`, and
`max_work=67108864`. Cells account for instruction-count × stored states;
work counts logical visits/copies/merges, not cycles or elapsed time. Zero is a
literal ceiling. Exhaustion fails explicitly; no partial certificate is returned.

## Limits of the conclusion

Successful analysis is not conventional SSA dominance certification, a value-range
proof, an argument-availability guarantee, or termination proof. It does not prove
nonzero divisors, array sizes/indices, allocation success, or sufficient fuel.
Conservative path joins can reject a safe program. Runtime checks are never
disabled because analysis succeeded.

```sh
out-manual/ssavm-run --check --max-analysis-states 4096 \
  --max-analysis-cells 1048576 --max-analysis-work 8388608 \
  VMLibrary/SSAVM/fibonacci.ssa
```

Check mode accepts no execution arguments and prints `verified` on success.
It can accept an infinite structurally/type-correct loop. Normal execution
still needs chapter 29's independent budgets.

**References:** [full SSAVM contract](../docs/SSAVM.md), [analysis implementation](../VMLibrary/SSAVM/ISSAAnalysis.cpp).
