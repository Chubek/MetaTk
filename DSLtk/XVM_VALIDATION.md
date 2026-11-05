# XVM validation — 2026-11-05

XVM is the default executor for both MetaML dialects and MetaVM Machinery.
MetaML expressions now lower to stack bytecode and native helper-call blocks;
execution does not delegate to the retained tree evaluator. Machinery's verified
instruction set compiles to its own block plan with native/portable tiers.
These remain different instruction sets and value models.

## Observed results

Linux amd64; GCC 16.2.1, Clang 22.1.8. All results below were actually executed.
Elapsed times include concurrent machine load and are not performance claims.

| Configuration | Result | Elapsed |
| --- | --- | ---: |
| MetaML configured C++/D suite, both C++ CLIs and C adapter | 28/28 | 29.26 s |
| Included tree/portable/automatic/native differential | 270 comparisons | 17.33 s |
| MetaML Clang ASan/UBSan runtime + toolchain tests | 2/2, leak detection disabled | 6.80 s |
| MetaVM GCC Release + current MetaML | 25/25 | 6.75 s |
| MetaVM Clang Release + current MetaML | 25/25 | 6.97 s |
| MetaVM GCC Release shared libraries, lib64, relocated SDK | 23/23 | 2.54 s |
| MetaVM Clang Debug ASan/UBSan, MetaML disabled | 23/23, default leak detection | 2.77 s |
| Machinery GCC ASan/UBSan | 5/5, default leak detection | 0.11 s |
| Machinery GCC Release, native tier disabled | 5/5 | 0.01 s |
| Installed MetaML C++ and D SDK consumers | Both compile and execute 42 through XVM | — |

The original Machinery suite grows from 5,056 to **8,446 checks**, retaining its
mutation and parser coverage and adding third-engine result/fuel comparisons.
Dedicated XVM tests perform another **153 checks**. All 49 generated portable
catalog factories execute under the three engines; this is not evidence of new
historical guest runtime compatibility.

Before this work: MetaML passed 26/26 in 10.88 s; MetaVM GCC Release passed 22/22
in 2.32 s; standalone Machinery Clang sanitizers passed 4/4 in 0.06 s. Additional
test cases, real MetaML integration and enabled REPL coverage make the final
totals/timings intentionally different. No execution speedup was measured.

## Reproduction

From the DSLtk source directory:

```sh
cmake -S . -B /tmp/opencode/metaml-toolchain-build \
  -DMETAML_BUILD_REPL=ON -DMETAML_BUILD_C_API=ON -DDSLtk_TEST_D=ON
cmake --build /tmp/opencode/metaml-toolchain-build -j2
ctest --test-dir /tmp/opencode/metaml-toolchain-build --output-on-failure -j2

cmake -S ../MetaToolchain/MetaVM -B /tmp/opencode/metavm-xvm-full \
  -DCMAKE_BUILD_TYPE=Release -DMETAVM_TEST_CPP=ON \
  -DMETAVM_ENABLE_METAML=ON -DMETAVM_DSLTK_INCLUDE_DIR="$PWD"
cmake --build /tmp/opencode/metavm-xvm-full -j2
ctest --test-dir /tmp/opencode/metavm-xvm-full --output-on-failure -j4
```

Repeat the MetaVM configuration in a separate directory with
`-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++` for Clang Release.
Shared packaging uses `-DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_LIBDIR=lib64`.
Clang's full MetaVM sanitizer configuration uses Debug,
`-DMETAVM_SANITIZE=ON -DCMAKE_CXX_FLAGS_DEBUG=-g1`, with MetaML disabled.
Standalone Machinery uses the same sanitizer option; portable-only coverage adds
`-DMETAVM_XVM_NATIVE=OFF`.

MetaML sanitizer targets were built with Clang and
`-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined`
`-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined`, REPL/D tests disabled.
Run the built `metaml_runtime_tests` and `metalib_toolchain_tests` through:

```sh
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/opencode/metaml-xvm-clang-sanitize \
  -R '^(metalib_toolchain|metaml_runtime)$' --output-on-failure
```

Installed MetaML consumers were compiled against only the installed include
directory. C++ used C++20, pthread and `-Wall -Wextra -Wpedantic -Werror`.
D linked the installed `MetaML.d` and `DSLtk.d`; the new XVM modules are templates
and require no additional object files. Both checked the default engine, result
42 and a nonzero compiled-instruction counter.

## Limits and remaining validation

This is a full-expression **baseline helper-call JIT**, not an optimizing native
compiler with register allocation/inlined arithmetic. Runtime helpers still
implement boxed values, pattern matching, module forcing and comprehensions;
their expressions execute through XVM. Source-backed chunk v1 persistence is
retained deliberately so destination-session checking and permissions apply.

Only Linux amd64 native execution was exercised. Aarch64/Riscv64 emitters and
Windows/macOS mappings need target execution validation. Hardened platforms can
use portable compiled XVM; strict native mode reports unavailability. Neither
runtime claims process-wide memory sandboxing or bounded host callback time.

MetaML sanitizer tests disable leak detection because retained checker/environment
allocations are not resolved by this backend change. This is not a leak-clean
claim; the complete MetaML suite was not sanitizer-tested. MetaVM's full Clang
sanitizer suite and standalone GCC Machinery suite ran with default leak
detection. The optional MetaTeX/libdrm integrations remain unvalidated here.

MetaVM builds used their existing warnings-as-errors policy. Enabling the full
MetaML CLI also exposed existing vendored replxx warnings (UTF-8 table bounds,
C++20 `char8_t` compatibility and old CMake policy); that dependency was not
modified. Do not describe the entire workspace build as warning-free.

API/ownership/limits are documented in [XVM.md](XVM.md) and
`../MetaToolchain/MetaVM/docs/XVM.md`. Supplied third-party dependencies,
generated architecture data and catalog guest-coverage boundaries were not
changed by this task. No commit or push was issued by this assistant.
