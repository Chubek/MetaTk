# Weighstone

**CCINT — Compiler-Centric Integer Toolkit** is a C++20 header-only collection of exact integer arithmetic and conservative reasoning primitives for compiler optimizers.

## Quick start

```cpp
#include <Weighstone/CCINT.hpp>
using namespace weighstone::ccint;
int main() {
  BitVector x(8,255);
  auto sum = checked_add(x,BitVector(8,1));
  return (sum.unsigned_overflow && sum.value.unsigned_value()==0) ? 0 : 1;
}
```

Build/tests: `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`.

**Dependencies:** C++20 and Boost.Multiprecision (header-only). GMP, ISL, Z3 and LLVM are *not* integrated. All Weighstone components are inline or defined inside classes. No compiled Weighstone binary is required.

## Available today

- Exact `BigInt` arithmetic, Euclidean division, GCD, CRT, modular inverse, rationals.
- Arbitrary-width fixed-width `BitVector` values; checked arithmetic; bitwise known-bits facts.
- Mathematical integer intervals, congruences, a lightweight reduced product and linear-expression range proofs.
- Exact determinant using fraction-free Bareiss elimination, affine recurrences, a small symbolic rewrite engine and conservative proof queries.
- Interval widening, one-sided interval/congruence reduction, and exact signed-stride loop counts and tiling.
- Integer polyhedra, affine maps/composition, budgeted emptiness and universal constraint proofs.
- Bounded affine memory-dependence analysis, loop interchange legality and loop-carried dependence checks.
- Exact tensor shapes, broadcasting, strided layouts, transposition, contiguous reshape, affine linearization and index-bounds proofs.
- Deterministic directed graphs, topological sorting and iterative strongly connected components.

## Loop and tensor reasoning

```cpp
using namespace weighstone::ccint;
auto i = LinearExpr::variable("i"), j = LinearExpr::variable("j");
auto domain = LoopNest({"i", "j"}, {{0, 4}, {0, 8}}).domain();
AffineMap access({"i", "j"}, {i, j});
auto layout = TensorLayout::row_major(TensorShape({4, 8}));
auto safe = prove_in_bounds(domain, access, layout.shape());
auto deps = analyze_dependences(domain, {{"A", layout.linearize(access), true}});
if (safe.is_proven() && deps.graph &&
    deps.graph->permutation_legal({1, 0}).is_proven()) {
    // Index bounds and the supplied memory dependences permit interchange.
}
```

See the runnable `examples/loop_tensor.cpp`. Public APIs live in
`include/Weighstone/CCINT/`; reusable graph/permutation helpers live in `support/`.

### Contracts and limits

- **Polyhedra:** conjunctions of affine integer constraints. `add`, `intersect`,
  `contains`, `bounding_box`, `integer_points`, `is_empty`, and `prove` use exact
  `BigInt` arithmetic. Integer-strengthened Fourier–Motzkin elimination can prove
  emptiness; it never infers integer feasibility from a rational solution.
  Enumeration requires a finite box derived from unary constraints, so even
  some bounded relational sets return `unknown`. There is no projection or
  parametric scheduler. Variables must be declared; composition is positional.
- **Verdicts:** `is_empty` describes emptiness; `prove` and `prove_in_bounds`
  describe universal properties. For these property queries, `disproven` means
  false everywhere, while mixed domains can return `unknown`. Legality queries
  instead disprove a proposed transformation when any supplied dependence is
  violated. `IntegerPoints::complete` and `DependenceAnalysis::complete` describe
  analysis completeness. An incomplete dependence analysis has **no graph**.
- **Budgets:** `AnalysisBudget` defaults to 100,000 candidate box points and
  4,096 elimination inequalities; `DependenceBudget` adds a 1,000,000 event-pair
  cap. Each query/branch has its own budget. These are work-count limits, not
  wall-clock, input-size, coefficient-size or total memory limits. Exhaustion
  returns `unknown`; invalid dimensions, names and permutations throw standard
  exceptions. Zero-dimensional domains contain one empty tuple unless constrained
  to be empty; a zero point budget cannot enumerate that tuple.
- **Loops:** `LoopRange(start, stop, step)` is half-open, with either sign of
  nonzero step. Tiling uses a positive number of iterations per tile and retains
  the partial final tile. `LoopNest` domains use increasing **ordinal** coordinates;
  `induction_values()` maps them to potentially descending/strided values.
  `analyze_dependences` follows domain-variable lexicographic order, then access
  list order, retaining all conflicting flow/anti/output pairs. Accesses are
  unconditional. Distinct nonempty object names must denote proven nonaliasing
  memory; equal names must use a common element-coordinate system. Flatten
  aliased views with `TensorLayout::linearize`. Other effects require additional
  dependences. This is bounded analysis, not compiler IR rewriting.
- **Graphs:** `support::DirectedGraph` has stable dense vertex IDs, ignores
  duplicate edges, and returns `nullopt` for cyclic topological order. SCC
  traversal is iterative. `DependenceGraph` preserves distance facts on edges;
  missing distances yield `unknown`. Permutations use `order[new_axis]=old_axis`
  and retain within-iteration statement order. `parallel_axis` checks dependence
  carriage at a fixed outer-loop prefix. These checks assume a complete graph.
- **Tensors:** metadata only, with arbitrary-precision element offsets and signed
  strides. Rank zero is a scalar; any zero extent is empty. Broadcasting creates
  zero strides and may alias elements. `offset` checks logical bounds;
  `storage_range` is an inclusive hull, not an allocation guarantee. Reshape
  requires row-major contiguity and equal element counts. Bounds proofs do not
  establish safe parallel writes or physical allocation validity.

## Verification

The build checks each header independently. CTest includes exhaustive low-width
arithmetic, reduced-product checks, small-polyhedron enumeration oracles, all
three-vertex directed graphs, and loop-tiling/tensor-layout properties. Run:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Deliberate limits

This is an **implemented foundation**, not a production-complete optimizer. No complete Presburger/SMT solver, parametric polyhedral scheduler, generic abstract interpretation fixed-point engine, e-graph, instruction cost model or proof-producing validator is included. Proofs report `unknown` when the engine cannot decide. The symbolic normalizer, polyhedra, tensors and loops use **mathematical integers** and must not be applied to trapping/poison/overflow-sensitive compiler IR without establishing semantic compatibility. `BitVector` fixed-width operations explicitly wrap; shifts of >= width use totalizing semantics defined by CCINT, not necessarily the semantics of a target IR. `trunc` rejects widening, and `zext`/`sext` reject narrowing; known-bit widths share the bitvector width limit.

See `manual/CCINT.md`, `manual/chapters/`, and `MANIFEST.yaml` for detailed coverage and current status.
