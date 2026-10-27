# 29. Procedural transformations

Twelve registered kernels select procedural implementations:

| Kernel | Implementation |
|---|---|
| `ConstantFolding` | `constant-folding` |
| `DeadAssignmentElimination` | `dead-assignments` |
| `ConstantPropagation` | `constant-propagation` |
| `CopyPropagation` | `copy-propagation` |
| `BranchSimplification` | `simplify-branches` |
| `UnreachableCodeElimination` | `unreachable-code` |
| `CommonSubexpressionElimination` | `common-subexpressions` |
| `WeighstoneKnownBits` | `weighstone-known-bits` |
| `WeighstoneRanges` | `weighstone-ranges` |
| `WeighstoneStrengthReduction` | `weighstone-strength` |
| `WeighstoneShifts` | `weighstone-shifts` |
| `WeighstoneAlgebra` | `weighstone-algebra` |

These operate on function/statement structure and are implemented in
`lua/metail/transform.lua`. A kernel declaration selects one with
`K:builtin "constant-propagation"`; declaring an arbitrary unknown name is an error.

Propagation and common-subexpression elimination are conservative and intrablock.
They do not claim global SSA optimization. Assignments invalidate facts that
depend on overwritten variables, including self-referential assignments.

Dead-assignment elimination preserves an unused impure or potentially trapping
right-hand side as `eval`. Branch simplification handles constant conditions and
equal targets only when dropping the condition is safe. Unreachable-code removal
uses CFG edges after branch changes.

Procedural passes can expose opportunities for expression rewrites, and rewrites
can enable procedural cleanup. This motivates the bounded repeated pipeline.
When writing a new procedural transform, add the actual algorithm and regression
tests rather than registering a name with no computation.

The Weighstone passes share a block-local analysis of signed intervals, known
bits and low-bit congruences. Facts describe successful values; deleting an
expression additionally requires proof that it has no calls or possible traps.
Arithmetic ranges are wrapped to 64 bits before use. Overlapping comparison
ranges yield `unknown`, not a proof. Facts reset at labels and terminators;
assignment stores value facts rather than a symbolic expression depending on
mutable variables. See the [rewrite-family reference](../docs/Weighstone.md).

[Contents](README.md) · [Next: CFG analyses](30-cfg-analyses.md)
