# Chapter 5 — CMake configurations

[Previous](04-fallback-build.md) · [Contents](README.md) · [Next](06-first-programs.md)

## Recommended root build

```sh
cmake -S . -B out-manual -DCMAKE_BUILD_TYPE=Release -DMETAVM_TEST_CPP=ON
cmake --build out-manual -j4
ctest --test-dir out-manual --output-on-failure -j4
```

The root requires CMake 3.18 or newer and enables C and C++. Its default build
contains Machinery, VMWeave, the catalog/native library, runners/demos, and
generated examples. `METAVM_BUILD_EXAMPLES` controls the Machinery runner;
it does not disable every root-level demo or driver.

| Option/cache entry | Default | Meaning |
|---|---|---|
| `METAVM_BUILD_TESTS` | ON | Core and framework tests |
| `METAVM_BUILD_EXAMPLES` | ON | Machinery's `metavm-run` example |
| `METAVM_TEST_CPP` | OFF | Additional C++11 Machinery wrapper test |
| `METAVM_GENERATE_EXAMPLES` | ON | All catalog example binaries/C factories |
| `METAVM_WARNINGS_AS_ERRORS` | ON | GCC/Clang warning failures |
| `METAVM_SANITIZE` | OFF | GCC/Clang ASan/UBSan instrumentation |
| `METAVM_ENABLE_METAML` | OFF | Actual MetaML authoring CLI and tests |
| `METAVM_DSLTK_INCLUDE_DIR` | empty | Directory containing matching `DSLtk.hpp` |
| `METAVM_ENABLE_VMDESC` | OFF | Optional MetaTeX adapter CLI |
| `METAVM_METATEX_SOURCE_DIR` | empty | External MetaTeX source if no parent target |
| `METAVM_ENABLE_LIBDRM` | OFF | libdrm dumb-buffer backend |
| `BUILD_SHARED_LIBS` | OFF | Shared instead of static libraries |

## Useful variants

For sanitizer execution, configure a separate tree with `-DCMAKE_BUILD_TYPE=Debug
-DMETAVM_SANITIZE=ON`. Select Clang with `-DCMAKE_C_COMPILER=clang
-DCMAKE_CXX_COMPILER=clang++`; select the compiler when creating the tree, not
by changing it underneath existing objects.

To enable real MetaML in this workspace:

```sh
cmake -S . -B out-metaml -DCMAKE_BUILD_TYPE=Release \
  -DMETAVM_ENABLE_METAML=ON -DMETAVM_DSLTK_INCLUDE_DIR="$PWD/../../DSLtk"
cmake --build out-metaml -j4
ctest --test-dir out-metaml --output-on-failure
```

The build checks for the header's existence; actual compilation checks API
compatibility. Successful configuration alone is insufficient.

## Standalone core and installed targets

```sh
cmake -S Machinery -B out-machinery -DCMAKE_BUILD_TYPE=Release
cmake --build out-machinery -j4
ctest --test-dir out-machinery --output-on-failure
```

This is the C-only route unless C++ wrapper tests are enabled. Root consumers
can use `MetaVM::Machinery`, `MetaVM::VMWeave`, `MetaVM::Authoring`,
`MetaVM::Catalog`, or `MetaVM::Framework`. The build-tree alias `MetaVM::VM`
is not the exported replacement for the documented Machinery target.

Keep shared, sanitizer, optional-integration, and Release builds separate.
Multi-configuration generators additionally require an explicit `--config` and
matching CTest configuration. Installation and relocation are covered in
chapter 38; generating a build tree does not itself install an SDK.

**References:** [root CMake](../CMakeLists.txt), [Machinery CMake](../Machinery/CMakeLists.txt).
