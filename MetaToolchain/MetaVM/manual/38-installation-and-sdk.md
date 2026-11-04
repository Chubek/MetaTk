# Chapter 38 — Installation, packages, and SDK consumers

[Previous](37-vmdesc-and-metatex.md) · [Contents](README.md) · [Next](39-validation-and-troubleshooting.md)

## Install a tested CMake build

```sh
cmake -S . -B out-sdk -DCMAKE_BUILD_TYPE=Release -DMETAVM_TEST_CPP=ON
cmake --build out-sdk -j4
ctest --test-dir out-sdk --output-on-failure
cmake --install out-sdk --prefix /your/sdk/prefix
```

The fallback archive is not an installed package. Use CMake to export targets,
stage shared libraries, or build relocated consumer applications.

## Exported target reference

```cmake
find_package(MetaVM 0.2 CONFIG REQUIRED COMPONENTS Framework)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE MetaVM::Framework)
```

| Target/component | Interface |
|---|---|
| `MetaVM::Machinery` | C11 core and C++11 wrapper |
| `MetaVM::VMWeave` | C assembly bootstrap/C emitter, links Machinery |
| `MetaVM::Authoring` | C++17 Builder/Definition plus VMWeave |
| `MetaVM::Catalog` | C++17 catalog/native subsets/legacy factories |
| `MetaVM::Framework` | Authoring and Catalog together |

All components install together; component names validate requested interfaces,
not a per-machine selective installation. For a C-only installation/consumer,
use the standalone Machinery project and `find_package(MetaVMMachinery CONFIG
REQUIRED)`, linking `MetaVM::Machinery`.

The wrapper/authoring/catalog standards differ. Prefer the exported target's
language requirements over global flags that happen to work on your machine.
An installed optional MetaML/MetaTeX adapter still requires C++20 and manually
provided real external dependencies; there is no automatic complete integration
component for them.

## Layout and relocation

Default destinations are `include/MetaVM`, `lib/cmake/MetaVM`, `bin`, and
`share/MetaVM`, with GNUInstallDirs allowed to choose another library directory.
Relative public-header layout is preserved for includes such as
`Machinery/MetaVM.hpp`, `vmweave/Definition.hpp`, and native profile paths.
Metadata/companions and focused `docs/` material are installed under share.
This new `manual/` directory is source-tree documentation; the existing install
rules do not automatically install it or the manual example project. Compiled
`generated/` artifacts are likewise separate build products.

Configure consumers with `-DCMAKE_PREFIX_PATH=/your/sdk/prefix`. For custom
`lib64` locations not discovered by CMake, additionally set
`-DMetaVM_DIR=/your/sdk/prefix/lib64/cmake/MetaVM`. The framework config locates
its co-installed Machinery config relative to itself.

Linux shared tools/libraries use relative `$ORIGIN` runpaths. Keep install
subdirectories relative to preserve relocation. External libraries retain their
own deployment requirements; a libdrm-enabled SDK discovers libdrm through
pkg-config in the consuming project. No stable cross-toolchain C++ ABI is promised.

## Tools and consumer validation

The install includes built framework tools: `vmweave`, SSAVM/DVI runners,
Definition/closure demos, `metavm-run` when enabled, and optional MetaML/VMDesc
executables only when built. Verify actual configured targets rather than
assuming every tool exists in a minimal installation.

`metavm_installed_sdk` stages an install, moves its prefix, runs installed
tools, emits C, and builds separate C11/C++17 consumers. It covers native APIs,
catalog/legacy lookup, authoring, analysis, and DVI replay. Sanitized exports
propagate required link flags. The manual's own consumer project is described
in [examples/README.md](examples/README.md).

**References:** [packaging contract](../docs/PACKAGING.md), [package template](../cmake/MetaVMConfig.cmake.in).
