# Installed SDK

The root CMake project installs Machinery, VMWeave, the catalog/native subsets,
public headers, catalog metadata/examples, documentation and built command-line
tools. CMake 3.18 or newer is required. Dependencies remain optional.

```sh
cmake -S . -B out-sdk -DCMAKE_BUILD_TYPE=Release -DMETAVM_TEST_CPP=ON
cmake --build out-sdk -j4
ctest --test-dir out-sdk --output-on-failure
cmake --install out-sdk --prefix /your/sdk/prefix
```

Consumers configure with `-DCMAKE_PREFIX_PATH=/your/sdk/prefix`:

```cmake
find_package(MetaVM 0.2 CONFIG REQUIRED COMPONENTS Framework)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE MetaVM::Framework)
```

| Target / component | Interface |
|---|---|
| `MetaVM::Machinery` | C11 execution substrate and C++11 wrapper |
| `MetaVM::VMWeave` | C bootstrap compiler/emitter; links Machinery |
| `MetaVM::Authoring` | C++17 Builder/Definition headers and VMWeave |
| `MetaVM::Catalog` | C++17 catalog, native subsets and legacy factories |
| `MetaVM::Framework` | Authoring and Catalog together |

All components are installed together; component selection validates names, not
per-machine builds. The standalone `find_package(MetaVMMachinery CONFIG REQUIRED)`
package and its `MetaVM::Machinery` target remain usable.

Include `MetaVM.h`/`MetaVM.hpp` for the framework, or paths such as
`vmweave/Definition.hpp`, `VMLibrary/CHIP-8/Chip8.hpp` and
`Machinery/MetaVM.hpp`. The directory layout preserves relative header includes
and the historical `Smaltalk-80` spelling. Generated C factories need Machinery;
calling the C compiler/emitter API requires VMWeave.

Default locations are `include/MetaVM`, `lib/cmake/MetaVM`, `bin`, and
`share/MetaVM` (GNUInstallDirs may choose another library directory). Metadata
and portable `.mvm`/`.vmweave`/`.vmspc`/`.vmdesc` examples are under
`share/MetaVM/VMLibrary`. Use relative `CMAKE_INSTALL_*DIR` values to retain prefix
relocation. If CMake does not search a custom library directory, also pass
`-DMetaVM_DIR=/your/sdk/prefix/lib64/cmake/MetaVM` (adjust `lib64` as needed).
The framework config locates its co-installed Machinery package relative to itself.
Linux shared libraries/tools use relative `$ORIGIN` runpaths;
`BUILD_SHARED_LIBS=ON` and `CMAKE_INSTALL_LIBDIR=lib64` are tested together.
Absolute external dependencies retain their own platform deployment requirements.

`vmweave`, `ssavm-run`, `closure-vm-demo`, and `vmweave-definition-demo` are installed; `metavm-run`
is included when `METAVM_BUILD_EXAMPLES=ON`. Optional `vmweave-metaml` and `vmdesc`
are installed only when built. MetaML/MetaTeX adapters are installed as headers,
not as automatically dependency-configured CMake components: using them requires
C++20 and the real matching DSLtk/MetaTeX dependencies. The unchanged supplied
MetaML header and its provenance are included; DSLtk is not vendored. A libdrm-
enabled package discovers libdrm through pkg-config in its consuming project.

`metavm_installed_sdk` stages an install, renames its prefix, invokes the installed
tools, emits C and builds/runs standalone C11 and C++17 consumers against that
prefix. It exercises native CHIP-8, SSAVM, Definition execution, catalog lookup
and the legacy Smalltalk alias. Installed consumers also execute the native SECD
and Krivine APIs; `closure-vm-demo` exercises both installed implementations.
The installed SSAVM API and `ssavm-run --check`
also exercise the optional definite-assignment/type analyzer. Sanitized exports propagate
link flags to consumers. Native compiler/platform compatibility is still required;
there is no stable C++ binary ABI promise. Windows/macOS builds and optional
MetaTeX/libdrm package configurations remain unvalidated.
