# Chapter 4 — The Make/Python fallback build

[Previous](03-prerequisites-and-dependencies.md) · [Contents](README.md) · [Next](05-cmake-builds.md)

## Standard workflow

From `MetaToolchain/MetaVM`:

```sh
make
make test
make sanitize
```

These invoke `tools/build.py`, respectively without flags, with `--test`, and
with `--test --sanitize`. Python can be used directly when Make is unavailable:

```sh
python3 tools/build.py --test
```

The script compiles all selected sources on every invocation, deliberately
avoiding stale results after public-header changes. It runs up to four compiler
jobs concurrently. Its normal flags include `-g -O1 -Wall -Wextra -Wpedantic
-Werror`; this is not the CMake Release configuration.

## Compiler selection and outputs

```sh
CC=clang CXX=clang++ python3 tools/build.py --test
```

`CC` and `CXX` select compiler executable names/paths. Use a real compiler path,
not a shell command string with embedded flags. The fallback uses GCC/Clang-style
options and `ar`; use the CMake build for other toolchains and packaging.

Normal output goes to `build/`; sanitizer output goes to `build-sanitize/`.
Each contains a combined `libmetavm.a`, command-line tools, tests, and generated
portable examples under `generated/`. This combined fallback archive is not
the same target layout as CMake's separate Machinery, VMWeave, and catalog
libraries. Do not write consumer link commands by mixing the two layouts.

## What the test mode exercises

The script validates metadata and the legacy manual, checks public headers,
builds and runs the 5,056-check Machinery suite, tests the C++11 wrapper, and
executes native semantic regressions. It generates all 49 example binaries and
C factories, compiles the emitted C, and checks parity/execution under both
engines. It also checks Definition C generation, CLI preservation, SSAVM
analysis, DVI interoperability when independent tools are present, and pinned
CHIP-8 ROMs.

Successful fallback tests do not validate CMake installation or shared-library
deployment. They also do not build the optional MetaML, MetaTeX, or libdrm
integrations. Use the corresponding CMake configurations for those capabilities.

## Sanitizers and cleaning

Sanitizer mode adds ASan/UBSan, frame pointers, and its non-PIE compile/link flags.
Run it with ordinary leak detection first. Where LeakSanitizer cannot inspect
the process environment, a documented restricted-environment workaround is:

```sh
ASAN_OPTIONS=detect_leaks=0 make sanitize
```

Record that exception; it is not evidence of a leak-free run. The optional
MetaML dependency has a separately documented retained-allocation finding.

`make clean` removes the fallback's two build directories. They must contain
only rebuildable artifacts, never source or manually maintained documents.
`make regenerate` regenerates catalog-owned source companions; it is not a
clean operation and should be reviewed like a source edit.

**References:** [fallback implementation](../tools/build.py), [validation](39-validation-and-troubleshooting.md).
