# 4. Installation and CMake consumers

Install a built tree with:

```sh
cmake --install build --prefix /your/prefix
```

The install contains the CLI, native library, public headers, the selected DSLtk
header, CMake package files, Lua modules, optimization kernels, and this manual.
Runtime files normally live in `<prefix>/share/metail`. Public headers normally
live in `<prefix>/include/metail`; the CMake target supplies that include path.

A consumer's `CMakeLists.txt` can be:

```cmake
cmake_minimum_required(VERSION 3.16)
project(consumer LANGUAGES C CXX)
find_package(MetaIL CONFIG REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE MetaIL::MetaIL)
```

Configure with `-DCMAKE_PREFIX_PATH=/your/prefix`. For an in-tree integration use
`add_subdirectory(/path/to/MetaIL metail-build)` instead of `find_package`.

Even C-only application sources should enable CXX in the consumer project: the
static MetaIL library contains the C++ MetaNum binding. When linking manually,
compile C sources as C but use a C++ linker for the final executable.

Headers and library are not the whole runtime. Set
`METAIL_HOME=/your/prefix/share/metail` for a relocated application, or pass that
root to `Context`. The installed CLI also searches relative to its executable
path. Shared CLI installations use a relative library search path.

[Contents](README.md) · [Next: First textual program](05-first-textual-program.md)
