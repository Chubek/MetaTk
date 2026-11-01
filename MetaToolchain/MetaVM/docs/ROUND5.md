> [ROUND6.md](ROUND6.md) adds SECD and Krivine native subsets; 42 profiles now
> remain catalog-only. The fifth-pass record below is historical.

# Fifth implementation pass

This phase adds bounded, opt-in SSAVM dataflow checking while preserving the
instruction-vector runner, structural verifier, simultaneous PHIs and immutable
arrays. See [SSAVM.md](SSAVM.md) for the full API/semantic/limit contract.

## Additions

- `ISSA::analyze`, with default and explicit program/analysis-limit overloads.
- `ISSA::AnalysisLimits`: independent state, fact-cell and logical-work ceilings.
- `ISSA::Analysis` and `ValueKind`: conservative reachability, produced/returned
  kinds and analysis resource counts.
- `ISSA::parse_checked`: parse/analyze with original source-line diagnostics.
- `ssavm-run --check` and `--max-analysis-{states,cells,work}` options.
- Installed SDK regression now calls the analysis API and installed check CLI.

The engine in `VMLibrary/SSAVM/ISSAAnalysis.cpp` constructs basic blocks and tracks
undefined/scalar/array may-sets separately for each incoming CEN. It follows both
conditional successors and iterates until stable. PHI groups read old facts and
commit simultaneously at PFE; CEN propagation/reset matches the existing runtime.
It reports possibly undefined uses, incompatible operand kinds and invalid PHI
selections before execution. Existing parse/verify/run entry points stay opt-in
compatible: their acceptance and runtime checks are unchanged.

## Validation coverage

`tests/refinement.cpp` adds early/forward undefined reads, skipped definitions,
joins, delayed backedge type widening, scalar/array PHIs, all scalar binary and
branch operand contracts, CEN persistence/reset, unselected PHI operands,
simultaneous staging, unreachable code, sparse maximum-size CENs and all analysis
budget boundaries. Two-way diamonds compare the inferred union against actual
execution of both scalar/array alternatives. A 20,001-instruction straight-line
program verifies using one entry-state table.

Existing figure-5 Fibonacci, nested factorial and array-Fibonacci differential
tests now also invoke analysis. CTest adds a check-only Fibonacci case. CLI tests
cover source lines, budget diagnostics, usage errors and nonexecution of loops.
All original checks and fourth-pass ROM/install tests remain. Reproducible results
are recorded in [VALIDATION.md](VALIDATION.md).

## Scope and remaining work

This is definite-assignment/type checking for the implemented i64/array interpreter,
not conventional SSA-form dominance certification. Predicate/range analysis,
argument availability, termination and runtime allocation/arithmetic/bounds checks
remain separate. Conditional paths are conservatively explored even if their
predicate is constant. No new historical ISA support is implied; 44 catalog
profiles remain catalog-only.

Reusable native decoder generation, MetaTeX/libdrm integration, non-Linux CI and
the supplied MetaML type-checker leak finding remain open. Dependency bytes and
all historical reports/archives are retained.
