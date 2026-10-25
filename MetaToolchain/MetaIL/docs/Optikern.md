# Optikern specification

Optikern kernels are executable Lua programs ending in `.optk` in `optimize-kernels/`. Their execution environment provides `lkopt`. Each file must register exactly one unique kernel, using:

- `lkopt.optimizer.new "Name"` — allocate and register a pass.
- `K:rewrite { pattern = "...", replace = "...", guard = "..." }` — structural bottom-up rewrite with captures `$name`.
- `K:analysis { kind = "...", ... }` — report data without mutating IR.
- `K:builtin "name"` — select a procedural pass implemented in the runtime.

## Captures and guards

Captures in a pattern bind entire IR expression trees; repeated captures require structural identity. Replacement captures must have already appeared in the pattern. Multiple guard clauses are separated with semicolons. Each guard is `kind:comma,separated,captures` (without `$`).

Available guards: `const`, `nonzero`, `positive`, `nonnegative`, `power-of-two`, `pure`, and `boolean`. `nonzero`, `positive`, `nonnegative`, and `power-of-two` currently require a **literal** integer, not an inferred fact. `boolean` requires literal 0 or 1. A rewrite can change the number of evaluations only when discarded/duplicated captures are pure. Source-to-source round-tripping is supported.

`pure` means free of observable effects **and traps**. Division/remainder with an
unknown divisor, zero divisor, or possible `INT64_MIN / -1` are not pure. Shifts
require a literal count in 0..63. Known `select` conditions inspect only the
evaluated arm. Malformed guards, unbound captures, unknown pass kinds, and no-op
rewrites are rejected when loading. A failed file or directory load rolls back
all registrations made by that load. Library tables are private to each kernel;
the environment exposes `lkopt.optimizer.new` and `lkopt.numeric`. Kernels are
trusted Lua programs, not a resource-isolated execution service.

## Analysis implementations

Operator counts are selected by `kind="operator-count", op="add"` etc. Statement counts use `kind="statement-count", statement="let"` etc. Other implementations:

`instruction-count`, `function-count`, `node-count`, `maximum-expression-depth`, `literal-histogram`, `variable-uses`, `definitions`, `unused-definitions`, `call-graph`, `impure-call-sites`, `cfg`, `reachable-blocks`, `unreachable-blocks`, `dominators`, `postdominators`, `liveness`, `reaching-definitions`, `natural-loops`, `critical-edges`, `cyclomatic-complexity`, `loop-headers`, `block-def-use`, `expression-redundancy`, `constant-branches`, `literal-range`, `parameter-uses`, `branch-targets`, `terminal-blocks`, `opcode-histogram`, `statement-histogram`, and `expression-purity`.

All analyses produce JSON and are evaluated against the actual parsed program. Examples of classic iterative dataflow analyses are liveness, reaching definitions, dominators, and postdominators. The CFG analyzer includes predecessor and successor lists and reachability. Dominators of unreachable blocks are not meaningful for optimization; consumers should filter reachability.

Unreachable predecessors do not contaminate reachable dominator or reaching-
definition results. Postdominators conservatively report only the node itself
for unreachable blocks or blocks with no path to an exit. Infinite successor
regions therefore do not make an exit appear to postdominate a conditional
branch into such a region. Natural self-loops exclude their entry predecessors.

## Procedural optimization implementations

`constant-folding`, `dead-assignments`, `constant-propagation`, `copy-propagation`, `simplify-branches`, `unreachable-code`, `common-subexpressions`.

Propagation and CSE are intrablock/conservative. Dead-assignment elimination preserves impure RHSs by converting unused assignments into `eval`. CFG branch simplification respects side effects when dropping branch conditions. Unreachable-block elimination follows explicit successor edges after branches are simplified.

## Kernel count / verification

The generator preserves the 200 base additions and adds 120 numeric kernels:
84 analyzers + 230 declarative rewrites + 7 procedural passes = **321 total**.
`python3 tools/generate_kernels.py --check` checks reproducibility.
`metail verify` exercises signed limits, mask/shift boundaries, two-capture
Cartesian products, deterministic full-width random vectors, effectful calls,
and trapping subexpressions. It compares values, trap reasons, and call traces,
including originally trapping cases, and confirms that every rule fires.

Numeric analyses use `K:analysis {kind="numeric-statistic", source="Literals",
statistic="Mean"}`. The supported sources/statistics and 96 numeric rewrite
families are documented in [MetaNum.md](MetaNum.md).
