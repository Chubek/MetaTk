# XVM default execution — 2026-11-05

See [XVM.md](XVM.md) for the added C11 APIs, native/portable block compiler,
default changes and unchanged guest-coverage boundaries. GCC and Clang Release
with current MetaML pass **25/25** CTest cases. GCC shared/lib64 packaging passes
**23/23**; Clang ASan/UBSan without MetaML passes **23/23** with default leak
detection. Standalone GCC Machinery sanitizer and native-disabled builds each
pass **5/5**. Machinery now performs **8,446** checks, plus **153** dedicated XVM
checks, retaining the original coverage and adding the third engine.

DSLtk's current MetaML C++/D suite passes **28/28**, including **270** comparisons
against the retained tree evaluator. Its two Clang runtime/toolchain sanitizer
tests pass with leak detection disabled; this is not a complete sanitizer or
leak-clean claim. Installed C++ and D SDK consumers compile and execute XVM.
See `../../../DSLtk/XVM_VALIDATION.md` for the full matrix and reproduction.
Native execution validation is Linux amd64 only. Historical records follow.

# Generation repair — 2026-11-04

See [GENERATION.md](GENERATION.md) for the failed baseline, repair scope,
reproduction commands and artifact/failure-preservation tests. GCC Release passes
22/22 CTest cases; Clang Release with real MetaML passes 24/24; Clang Debug
ASan/UBSan passes 22/22 with default leak detection (MetaML disabled). The GCC
fallback suite also passes, retaining 5,056 Machinery checks and adding compilation
and dual-engine execution of all 49 generated C factories. Historical records follow.

# Sixth-pass validation — 2026-10-31

The two new native closure-machine profiles are documented in `ROUND6.md` and
`CLOSURE_VMS.md`. Toolchains remain Linux x86-64, CMake 4.4.3, GCC/G++ 16.2.1,
and Clang/Clang++ 22.1.8. Supplied dependency bytes are unchanged.

| Configuration | Result |
|---|---|
| Pre-edit fallback baseline | Passed; 5,056 Machinery checks, 49 catalog examples |
| Final `make test` | All stages passed, including both closure VMs and native demo |
| GCC fallback ASan/UBSan | All stages passed, `detect_leaks=0` |
| GCC Release + real MetaML + C++ wrapper (`out-meta`) | 19/19 passed, 10.06 s |
| Clang Release + real MetaML + C++ wrapper (`out-clang`) | 19/19 passed, 10.39 s |
| Clang Debug ASan/UBSan + real MetaML (`out-meta-sanitize`) | 19/19 passed, 10.41 s, `detect_leaks=0` |
| Dedicated Clang ASan/UBSan closure-VM suite with `detect_leaks=1` | Passed; no leak report |
| GCC Release shared libraries and `lib64` (`out-shared`) | 17/17 passed, 3.61 s |
| Catalog regeneration / dependency hashes | Byte-identical regeneration; native sources and supplied dependency hashes preserved |

Builds are warning-free under the existing warnings-as-errors policy; Release
checks remain unconditional with `NDEBUG`. Test configurations ran concurrently,
so elapsed times are smoke-test observations with contention, not benchmarks.
The full suites retain all prior tests. The two new CTest cases execute native
closure-machine regressions and `closure-vm-demo`. Header self-containment and
relocated SDK consumers include both new APIs; the installed demo is also run.

New coverage includes lexical capture and shadowing, escaping closures with owned
result snapshots, nested SEL/AP/RET/JOIN continuations, signed-i64 edge arithmetic,
unknown/malformed/unreachable opcodes, version/target/index checks, every independent
resource ceiling at its boundary, divergence, lazy discard of Omega and 50,000
nested abstractions. A test-only compiler compares 64 terminating selector
programs across both VMs and known results; `(K I) Omega` additionally demonstrates
termination by name versus strict-execution exhaustion.

Use the fifth-pass configuration commands below for GCC, Clang, full sanitizer and
shared SDK builds. Additional direct commands:

```sh
make test
ASAN_OPTIONS=detect_leaks=0 make sanitize
build/closure-vm-demo
# secd=42 krivine-term=1

ctest --test-dir out-meta -R metavm_closure --output-on-failure
ASAN_OPTIONS=detect_leaks=1 out-meta-sanitize/metavm_closure_vms
```

