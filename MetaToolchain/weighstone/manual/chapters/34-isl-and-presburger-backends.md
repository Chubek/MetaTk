# Chapter 34: ISL and Presburger Backends

**Domain:** polyhedral · **Status:** planned · **Primary reference:** `include/Weighstone/CCINT/Polyhedron.hpp`

## Problem and model

The implemented Polyhedron provides conjunctions of integer affine constraints, intersection, membership, unary bounding boxes and budgeted proofs. Integer-strengthened Fourier-Motzkin elimination proves emptiness; only witnesses or complete enumeration establish nonemptiness. External ISL and complete Presburger backends remain future work.

At every compiler optimization boundary it is necessary to identify what is known about the input program, which arithmetic universe is being modeled, and whether the facts have been inferred conservatively. A result is useful only insofar as it remains true for every execution admitted by those assumptions. This chapter describes this issue in the context of **isl and presburger backends**.

## Method and API contract

Use the local exact APIs for bounded and simple relational problems; map unsupported queries to unknown. Future foreign solvers must preserve context lifetime and distinguish timeout from disproof.

When implementing or extending the API, document preconditions, output guarantees, exceptional situations, and complexity. Use exact integer values for mathematical reasoning; when the source language describes fixed-width behavior, perform arithmetic through `BitVector` or a formally specified adapter. A query should return an explicit `unknown` if it lacks enough evidence, rather than guessing from an example.

## Worked reasoning pattern

Start with an IR expression and annotate its width, signed interpretation, reachable input states, and relevant control-flow guards. Translate the provable properties into the smallest appropriate representation: `BigInt` for exact constants, `Interval` for independent bounds, `Congruence` for modular information, and `LinearExpr` for affine sums. Apply only operations with matching semantics. Check the result against a boundary case, then decide whether it is strong enough to justify a rewrite.

For a concrete experiment, begin by constructing a test case in `tests/test_ccint.cpp`. Use the public umbrella include `#include <Weighstone/CCINT.hpp>`, and build the test using the project CMake target. If the technique described in this chapter is marked **planned**, this experiment is a design exercise, not a claim that its high-level optimization is already shipped.

## Failure modes and precision

No ISL adapter, projection, union representation or parametric scheduler ships in this version. A rational solution is not proof of integer feasibility.

There are two distinct outcomes when an optimizer cannot establish a result: the proposition may actually be false, or the chosen abstraction may simply be too weak to prove it. Reserve `disproven` for the former when a sound proof is available, and `unknown` for the latter. This distinction is especially important when eliminating safety checks.

## Exercise

Compare 2*x+2*y=1 with x-y>=1 and explain an emptiness proof versus an inconclusive unbounded search. Add a regression test that covers a normal case, an extreme boundary, and at least one invalid or inconclusive case. Explain why your assertions use mathematical versus bitvector semantics.

## Implemented local polyhedron API

`Polyhedron({"i","j"})` declares an ordered coordinate space. `add` validates
variables; `intersect` requires matching spaces. `contains` requires every
coordinate. `bounding_box` derives only unary bounds and is an overapproximation.
`integer_points` enumerates the finite box and filters constraints; its complete
verdict must be proven before treating the returned vector as exhaustive.
The zero-dimensional universe has one empty tuple.

`is_empty` returns disproven only with an integer witness. Elimination combines
oppositely signed inequalities and divides coefficient GCDs, rounding bounds
down for integer coordinates. Contradictions prove emptiness, but failure to
derive a contradiction is not proof of satisfiability. `prove(constraint)` checks
universal truth using interval bounds and emptiness of negated integer branches.
Disproven means false throughout the domain, not one violating point. Mixed
domains and incomplete analyses return unknown. Empty domains prove universal
properties vacuously.

`AnalysisBudget` caps candidate box points (100,000) and intermediate elimination
inequalities (4,096). Each proof branch gets that budget. Bounds do not limit
input length, BigInt bit size, total memory or wall-clock time. Exhausted queries
stay unknown. `AffineMap` validates declared inputs, evaluates ordered outputs
and composes positionally with `outer.compose(inner)`.

## Further engineering

The project `MANIFEST.yaml` records this chapter and the corresponding header or design reference. For a production optimizer, follow up with property-based testing, compile-time budgets, diagnostics for rejected rewrites, and explicit review of the mathematical assumptions. Do not attach target-specific behavior to a domain-independent data structure unless its semantics are specified.
