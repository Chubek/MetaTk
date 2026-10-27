# Chapter 19: Integer Matrices

**Domain:** linear algebra · **Status:** implemented · **Primary reference:** `include/Weighstone/CCINT/Matrix.hpp`

## Problem and model

IntegerMatrix stores arbitrary-precision rectangular matrices and exposes dimensions and checked element access. Determinants use an exact fraction-free algorithm.

At every compiler optimization boundary it is necessary to identify what is known about the input program, which arithmetic universe is being modeled, and whether the facts have been inferred conservatively. A result is useful only insofar as it remains true for every execution admitted by those assumptions. This chapter describes this issue in the context of **integer matrices**.

## Method and API contract

Keep matrix entries exact when deriving dependence or lattice facts.

When implementing or extending the API, document preconditions, output guarantees, exceptional situations, and complexity. Use exact integer values for mathematical reasoning; when the source language describes fixed-width behavior, perform arithmetic through `BitVector` or a formally specified adapter. A query should return an explicit `unknown` if it lacks enough evidence, rather than guessing from an example.

## Worked reasoning pattern

Start with an IR expression and annotate its width, signed interpretation, reachable input states, and relevant control-flow guards. Translate the provable properties into the smallest appropriate representation: `BigInt` for exact constants, `Interval` for independent bounds, `Congruence` for modular information, and `LinearExpr` for affine sums. Apply only operations with matching semantics. Check the result against a boundary case, then decide whether it is strong enough to justify a rewrite.

For a concrete experiment, begin by constructing a test case in `tests/test_ccint.cpp`. Use the public umbrella include `#include <Weighstone/CCINT.hpp>`, and build the test using the project CMake target. If the technique described in this chapter is marked **planned**, this experiment is a design exercise, not a claim that its high-level optimization is already shipped.

## Failure modes and precision

Hermite normal form, Smith normal form and LLL are not implemented.

There are two distinct outcomes when an optimizer cannot establish a result: the proposition may actually be false, or the chosen abstraction may simply be too weak to prove it. Reserve `disproven` for the former when a sound proof is available, and `unknown` for the latter. This distinction is especially important when eliminating safety checks.

## Exercise

Check determinant of identity and zero matrices at several sizes. Add a regression test that covers a normal case, an extreme boundary, and at least one invalid or inconclusive case. Explain why your assertions use mathematical versus bitvector semantics.

## Tensor metadata and layouts

`include/Weighstone/CCINT/Tensor.hpp` supplies `TensorShape` and `TensorLayout`.
Extents, strides, element counts and offsets are BigInt values. Rank zero is a
scalar with one element; any zero extent makes a tensor empty. Negative extents
and rank mismatches throw. Broadcasting aligns trailing axes: equal extents or
an extent of one are compatible, including the 0-with-1 result of zero.

`TensorLayout::row_major(shape, base)` constructs contiguous strides.
`offset(indices)` validates logical bounds. `storage_range()` returns the exact
inclusive hull of addressed offsets, or bottom for an empty tensor; it does not
assert that every offset in the hull is addressed. Signed and zero strides are
valid. `permute(order)` uses order[new_axis]=old_axis. `broadcast_to(shape)`
introduces zero strides; writes through such views can alias. `reshape(shape)`
requires equal element count and row-major contiguity; singleton-axis strides
are irrelevant. `linearize(access)` returns a one-output affine element map.

For example, row-major shape `{2,3}` has strides `{3,1}` and offset `{1,2}` = 5.
Permuting axes `{1,0}` gives shape `{3,2}`, strides `{1,3}`, and offset `{2,1}` = 5.
No storage is allocated, and allocation bounds and byte-size conversions remain
caller responsibilities.

## Further engineering

The project `MANIFEST.yaml` records this chapter and the corresponding header or design reference. For a production optimizer, follow up with property-based testing, compile-time budgets, diagnostics for rejected rewrites, and explicit review of the mathematical assumptions. Do not attach target-specific behavior to a domain-independent data structure unless its semantics are specified.
