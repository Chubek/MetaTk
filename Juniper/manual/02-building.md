# 2. Building Juniper and setting up a workspace

Build the standalone Juniper project first. This avoids coupling your first
experiment to unrelated workspace projects and makes the executable path clear.

## Dependencies

You need C11 and C++20 compilers, CMake 3.24 or newer, and a POSIX environment.
MetaIL uses the real Lua 5.4 runtime, system Boost headers, and the workspace's
MetaNum, DSLtk, and Weighstone headers. Python 3 is needed for tests and the
manual's example checker, not for running the ordinary CLI.

On Debian/Ubuntu, typical system prerequisites include:

```sh
sudo apt-get install build-essential cmake python3 liblua5.4-dev libboost-dev
```

Package names differ on other distributions. The optional MetaArch adapter also
uses MetaArch's YAML/JSON/XML dependencies; it prefers installed packages and may
fetch missing dependencies under MetaArch's existing policy.

## Build and verify

From the workspace root:

```sh
export JUNIPER_ROOT="$PWD/Juniper"
export J="$JUNIPER_ROOT/build/juniper"
export METAIL_HOME="$PWD/MetaToolchain/MetaIL"
export MANUAL_EXAMPLES="$JUNIPER_ROOT/manual/examples"
export WORK="$PWD/juniper-work"
mkdir -p "$WORK"

cmake -S "$JUNIPER_ROOT" -B "$JUNIPER_ROOT/build" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$JUNIPER_ROOT/build" -j4
ctest --test-dir "$JUNIPER_ROOT/build" --output-on-failure
"$J" --version
"$J" --help
```

`METAIL_HOME` is the directory containing **both** `lua/` and
`optimize-kernels/`; it is not the `lua/` subdirectory itself. Explicitly setting
it also prevents stale source paths in older builds from choosing a different
runtime while you follow the manual.

## Useful build variations

For encoding examples in chapter 32:

```sh
cmake -S "$JUNIPER_ROOT" -B "$JUNIPER_ROOT/build" \
  -DCMAKE_BUILD_TYPE=Release -DJUNIPER_WITH_METAARCH=ON
cmake --build "$JUNIPER_ROOT/build" -j4
```

For an embedding deployment that does not need tests or examples:

```sh
cmake -S "$JUNIPER_ROOT" -B "$WORK/minimal-build" \
  -DJUNIPER_BUILD_TESTS=OFF -DJUNIPER_BUILD_EXAMPLES=OFF
cmake --build "$WORK/minimal-build" -j4
```

The provided presets are an alternative. Run them from the source directory:

```sh
cd "$JUNIPER_ROOT"
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Preset binaries live in `Juniper/build/release/`, not `Juniper/build/`. If you use
presets, set `J="$JUNIPER_ROOT/build/release/juniper"`. The `sanitize` preset adds
ASan/UBSan. Use separate build directories for GCC and Clang rather than changing
the compiler inside an existing CMake cache.

## Finding sibling projects

GPPX and MetaIL are located as CMake packages first, then as source siblings.
For a nonstandard workspace layout, configure explicit source roots:

```sh
cmake -S /path/to/Juniper -B /path/to/build \
  -DJUNIPER_GPPX_ROOT=/path/to/GPPX \
  -DJUNIPER_METAIL_ROOT=/path/to/MetaIL \
  -DJUNIPER_METAARCH_ROOT=/path/to/MetaToolchain
```

The last root matters only when the optional adapter needs to build MetaArch.
These are build-time paths. Runtime overrides are discussed next.

[Previous](01-compilation-model.md) · [Index](README.md) · [Next](03-installation-and-runtime.md)
