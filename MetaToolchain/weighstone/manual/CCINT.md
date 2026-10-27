# CCINT: A Compiler-Centric Integer Toolkit

**Weighstone Manual — 40 Chapters**

This manual describes implemented APIs, mathematical invariants, known limitations, and designs for future compiler components. Chapters with `planned` status are explanatory design material, **not** implemented features. All current Weighstone headers use C++20 and Boost.Multiprecision. Start with the `README.md` and build the test executable before adding an optimizer pass.

## Table of contents

1. [Design Goals](chapters/01-design-goals.md) — implemented
2. [C++20 Setup](chapters/02-c-20-setup.md) — implemented
3. [Mathematical vs Machine Integers](chapters/03-mathematical-vs-machine-integers.md) — implemented
4. [Big Integers](chapters/04-big-integers.md) — implemented
5. [Division and Remainders](chapters/05-division-and-remainders.md) — implemented
6. [GCD and LCM](chapters/06-gcd-and-lcm.md) — implemented
7. [Extended Euclid](chapters/07-extended-euclid.md) — implemented
8. [Modular Inverses](chapters/08-modular-inverses.md) — implemented
9. [Chinese Remainder Theorem](chapters/09-chinese-remainder-theorem.md) — implemented
10. [Exact Rationals](chapters/10-exact-rationals.md) — implemented
11. [BitVector Representation](chapters/11-bitvector-representation.md) — implemented
12. [Checked Arithmetic](chapters/12-checked-arithmetic.md) — implemented
13. [Shifts and Rotates](chapters/13-shifts-and-rotates.md) — implemented
14. [Known Bits](chapters/14-known-bits.md) — implemented
15. [Integer Intervals](chapters/15-integer-intervals.md) — implemented
16. [Congruence Domains](chapters/16-congruence-domains.md) — implemented
17. [Interval-Congruence Product](chapters/17-interval-congruence-product.md) — implemented
18. [Widening and Fixed Points](chapters/18-widening-and-fixed-points.md) — implemented
19. [Integer Matrices](chapters/19-integer-matrices.md) — implemented
20. [Bareiss Determinants](chapters/20-bareiss-determinants.md) — implemented
21. [Linear Expressions](chapters/21-linear-expressions.md) — implemented
22. [Linear Range Evaluation](chapters/22-linear-range-evaluation.md) — implemented
23. [Conservative Inequality Proofs](chapters/23-conservative-inequality-proofs.md) — implemented
24. [Symbolic Expression Trees](chapters/24-symbolic-expression-trees.md) — implemented
25. [Rewriting and Simplification](chapters/25-rewriting-and-simplification.md) — implemented
26. [Equality Queries](chapters/26-equality-queries.md) — implemented
27. [Affine Recurrences](chapters/27-affine-recurrences.md) — implemented
28. [Trip Counts](chapters/28-trip-counts.md) — implemented
29. [Induction Variable Recognition](chapters/29-induction-variable-recognition.md) — planned
30. [Bitwidth Reduction](chapters/30-bitwidth-reduction.md) — planned
31. [Bounds-Check Elimination](chapters/31-bounds-check-elimination.md) — implemented
32. [Strength Reduction](chapters/32-strength-reduction.md) — planned
33. [Dependence Analysis](chapters/33-dependence-analysis.md) — implemented
34. [ISL and Presburger Backends](chapters/34-isl-and-presburger-backends.md) — planned
35. [SMT and Bitvector Solving](chapters/35-smt-and-bitvector-solving.md) — planned
36. [Testing and Differential Fuzzing](chapters/36-testing-and-differential-fuzzing.md) — implemented
37. [Undefined and Poison Semantics](chapters/37-undefined-and-poison-semantics.md) — planned
38. [Compiler IR Integration](chapters/38-compiler-ir-integration.md) — planned
39. [Caching and Resource Budgets](chapters/39-caching-and-resource-budgets.md) — planned
40. [Roadmap and Extension Contracts](chapters/40-roadmap-and-extension-contracts.md) — implemented

## Conventions

All mathematical integer inequalities denote properties of arbitrary-precision `BigInt` values. Signed and unsigned machine integers require explicit `BitVector` width. Intervals have inclusive endpoints; a missing endpoint denotes infinity and bottom denotes emptiness. `Verdict::unknown` means insufficient proof, not falsity. Do not apply `Expr` algebraic laws to a compiler IR with poison, traps or nonmodular overflow unless the relevant conditions are established.

## Quality and scope

The included tests cover representative correctness cases. They are not a full formal verification or performance study. Extend low-bitwidth enumeration, metamorphic tests and differential comparison against independent numerical libraries before using Weighstone to justify safety-critical transformations.
