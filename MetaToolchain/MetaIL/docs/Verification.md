# Numeric integration verification

## Native API and registry follow-up

The native API/registry work adds C and C++ API tests and three executable C++
examples, bringing the default suite to **10 tests**. The initial run for this work
passed 3/5 existing tests: the generator and integration checks still expected a
`numeric/` subdirectory although all 120 numeric files were committed directly in
`optimize-kernels/`. Every relocated file was compared with its generated contents
before correcting those path expectations; kernel contents and inventory were
preserved.

Verification of the C/C++ APIs, registry, CLI and examples:

| Configuration | Result | Time |
|---|---:|---:|
| GCC 16 Release, warnings as errors | 10/10 | 4.25 s |
| Clang 22 Release, warnings as errors | 10/10 | 4.41 s |
| GCC ASan + UBSan, warnings as errors | 10/10 | 13.95 s |
| Clang ASan + UBSan, warnings as errors | 10/10 | 12.16 s |
| Shared library, examples disabled | 7/7 | 4.15 s |

The new tests cover error recovery, wraparound and effect/trap preservation through
the C API, explicit pass selection, metadata/path discovery, conjunctive keyword,
tag and use-case searches, custom metadata, duplicate/invalid-load rollback, context
isolation, move semantics, and registry lifetime. Installed C and C++ consumers
were built with `find_package(MetaIL CONFIG REQUIRED)` and run against the installed
runtime. `metail verify` confirms all 230 rewrites fire across 183,473 signed-64 cases
(2,470 effect/trap cases); generator `--check` confirms all 321 declarations.

The following sections record the earlier numeric-integration verification.

Verified on Linux with GCC 16.2.1, Clang 22.1.8, and the actual Lua 5.4 runtime.

## Baseline and final inventory

| Item | Baseline | Final |
|---|---:|---:|
| Kernel files | 201 | 321 |
| Declarative rewrites | 134 | 230 |
| Analyses | 60 | 84 |
| Procedural passes | 7 | 7 |
| CTest cases | 1 | 5 |

The original baseline build and integration test passed in 0.29 seconds, before
edits. The expanded verifier checks 183,473 signed-64 cases, including 2,470
effect/trap cases, and requires every one of the 230 rules to fire.

Final runs (parallel build-tree verification; timings are indicative):

| Configuration | CTest | Time |
|---|---:|---:|
| GCC Release, warnings as errors | 5/5 passed | 3.99 s |
| Clang Release, warnings as errors | 5/5 passed | 3.97 s |
| GCC ASan + UBSan, warnings as errors | 5/5 passed | 11.09 s |
| Clang ASan + UBSan, warnings as errors | 5/5 passed | 10.55 s |

The five tests cover CLI/integration and rewrite equivalence, native numeric
results and error handling, runtime/loader hardening, generator reproducibility,
and the standalone Lua 5.4 module. All 24 numeric analysis results are compared
with independently computed Python statistics. Native tests include extreme
magnitudes, NaN/infinity, paired-series shape/domain errors, input immutability,
catalog contents, and binary64 JSON precision.

Regression cases cover discarded traps, self-referential CSE, unreachable
predecessors, natural self-loops, nonterminating postdominator regions, malformed
guards, failed-load rollback, library-table isolation, symlink cycles, overflowing
hex literals, and excessive expression nesting/depth.

An installation under `/tmp/opencode/metail-install` passed host discovery and
321-kernel selftest from outside the source directory. The installed module also
loaded and computed statistics in the system Lua 5.4 interpreter.

## Reproduce

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS=-Werror -DCMAKE_CXX_FLAGS=-Werror
cmake --build build
ctest --test-dir build --output-on-failure
build/metail verify
python3 tools/generate_kernels.py --check

cmake -S . -B /tmp/opencode/metail-sanitize -DCMAKE_BUILD_TYPE=Debug \
  '-DCMAKE_C_FLAGS=-Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_CXX_FLAGS=-Werror -fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build /tmp/opencode/metail-sanitize
ctest --test-dir /tmp/opencode/metail-sanitize --output-on-failure
```

For Clang, use a separate build tree and add
`-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`. CMake preloads the matching
ASan runtime for the uninstrumented system Lua executable when testing an
instrumented shared module on Linux.

MetaNum's upstream GSL/ISL/PPL catalogs remain metadata; no GSL/ISL/PPL executable
wrappers are claimed. The integer IR remains signed-64, while MetaNum analysis
results use binary64 and are not used to prove integer rewrites.
