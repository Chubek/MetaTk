# Chapter 33: Dependence Analysis

**Domain:** polyhedral · **Status:** implemented · **Primary reference:** `include/Weighstone/CCINT/Loop.hpp`

## Problem and model

analyze_dependences enumerates bounded integer iteration domains and ordered affine memory accesses, retaining every conflicting flow, anti and output pair. The complete DependenceGraph supports loop permutation legality and loop-carried dependence queries.

At every compiler optimization boundary it is necessary to identify what is known about the input program, which arithmetic universe is being modeled, and whether the facts have been inferred conservatively. A result is useful only insofar as it remains true for every execution admitted by those assumptions. This chapter describes this issue in the context of **dependence analysis**.

## Method and API contract

Use increasing ordinal coordinates and supply proven nonaliasing memory identities. Preserve access-list order within each iteration. A failed or budget-exhausted analysis returns unknown and no graph.

When implementing or extending the API, document preconditions, output guarantees, exceptional situations, and complexity. Use exact integer values for mathematical reasoning; when the source language describes fixed-width behavior, perform arithmetic through `BitVector` or a formally specified adapter. A query should return an explicit `unknown` if it lacks enough evidence, rather than guessing from an example.

## Worked reasoning pattern

Start with an IR expression and annotate its width, signed interpretation, reachable input states, and relevant control-flow guards. Translate the provable properties into the smallest appropriate representation: `BigInt` for exact constants, `Interval` for independent bounds, `Congruence` for modular information, and `LinearExpr` for affine sums. Apply only operations with matching semantics. Check the result against a boundary case, then decide whether it is strong enough to justify a rewrite.

For a concrete experiment, begin by constructing a test case in `tests/test_ccint.cpp`. Use the public umbrella include `#include <Weighstone/CCINT.hpp>`, and build the test using the project CMake target. If the technique described in this chapter is marked **planned**, this experiment is a design exercise, not a claim that its high-level optimization is already shipped.

## Failure modes and precision

This bounded analyzer does not infer aliases, handle conditional accesses, synthesize schedules or model non-memory effects. A complete graph is a precondition for legality checks.

There are two distinct outcomes when an optimizer cannot establish a result: the proposition may actually be false, or the chosen abstraction may simply be too weak to prove it. Reserve `disproven` for the former when a sound proof is available, and `unknown` for the latter. This distinction is especially important when eliminating safety checks.

## Exercise

Model A[i]=A[i-1]+1 and show why the outer loop cannot run in parallel. Add a regression test that covers a normal case, an extreme boundary, and at least one invalid or inconclusive case. Explain why your assertions use mathematical versus bitvector semantics.

## Memory-dependence contracts

`AffineAccess` contains an object identity, affine subscript map and write flag.
Access-list order is execution order within each point; domain variable order
defines increasing lexicographic point order. Distinct nonempty object names
must be known nonaliasing. Equal names require the same element-coordinate
system; flatten views with `TensorLayout::linearize` when necessary. Empty
identities return unknown. Read/read pairs do not induce ordering edges.

`DependenceAnalysis::complete` describes completeness, not absence of edges.
Its optional graph is present only for complete analysis. `DependenceGraph`
stores target-minus-source iteration distances. Unknown distances make legality
unknown unless another known edge already disproves it. `permutation_legal`
requires lexicographically nonnegative transformed distances and retains
within-iteration statement order. `parallel_axis` checks for dependencies
carried by that axis at a fixed outer prefix. Non-memory ordering must be added
by the caller. Neither query alone proves arbitrary compiler IR rewrites safe.

The algorithm enumerates candidate box points and then event pairs, with defaults
of 100,000 points and 1,000,000 pairs. Work is O(E^2) pair checks for E events,
plus exact affine evaluation, graph insertion and coefficient arithmetic. Graphs
can retain multiple distance facts for the same source/target statement pair.

## Further engineering

The project `MANIFEST.yaml` records this chapter and the corresponding header or design reference. For a production optimizer, follow up with property-based testing, compile-time budgets, diagnostics for rejected rewrites, and explicit review of the mathematical assumptions. Do not attach target-specific behavior to a domain-independent data structure unless its semantics are specified.