The leak-enabled result applies only to the dedicated new-VM suite. The separate
supplied MetaML type-checker leak finding remains unresolved; the full sanitizer
configuration still disables leak detection. No new external binary format loader,
guest GC, non-Linux validation or MetaTeX/libdrm validation is claimed. Current
coverage is seven native subsets and 42 catalog-only profiles. Historical records
follow unchanged.

# Fifth-pass validation — 2026-10-31

Current additions are documented in `ROUND5.md` and the opt-in analysis section
of `SSAVM.md`. Linux x86-64 toolchains and optional dependencies match the fourth
pass; dependency hashes remain unchanged.

| Configuration | Result |
|---|---|
| Pre-edit `make test` baseline | Passed; 5,056 Machinery checks and 49 catalog examples |
| Final `make test` | All stages passed, including SSA analysis and CLI checks |
| GCC fallback ASan/UBSan | All stages passed with `ASAN_OPTIONS=detect_leaks=0` |
| GCC Release + real MetaML + C++ wrapper (`out-meta`) | 17/17 passed, 12.32 s |
| Clang Release + real MetaML + C++ wrapper (`out-clang`) | 17/17 passed, 12.96 s |
| Clang Debug ASan/UBSan + real MetaML (`out-meta-sanitize`) | 17/17 passed, 12.16 s, leak detection disabled |
| GCC Release shared libraries, `lib64` (`out-shared`) | 15/15 passed, 3.97 s |
| Regeneration/dependencies | Byte-identical catalog regeneration; supplied MetaML and matching DSLtk hashes unchanged |

CTest configurations were run concurrently; these wall-clock timings include
contention and are not comparable performance benchmarks. Builds were warning-free
with the existing `-Wall -Wextra -Wpedantic -Werror` policy. Optimized tests retain
unconditional assertions under `NDEBUG`. `git diff --check` passed.

The added `metavm_ssa_check` case analyzes figure-5 Fibonacci without execution.
The refinement suite also analyzes the existing nested-factorial and persistent-
array Fibonacci programs before differential execution. New regressions cover
undefined reads, type conflicts, all scalar binary/branch operand positions,
backedge widening, simultaneous PHIs, CEN propagation/reset, unreachable code,
mixed scalar/array results, sparse CEN values, source lines and exact budget
boundaries. A 20,001-instruction straight-line program uses one analysis state.
Installed C++ consumers call `analyze`/`parse_checked`; installed `ssavm-run`
exercises `--check`, including after shared-library prefix relocation.

Reproduction commands from the root (substitute the matching DSLtk directory):

```sh
make test
ASAN_OPTIONS=detect_leaks=0 make sanitize

cmake -S . -B out-meta -DCMAKE_BUILD_TYPE=Release -DMETAVM_TEST_CPP=ON \
  -DMETAVM_ENABLE_METAML=ON -DMETAVM_DSLTK_INCLUDE_DIR=/mnt/nvme/metatk/DSLtk
cmake --build out-meta -j4
ctest --test-dir out-meta --output-on-failure

cmake -S . -B out-clang -DCMAKE_BUILD_TYPE=Release -DMETAVM_TEST_CPP=ON \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DMETAVM_ENABLE_METAML=ON -DMETAVM_DSLTK_INCLUDE_DIR=/mnt/nvme/metatk/DSLtk
cmake --build out-clang -j4
ctest --test-dir out-clang --output-on-failure

cmake -S . -B out-meta-sanitize -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS_DEBUG='-O1 -g1' \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DMETAVM_TEST_CPP=ON -DMETAVM_SANITIZE=ON -DMETAVM_ENABLE_METAML=ON \
  -DMETAVM_DSLTK_INCLUDE_DIR=/mnt/nvme/metatk/DSLtk
ASAN_OPTIONS=detect_leaks=0 cmake --build out-meta-sanitize -j4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir out-meta-sanitize --output-on-failure

cmake -S . -B out-shared -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
  -DMETAVM_TEST_CPP=ON -DCMAKE_INSTALL_LIBDIR=lib64
cmake --build out-shared -j4
ctest --test-dir out-shared --output-on-failure
```

The fourth-pass MetaML leak finding remains unresolved; no leak-clean claim is
made. Conventional SSA dominance certification, value-range/termination proofs,
MetaTeX/libdrm integration and non-Linux validation remain open. Historical
records follow, retaining their observed results and superseded limitations.

