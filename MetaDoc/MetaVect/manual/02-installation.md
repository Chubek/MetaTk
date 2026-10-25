# Chapter 2 — Installation and Build

**Status:** implemented. The CMake build configures and compiles on a stock
Linux toolchain.

## Prerequisites

- CMake 3.20 or newer.
- A C/C++ compiler (GCC or Clang).
- A D compiler for the syntax frontend: LDC (`ldc2`), DMD (`dmd`), or GDC
  (`gdc`). Any one of the three works; the build auto-discovers them.
- Optional: `luac` for syntax-checking the Lua rewrite specs, `vim` for the
  editor test, `python3` for the grammar and TeX checks, `pdflatex` for the
  document build check.

No external libraries are needed for the first milestone.

## Building

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

This produces `build/mvect` (the driver) and `build/test_frontend` (the
contract tests), plus the `metavect` static C library.

## Build options

| Option | Effect |
| --- | --- |
| `-DMETAVECT_D_COMPILER=/path/to/compiler` | Force a specific D compiler. |
| `-DMETAVECT_BUILD_FRONTEND=OFF` | C-only build; skips `mvect` and frontend tests. |
| `-DMETAVECT_BUILD_C_API=OFF` | Skip the static C library. |
| `-DMETAVECT_BUILD_TESTS=OFF` | Skip all tests. |

If no D compiler is installed, CMake reports that the frontend is skipped
and builds the C API alone.

## Validating the rest of the tree

```sh
luac -p mvir/rewrite-specs/*.lua
find . -path ./build -prune -o -type f -empty -print
grep -RIn --exclude-dir=build -E 'TODO|FIXME' . | grep -v AGENTS.md
git diff --check
```

The Lua specs are declarative metadata only; `luac -p` checks their syntax
without executing anything.
