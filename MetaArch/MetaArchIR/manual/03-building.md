# Chapter 3 — Building and installing

## Configure a source build

Use a C++20 compiler, CMake and a supported build tool. MetaArch's existing C
library dependencies remain necessary; MetaArchIR adds DSLtk headers and the
MetaArch DSL library to the consumer's requirements.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DARCHDB_BUILD_DSL=ON -DARCHDB_BUILD_TOOLS=ON
cmake --build build --parallel 4
build/marchir --version
```

If DSLtk is not beside the MetaArch checkout, supply its header directory:

```sh
cmake -S . -B build -DARCHDB_DSLTK_DIR=/path/to/DSLtk
```

That directory must contain `DSLtk.hpp`. A missing-header configuration error
should be fixed by selecting the correct directory, not by disabling the DSL
component while expecting `marchir` to remain available.

## Link an application

When MetaArch is part of your CMake build, use its interface target:

```cmake
add_subdirectory(/path/to/MetaArch metaarch-build)
add_executable(my_assembler main.cpp)
target_link_libraries(my_assembler PRIVATE MetaArch::ir)
```

Include `MetaArchIR/MetaArchIR.hpp` in `main.cpp`. The target carries the DSLtk
include directory and the MetaArch DSL/C library dependency chain. Inline API
implementation does not mean a dependency-free, single-source executable.

## Install and locate the database

```sh
cmake --install build --prefix /tmp/metaarch-install
/tmp/metaarch-install/bin/marchir \
  --db /tmp/metaarch-install/share/metaarch/MetaArchDB --list-targets
```

Headers install beneath `include/metaarch`. Installed-header consumers must
provide DSLtk headers and appropriate library linkage themselves; the build-tree
`MetaArch::ir` alias should not be mistaken for a promise of an installed
`find_package` configuration. The CLI does not derive its database location from
its executable path: pass `--db` or configure `METAARCHDB`.

[Previous](02-components.md) · [Contents](README.md) · [Next](04-first-program.md)