# Fourth-pass validation — 2026-10-31

Current changes are documented in `ROUND4.md`, `CHIP8.md` and `PACKAGING.md`.
Observed on the same Linux x86-64 toolchain as the third pass (CMake 4.4.3,
GCC/G++ 16.2.1, Clang/Clang++ 22.1.8):

| Configuration | Result |
|---|---|
| Pre-edit fallback baseline | Passed; 5,056 Machinery checks and 49 catalog examples |
| Final `make test` | All stages passed, including direct CHIP-8 tests and both external ROMs |
| GCC fallback ASan/UBSan | All stages passed with `detect_leaks=0` |
| GCC Release + real MetaML + C++ wrapper (`out-meta`) | 16/16 passed, 6.93 s |
| Clang Release + real MetaML + C++ wrapper (`out-clang`) | 16/16 passed, 8.35 s; SDK recheck after config correction passed, 1.27 s |
| Clang Debug ASan/UBSan + real MetaML (`out-meta-sanitize`) | 16/16 passed, 7.13 s, `detect_leaks=0` |
| GCC Release shared libraries, `lib64`, no MetaML (`out-shared`) | 14/14 passed, 3.48 s |
| GCC default build type, `METAVM_BUILD_EXAMPLES=OFF` (`out-no-runner`) | 11/11 passed, 3.40 s |
| Catalog regeneration, fixture/dependency hashes | Byte-identical regeneration; all five fixture hashes and original dependency hashes match |

Timings are smoke-test observations, not benchmarks. Project code builds with
`-Wall -Wextra -Wpedantic -Werror`; Release tests retain unconditional checks under
`NDEBUG`. The additional CTest cases run Corax+/flags ROMs and install/relocate the
SDK. Each of the three CHIP-8 profiles checks all 22 Corax+ and 47 flags glyphs
against source-derived expectations. All previous native, Machinery, generated-C,
MetaML and SSAVM regressions remain active.

The shared/lib64 regression first exposed failure to discover the co-installed
Machinery package. `MetaVMConfig.cmake` now finds it using its relative sibling
directory; both installed tools and C11/C++17 consumers run after prefix relocation.

Use the third-pass commands below for fallback, GCC/Clang Release and real MetaML
ASan/UBSan configurations. Additional fourth-pass configurations:

```sh
cmake -S . -B out-shared -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
  -DMETAVM_TEST_CPP=ON -DCMAKE_INSTALL_LIBDIR=lib64
cmake --build out-shared -j4
ctest --test-dir out-shared --output-on-failure

cmake -S . -B out-no-runner -DMETAVM_BUILD_EXAMPLES=OFF -DMETAVM_TEST_CPP=ON
cmake --build out-no-runner -j4
ctest --test-dir out-no-runner --output-on-failure
```

## Newly observed leak diagnostic

A sanitizer build run without `ASAN_OPTIONS=detect_leaks=0` failed during the
MetaML Definition CLI test. LeakSanitizer reported **3040 bytes in 27 allocations**,
with stacks in the unchanged supplied MetaML type checker (`TypeNode`, `t_con`,
`Checker::check_types`, `Session::Session`). This is an actual leak report, not
the earlier environment's `/proc` inspection limitation. The dependency bytes
remain unchanged; the finding is unresolved and no leak-clean claim is made.
The successful ASan/UBSan runs above use the previously documented leak-disabled
configuration for both build-time generators and tests.

Diagnostic reproduction after building `out-meta-sanitize` (expected to fail on
the recorded dependency version):

```sh
ASAN_OPTIONS=detect_leaks=1 python3 tools/test_metaml_cli.py \
  out-meta-sanitize/vmweave-metaml out-meta-sanitize/vmweave . \
  /tmp/opencode/metavm-leak-probe.c
```

MetaTeX/libdrm integration, full hardware compatibility and Windows/macOS remain
unvalidated. Forty-four catalog profiles remain catalog-only. Historical results
follow unchanged; their installation limitations are superseded by this pass.

# Third-pass validation — 2026-10-31

The current implementation and API changes are documented in `ROUND3.md` and
`SSAVM.md`. The prior records below remain historical. Observed on Linux x86-64
with CMake 4.4.3, GCC/G++ 16.2.1 and Clang/Clang++ 22.1.8:

