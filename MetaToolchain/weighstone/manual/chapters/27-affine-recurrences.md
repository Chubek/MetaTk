# Chapter 27: Affine Recurrences

**Domain:** loops · **Status:** implemented · **Primary reference:** `include/Weighstone/CCINT/Recurrence.hpp`

## Problem and model

AffineRecurrence describes x(n)=initial+step*n over nonnegative iteration indices. Exact arithmetic means long-running loops do not overflow in the model.

At every compiler optimization boundary it is necessary to identify what is known about the input program, which arithmetic universe is being modeled, and whether the facts have been inferred conservatively. A result is useful only insofar as it remains true for every execution admitted by those assumptions. This chapter describes this issue in the context of **affine recurrences**.

## Method and API contract

Use this form only after proving that an SSA induction value follows the same recurrence.

When implementing or extending the API, document preconditions, output guarantees, exceptional situations, and complexity. Use exact integer values for mathematical reasoning; when the source language describes fixed-width behavior, perform arithmetic through `BitVector` or a formally specified adapter. A query should return an explicit `unknown` if it lacks enough evidence, rather than guessing from an example.

## Worked reasoning pattern

Start with an IR expression and annotate its width, signed interpretation, reachable input states, and relevant control-flow guards. Translate the provable properties into the smallest appropriate representation: `BigInt` for exact constants, `Interval` for independent bounds, `Congruence` for modular information, and `LinearExpr` for affine sums. Apply only operations with matching semantics. Check the result against a boundary case, then decide whether it is strong enough to justify a rewrite.

For a concrete experiment, begin by constructing a test case in `tests/test_ccint.cpp`. Use the public umbrella include `#include <Weighstone/CCINT.hpp>`, and build the test using the project CMake target. If the technique described in this chapter is marked **planned**, this experiment is a design exercise, not a claim that its high-level optimization is already shipped.

## Failure modes and precision

Wrapping machine recurrences require an explicit modular model; the current type assumes mathematical integers.

There are two distinct outcomes when an optimizer cannot establish a result: the proposition may actually be false, or the chosen abstraction may simply be too weak to prove it. Reserve `disproven` for the former when a sound proof is available, and `unknown` for the latter. This distinction is especially important when eliminating safety checks.

## Exercise

Compute the 10^9-th term of a recurrence with a 200-bit step. Add a regression test that covers a normal case, an extreme boundary, and at least one invalid or inconclusive case. Explain why your assertions use mathematical versus bitvector semantics.

## Signed loops and tiling

`LoopRange(10,-2,-3)` visits 10, 7, 4, 1. `tile_count(3)` is two; `tile(0,3)`
visits 10, 7, 4 and `tile(1,3)` visits 1. Empty loops have zero tiles. Invalid
ordinals throw out_of_range; zero steps and nonpositive tile sizes are invalid.
`LoopNest({"t"}, {{10,-2,-3}})` describes ordinals 0 through 3 and maps them to
10-3*t using `induction_values()`. This separation prevents descending source
loops from being confused with ascending lexicographic execution order.

## Further engineering

The project `MANIFEST.yaml` records this chapter and the corresponding header or design reference. For a production optimizer, follow up with property-based testing, compile-time budgets, diagnostics for rejected rewrites, and explicit review of the mathematical assumptions. Do not attach target-specific behavior to a domain-independent data structure unless its semantics are specified.
