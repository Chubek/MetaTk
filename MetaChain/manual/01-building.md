# 1. Building, dependencies, and first programs

## Requirements

* A C++20 compiler and standard library.
* `DSLtk.hpp` from the sibling DSLtk project.
* CMake 3.20 or newer for the optional build targets.
* Python 3 for optional black-box integration tests.

The core has no separately compiled runtime. It needs neither Boost.JSON nor
Lua. Enabling Symspec metadata support adds Symspec's own dependencies; it is
not needed for the built-in assembler or native backend.

## Include-path configurations

For the MetaTk checkout layout:

```sh
c++ -std=c++20 -I/mnt/lambda/metatk/MetaChain \
    -I/mnt/lambda/metatk/DSLtk examples/quickstart.cpp -o quickstart
./quickstart
```

`metachain/Common.hpp` first honors `METACHAIN_DSLTK_HEADER`, then searches
`<DSLtk.hpp>`, then the sibling relative path `../../DSLtk/DSLtk.hpp`. An
explicit header override is useful when embedding the libraries under another
layout:

```cpp
#define METACHAIN_DSLTK_HEADER "vendor/DSLtk.hpp"
#include "MetaChain.hpp"
```

Define configuration macros consistently in every translation unit. Changing
the definition of a header-only type between translation units violates the
one-definition rule.

Individual component headers can also be included directly. `ParseTk.hpp`,
`Rewrite.hpp`, and `MetaVM.hpp` do not require DSLtk. `MetaIL.hpp`, `ASMtk.hpp`,
`LDtk.hpp`, `Native.hpp`, and `Text.hpp` reach DSLtk through `Common.hpp`.

## CMake

From the MetaChain directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Build options:

| Variable | Default | Purpose |
|---|---|---|
| `METACHAIN_DSLTK_DIR` | sibling `../DSLtk` | DSLtk include directory |
| `METACHAIN_BUILD_TESTS` | `ON` | unit, example, and integration tests |
| `METACHAIN_BUILD_CLI` | `ON` | textual command driver |
| `METACHAIN_SANITIZERS` | `OFF` | ASan and UBSan on GCC/Clang executable targets |

For an embedding project:

```cmake
set(METACHAIN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(METACHAIN_BUILD_CLI OFF CACHE BOOL "" FORCE)
add_subdirectory(path/to/MetaChain)
target_link_libraries(my_application PRIVATE MetaChain::MetaChain)
```

The interface target propagates C++20 and include paths. It does not install
system packages, download dependencies, or add platform-specific runtime
libraries. There is no install/export package supplied by this directory;
vendor the headers and DSLtk together or provide the include paths explicitly.

## First program: execute text

```cpp
#include "MetaChain.hpp"
#include <iostream>

int main() {
    metachain::Toolchain toolchain;
    auto result = toolchain.run(
        "const r0, 40\n"
        "const r1, 2\n"
        "add r0, r0, r1\n"
        "print r0\n"
        "halt r0\n");
    if (!result) {
        for (const auto& d : result.diagnostics)
            std::cerr << metachain::format(d) << '\n';
        return 1;
    }
    std::cout << result->value << '\n'; // 42
}
```

`print` records output in `Execution::output`; it does not write to the host's
stdout. The driver decides how to display recorded output. Native executables
use Linux `write` for `print`.

## First native executable

```sh
build/metachain build examples/factorial.mcil -o factorial
chmod +x factorial
./factorial
```

The program prints `720`. The shell exit status is `720 & 255`, namely 208.
Writing the ELF image does not automatically grant execute permission. Native
files are intended for a Linux x86-64 host; generating bytes and executing those
bytes are separate operations.

## Optional Symspec metadata

```cpp
#define METACHAIN_ENABLE_SYMSPEC
#include "MetaChain.hpp"
```

Make `ArchTk/Symspec.hpp` and Boost.JSON available and link Boost.JSON when
using Symspec functionality. `Architecture` then exposes `get`, `forms`,
`register_info`, and `record`, and `Assembler::load_architecture` becomes
available. Returned views borrow from the retained immutable specification.
Loading a non-x86-64 architecture does not install a corresponding encoder:
instruction assembly reports the unsupported target.