| Configuration | Result |
|---|---|
| Baseline fallback, before edits | All stages passed; 5,056 Machinery checks and 49 catalog examples |
| Baseline CMake Debug + C++ wrapper | 12/12 passed, 1.42 s |
| Final `make test` (GCC) | All stages passed, including expanded SSAVM and CLI regressions |
| Final fallback ASan/UBSan (GCC) | All stages passed with leak detection disabled |
| GCC Release + real MetaML + C++ wrapper | 14/14 CTest cases passed, 5.10 s |
| Clang Release + real MetaML + C++ wrapper | 14/14 passed, 5.66 s |
| Clang ASan/UBSan + real MetaML + C++ wrapper | 14/14 passed, 6.02 s, leak detection disabled |
| Catalog regeneration | Byte-identical across all catalog/legacy files, manifest and coverage |

Timings are observed smoke-test durations, not performance budgets. Most CTest
time is public-header self-containment compilation, including the optional C++20
MetaML adapter. Both Release configurations use `NDEBUG`; integration/refinement
checks remain unconditional. All project targets in these configurations use
`-Wall -Wextra -Wpedantic -Werror`. The unchanged supplied MetaML header needs a
local Clang suppression for its unused private parser field, described in
`ROUND3.md`; warnings in project code remain enabled.

Additional coverage:

- All **51** current `.vmweave` files evaluated by the real MetaML: 49 catalog
  translations, typed factorial, and ToyR16 Definition authoring. Catalog/factorial
  serialized modules match assembly companions byte-for-byte. ToyR16's lowered
  modules match the C++ definition for arithmetic/store and backward-branch code.
- MetaML function-generated loop, all Definition binary operations, signed fields,
  memory accesses, jumps/branches, invalid references/fields/masks, node budgets,
  evaluation cancellation/depth, and preserved diagnostic codes/source spans.
- MetaML CLI failure and in-place-output checks run during generated-C build;
  the emitted ToyR16 C factory executes under both Machinery engines.
- SSAVM logical versus bitwise distinctions, all six branch predicates and both
  outcomes at signed-i64 extremes, wrapping negation, persistent versions, PHI
  groups, cumulative/empty-array budgets, source limits, final fallthrough and
  references to non-producing instructions. Factorial through 12! and array
  Fibonacci through F(46) are compared with independent C++ recurrences, adapting
  the paper's appendix algorithms to this subset's syntax and result API.
- Original 5,056 Machinery checks, C++11 wrapper, both factorial engines, 49
  catalog factories, original native regressions, both fallback C-emitter paths,
  binary round trips, atomic-output and registry regressions are retained.

Reproduce from the MetaVM root (substitute your matching DSLtk directory):

```sh
make test
ASAN_OPTIONS=detect_leaks=0 make sanitize

cmake -S . -B out-meta -DCMAKE_BUILD_TYPE=Release -DMETAVM_TEST_CPP=ON \
  -DMETAVM_ENABLE_METAML=ON -DMETAVM_DSLTK_INCLUDE_DIR=/mnt/nvme/metatk/DSLtk
cmake --build out-meta -j4
ctest --test-dir out-meta --output-on-failure

cmake -S . -B out-clang -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Release -DMETAVM_TEST_CPP=ON \
  -DMETAVM_ENABLE_METAML=ON -DMETAVM_DSLTK_INCLUDE_DIR=/mnt/nvme/metatk/DSLtk
cmake --build out-clang -j4
ctest --test-dir out-clang --output-on-failure

cmake -S . -B out-meta-sanitize -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS_DEBUG='-O1 -g1' \
  -DMETAVM_TEST_CPP=ON -DMETAVM_SANITIZE=ON -DMETAVM_ENABLE_METAML=ON \
  -DMETAVM_DSLTK_INCLUDE_DIR=/mnt/nvme/metatk/DSLtk
ASAN_OPTIONS=detect_leaks=0 cmake --build out-meta-sanitize -j2
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir out-meta-sanitize --output-on-failure
```

The compatible DSLtk and unchanged MetaML hashes are in
`third_party/DEPENDENCIES.md`. No LeakSanitizer pass is claimed. MetaTeX, libdrm
hardware, expanded installation/export and Windows/macOS remain unvalidated.
Forty-four catalog entries remain catalog-only; successful authoring tests do not
change native coverage. Full SSA dominance/type analysis is still future work.

