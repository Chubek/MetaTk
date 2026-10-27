# Chapter 18: Widening and Fixed Points

**Domain:** analysis · **Status:** implemented · **Primary reference:** `include/Weighstone/CCINT/Interval.hpp`

## Problem and model

Interval::widen(next) preserves stable bounds and drops outward-moving bounds to infinity. Bottom is the identity for an initial widening. Exact scale preserves one-sided bounds and handles zero explicitly.

At every compiler optimization boundary it is necessary to identify what is known about the input program, which arithmetic universe is being modeled, and whether the facts have been inferred conservatively. A result is useful only insofar as it remains true for every execution admitted by those assumptions. This chapter describes this issue in the context of **widening and fixed points**.

## Method and API contract

Call previous.widen(next) at selected loop headers in a caller-managed worklist. DirectedGraph supplies CFG traversal and strongly connected components.

When implementing or extending the API, document preconditions, output guarantees, exceptional situations, and complexity. Use exact integer values for mathematical reasoning; when the source language describes fixed-width behavior, perform arithmetic through `BitVector` or a formally specified adapter. A query should return an explicit `unknown` if it lacks enough evidence, rather than guessing from an example.

## Worked reasoning pattern

Start with an IR expression and annotate its width, signed interpretation, reachable input states, and relevant control-flow guards. Translate the provable properties into the smallest appropriate representation: `BigInt` for exact constants, `Interval` for independent bounds, `Congruence` for modular information, and `LinearExpr` for affine sums. Apply only operations with matching semantics. Check the result against a boundary case, then decide whether it is strong enough to justify a rewrite.

For a concrete experiment, begin by constructing a test case in `tests/test_ccint.cpp`. Use the public umbrella include `#include <Weighstone/CCINT.hpp>`, and build the test using the project CMake target. If the technique described in this chapter is marked **planned**, this experiment is a design exercise, not a claim that its high-level optimization is already shipped.

## Failure modes and precision

A generic worklist solver and narrowing policy are not supplied. Widening is deliberately less precise than join.

There are two distinct outcomes when an optimizer cannot establish a result: the proposition may actually be false, or the chosen abstraction may simply be too weak to prove it. Reserve `disproven` for the former when a sound proof is available, and `unknown` for the latter. This distinction is especially important when eliminating safety checks.

## Exercise

Apply widening to successive ranges [0,0], [0,1], [0,2] and explain convergence. Add a regression test that covers a normal case, an extreme boundary, and at least one invalid or inconclusive case. Explain why your assertions use mathematical versus bitvector semantics.

## Graph and widening APIs

`support/Graph.hpp` implements `weighstone::support::DirectedGraph`. Vertex IDs
are stable dense indices. Duplicate edges are ignored. `topological_order()`
returns an optional vector (nullopt on cycles); `strongly_connected_components()`
uses iterative Kosaraju traversal with sorted members and topologically ordered
components. `reachable(a,b)` includes the zero-edge path. Traversals are
deterministic. SCC traversal is O(V+E), plus sorting component members;
topological sorting uses a minimum-ID priority queue.

`Interval::widen(next)` uses the receiver as the old state. For example,
`Interval(0,3).widen(Interval(0,4))` becomes `[0,+infinity)`. Callers own the
worklist, update order, widening placement, convergence checks and IR facts.

## Further engineering

The project `MANIFEST.yaml` records this chapter and the corresponding header or design reference. For a production optimizer, follow up with property-based testing, compile-time budgets, diagnostics for rejected rewrites, and explicit review of the mathematical assumptions. Do not attach target-specific behavior to a domain-independent data structure unless its semantics are specified.
