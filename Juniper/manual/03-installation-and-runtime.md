# 3. Installing and locating runtime resources

A Juniper deployment is more than a copied executable. Macro loading needs GPPX
and `Juniper.xpp`; MetaIL operations need Lua files and optimization kernels.
Install the complete project, or configure those resources explicitly.

## Install a standalone build

```sh
cmake --install "$JUNIPER_ROOT/build" --prefix "$WORK/install"
"$WORK/install/bin/juniper" check \
  "$WORK/install/share/juniper/examples/Juniperfile"
```

When GPPX and MetaIL were built as source dependencies, the install includes their
runtime files and package targets as well. If they were found as existing system
packages, installing Juniper does not copy those independent packages; keep their
installations available or install them separately.

A typical prefix contains:

```text
bin/juniper
bin/gppx                          # when installed by the source dependency
lib/libJuniper.*
lib/cmake/Juniper/
include/juniper/Juniper.hpp
include/juniper/MetaArch.hpp
share/juniper/stdlib/Juniper.xpp
share/metail/lua/
share/metail/optimize-kernels/
share/doc/juniper/manual/
```

Library directories may be `lib64` or another GNUInstallDirs location. Do not
hard-code `lib` into a consumer when `find_package` can locate the package.

## Override resources deliberately

The CLI accepts explicit options:

```sh
"$J" check "$MANUAL_EXAMPLES/affine.jun" \
  --gppx "$JUNIPER_ROOT/build/gppx/gppx" \
  --stdlib "$JUNIPER_ROOT/stdlib/Juniper.xpp" \
  --metail-home "$METAIL_HOME"
```

The GPPX path above assumes it was built from source, not imported as an installed
package. In an installed deployment use that installation's actual binary path.
Environment variables provide convenient process-wide defaults:

```sh
export JUNIPER_GPPX="$WORK/install/bin/gppx"
export JUNIPER_STDLIB="$WORK/install/share/juniper/stdlib/Juniper.xpp"
export METAIL_HOME="$WORK/install/share/metail"
"$WORK/install/bin/juniper" check "$MANUAL_EXAMPLES/affine.jun"
```

For GPPX and the prelude, explicit API/CLI paths take precedence over environment
variables. Discovery then tries executable-relative and configured build/install
locations. On Linux, executable-relative discovery uses `/proc/self/exe`.
Nonstandard layouts, relocated library consumers, and systems without `/proc`
should use explicit paths rather than depending on a development checkout.

## Consume the installed C++ package

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyJuniperTool LANGUAGES CXX)
find_package(Juniper CONFIG REQUIRED)
add_executable(my_tool main.cpp)
target_link_libraries(my_tool PRIVATE Juniper::Juniper)
```

Configure with `-DCMAKE_PREFIX_PATH=/path/to/prefix`. The package publishes
`Juniper::Juniper` and `Juniper::juniper`; it also loads `juniper_compile`.
The optional source-build adapter target `Juniper::MetaArch` is **not** part of
that installed export. Chapter 32 explains how to supply its MetaArch dependency.

**Deployment check:** launch the installed executable outside the source tree,
with the intended environment. An old `METAIL_HOME` overrides automatic lookup,
so inspect it before diagnosing a supposedly broken relocation.

[Previous](02-building.md) · [Index](README.md) · [Next](04-first-juniperfile.md)