---

# Second-pass validation amendment

The current results and limitations are in `ROUND2.md`. The 5,056 core checks,
C++11 wrapper, all 49 catalog examples and original bytecode/threaded examples
remain passing. Added Definition, SSA source, transactional binding, atomic-output,
CLI preservation, generated-definition C execution and public-header tests pass.
The refinement suite passes with `-O2 -DNDEBUG`; ASan/UBSan passes with leak
detection disabled. Matching MetaML/MetaTeX/libdrm dependencies and CMake remain
unavailable or incompatible. No leak-check or optional-integration pass is claimed.

The exact prior validation record is retained below.

---

# Observed validation

Tested on Linux x86-64 with GCC C11/C++17. No CMake, pkg-config, libdrm
headers/device, matching DSLtk.hpp, or complete MetaTeX dependency graph was
available. These observations describe this edition, not full guest conformance.

## Passed

- Dependency-free fallback compile and link of the C11 Machinery core, VMWeave
  CLI, all 49 C/C++ catalog profiles, native implementations and legacy adapters.
  New build uses `-Wall -Wextra -Wpedantic -Werror`.
- Existing Machinery regression suite: **5,056 checks passed**.
- Existing C++11 ownership wrapper test.
- Original factorial example under bytecode and threaded engines: **3628800**.
- All 49 portable module factories verify and return **42** under both engines.
  DRMVM's example also verifies its host-bound drawing effect.
- CHIP-8 arithmetic carry, shifts/quirk switch, BCD, drawing/XOR collision,
  subroutine return, key wait/resumption, timers and memory/stack/ROM bounds.
- SSAVM paper figure-5 Fibonacci loop: **55** with staged PHI/PFE semantics;
  persistent array versions, invalid operands, malformed PHI group,
  undefined register, array bounds and fuel traps.
- JVM integer expression/local arithmetic: **42**; backward-branch loop: **5**;
  malformed input, branch alignment, undefined local, divide-zero and fuel traps.
- FORTH colon word execution: **42**; stack words, signed true value and
  stack/arithmetic/recursion-fuel traps.
- DRMVM XRGB masking, clipped rectangles, dimensions, host binding and execution.
- Typed VMWeave builder, identifier/register/opcode validation, source limits,
  unfinished function rejection, module finalization and execution.
- C emitter output compiled and linked by the real C compiler; factory module
  verification, invalid identifier rejection and binary round trip.
- Final line-wrapped C-emitter output compiled with warnings treated as errors.
- All 49 profile specifications checked for current schema properties,
  unique identities, manifest agreement and required file references.
- Native integration tests also passed with `-O2 -DNDEBUG` (checks remain active).
- Python build/generation/validation scripts passed syntax compilation.
- AddressSanitizer + UndefinedBehaviorSanitizer: Machinery suite, C++ wrapper,
  native/catalog integration, both factorial engines, emitted-C factory and
  serialized module execution passed with `ASAN_OPTIONS=detect_leaks=0`.

## Limitations

LeakSanitizer could not inspect `/proc` in this environment and terminated before
leak analysis. Leak detection was disabled for the sanitizer execution; **no
leak-check pass is claimed**. A runner lost its executable bit on relinking;
restoring it allowed the remaining sanitized stages to pass. The fallback build
now sets the executable owner bit after successful links.

The optional MetaML adapter, VMDesc/MetaTeX adapter and libdrm allocation/upload
branch have not been compiled or run. No fake dependency headers were used to
claim validation. CMake configuration/build/install paths are provided but were
not exercised because CMake is absent. The fallback build is the observed path.

Portable catalog tests are not tests of the corresponding historical ISA.
Forty-four native guest runtimes remain unimplemented. The five native subsets
lack full external image loaders and platform runtimes. CHIP-8 uses direct ROM
loading with classic instructions; no external compatibility ROM suite was run.
The file `docs/COVERAGE.md` and each `.vmspc` record the distinction explicitly.

No complete architecture-to-native-backend generator, JIT, differential guest
conformance suite, GPU presentation/modesetting path, or full multi-runtime
implementation is claimed. See AGENTS.md for refinement priorities.
