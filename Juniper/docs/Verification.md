# Juniper verification

Verified on Linux, 2026-10-28, with GCC 16.2.1 and Clang 22.1.8.

## Scope and baseline

This change implements Juniper, not the separate MetaArch architecture-data
hardening brief. No architecture specifications, generated database backends,
MetaIL kernels, Lua semantics, or existing public C APIs were changed.

Before implementation:

* MetaIL: **18/18** tests passed (4.06 s).
* MetaIL verifier: **230** declarative rules, **183473** signed-64 cases,
  including **2470** effect/trap cases.
* MetaArch: **25/25** tests passed (11.92 s), using its existing system YAML/XML
  dependencies and fetched yyjson 0.10.0. Ninja was unavailable, so Make was used.
* The current GPPX source compiled and reported its version; its CMake file and
  the Juniper directory were initially empty. Unrelated pre-existing deletions
  and modifications were left intact.

## Implemented API and build changes

* Owned expression/statement/function/module IR, backend patterns/rules/resources,
  typed machine operands/instructions/functions/modules, diagnostics and results.
* `preprocess`, `Compiler::{load,parse,check,emit,emit_metail,optimize,analyze}`,
  `select`, `schedule`, and deterministic machine JSON emission.
* Optional `FormBinding`, `RegisterBindings`, `encode_instruction` adapter over
  real MetaArch checked forms, with explicit parameter and physical-register maps.
* `Juniper::Juniper`, `Juniper::juniper`, optional source-build `Juniper::MetaArch`,
  installed CMake package, `juniper_compile` with depfiles, presets and CI matrix.
* GPPX executable/package target `GPPX::preprocessor`, distinct from the deleted
  historical GPPX C++ API. Freed the borrowed include filename after `DoInclude`
  to fix the allocation leak found by LeakSanitizer. Existing upstream unused
  parameter/fallthrough diagnostics are suppressed only on the GPP target.
* MetaIL gains `METAIL_BUILD_TESTS` (default ON), preserving all existing tests,
  while allowing dependency builds without Python/test executables.
* Root MetaTk enables Juniper with `METATK_BUILD_JUNIPER`; the absent EkippX
  directory is now conditional rather than blocking root configuration.

## Final results

All builds use `-Werror`; Juniper targets use `-Wall -Wextra -Wpedantic`.
Sanitizer builds additionally apply those warning flags globally.

| Configuration | Combined CTest result | Last measured wall time |
|---|---:|---:|
| GCC Release | 19/19 | 5.83 s |
| Clang Release | 19/19 | 6.21 s |
| GCC ASan + UBSan, leak detection enabled | 19/19 | 15.01 s |
| Clang ASan + UBSan, leak detection enabled | 19/19 | 14.31 s |

The combined suite includes five Juniper cases and fourteen MetaIL dependency
cases (MetaIL's four API examples are disabled in the dependency build).
`juniper_test` performs **3995 checks**, including independently interpreted
source/selected/scheduled comparisons on fixed-seed random arithmetic and
boundary values; effects, lazy/nested selects, traps, eager logical operators,
loops, mutable-register hazards, costs, bounded immediates, scheduling resources,
dependency latencies, deterministic rescheduling and invalid input.

CLI tests cover canonical round trips, real passes, explicit runtime roots,
transitive/conditional includes, paths containing spaces and shell metacharacters,
depfiles, input/hardlink alias protection, unchanged outputs after errors,
disabled `#exec`, process deadlines and temporary-file cleanup. The CMake test
changes an included macro and verifies that the artifact actually rebuilds.
The optional MetaArch test checks exact synthetic form bytes, immediate rejection
and missing physical assignments. It makes no unverified ISA claims.

Additional checks: root `juniper` target; tests-off/Python-free configuration;
static and shared builds; installation under a nonconfigured prefix; installed
CLI/runtime discovery; and an external `find_package(Juniper)` consumer with a
generated scheduling artifact. The final MetaIL verifier still reports all
230 rules and 183473 signed-64 cases. No sanitizer suppression was added.

## Reproduce

From the workspace root:

```sh
cmake -S juniper -B juniper/build-release -DCMAKE_BUILD_TYPE=Release \
  -DJUNIPER_WITH_METAARCH=ON -DCMAKE_C_FLAGS=-Werror -DCMAKE_CXX_FLAGS=-Werror
cmake --build juniper/build-release -j4
ctest --test-dir juniper/build-release --output-on-failure -j4
juniper/build-release/metail/metail verify
python3 MetaToolchain/MetaIL/tools/generate_kernels.py --check

cmake -S juniper -B juniper/build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DJUNIPER_WITH_METAARCH=ON \
  '-DCMAKE_C_FLAGS=-Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build juniper/build-asan -j4
ctest --test-dir juniper/build-asan --output-on-failure -j4
```

Repeat with `-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++` in separate
build directories. Omit `JUNIPER_WITH_METAARCH` for the ordinary dependency-light
build. Its optional yyjson dependency follows MetaArch's existing FetchContent
policy; a local source can be supplied through `FETCHCONTENT_SOURCE_DIR_YYJSON`.

## Explicit limits and unverified areas

No register allocation, ABI lowering, object emission, memory model, SSA,
macro sandbox, full expansion source map or machine-JSON importer is claimed.
Backend opcode semantics are assertions supplied by the profile author.
Scheduling models pipelined launch capacities, not a real processor.
Ninja support uses CMake's standard depfile contract but was not locally run
because Ninja is not installed. Windows is unsupported by the POSIX driver;
macOS discovery requires explicit runtime/resource paths when `/proc` is absent.
The optional fetched yyjson emits an existing CMake minimum-version deprecation
notice; compiled code is warning-clean under the stated flags.
