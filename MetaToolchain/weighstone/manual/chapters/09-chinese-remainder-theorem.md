# Chapter 9: Chinese Remainder Theorem

**Domain:** number theory · **Status:** implemented · **Primary reference:** `include/Weighstone/CCINT/NumberTheory.hpp`

## Problem and model

chinese_remainder combines two residues even when moduli are non-coprime, provided they agree modulo the GCD. The answer is canonical modulo the LCM.

At every compiler optimization boundary it is necessary to identify what is known about the input program, which arithmetic universe is being modeled, and whether the facts have been inferred conservatively. A result is useful only insofar as it remains true for every execution admitted by those assumptions. This chapter describes this issue in the context of **chinese remainder theorem**.

## Method and API contract

Use the result to combine alignment requirements from nested loops and address calculations.

When implementing or extending the API, document preconditions, output guarantees, exceptional situations, and complexity. Use exact integer values for mathematical reasoning; when the source language describes fixed-width behavior, perform arithmetic through `BitVector` or a formally specified adapter. A query should return an explicit `unknown` if it lacks enough evidence, rather than guessing from an example.

## Worked reasoning pattern

Start with an IR expression and annotate its width, signed interpretation, reachable input states, and relevant control-flow guards. Translate the provable properties into the smallest appropriate representation: `BigInt` for exact constants, `Interval` for independent bounds, `Congruence` for modular information, and `LinearExpr` for affine sums. Apply only operations with matching semantics. Check the result against a boundary case, then decide whether it is strong enough to justify a rewrite.

For a concrete experiment, begin by constructing a test case in `tests/test_ccint.cpp`. Use the public umbrella include `#include <Weighstone/CCINT.hpp>`, and build the test using the project CMake target. If the technique described in this chapter is marked **planned**, this experiment is a design exercise, not a claim that its high-level optimization is already shipped.

## Failure modes and precision

An incompatible pair is unsatisfiable, not an unknown solution; propagate this as a contradiction.

There are two distinct outcomes when an optimizer cannot establish a result: the proposition may actually be false, or the chosen abstraction may simply be too weak to prove it. Reserve `disproven` for the former when a sound proof is available, and `unknown` for the latter. This distinction is especially important when eliminating safety checks.

## Exercise

Combine x=2 mod 3 and x=3 mod 5, then construct an incompatible pair. Add a regression test that covers a normal case, an extreme boundary, and at least one invalid or inconclusive case. Explain why your assertions use mathematical versus bitvector semantics.

## Further engineering

The project `MANIFEST.yaml` records this chapter and the corresponding header or design reference. For a production optimizer, follow up with property-based testing, compile-time budgets, diagnostics for rejected rewrites, and explicit review of the mathematical assumptions. Do not attach target-specific behavior to a domain-independent data structure unless its semantics are specified.
