# 3. Building MetaIL

You need CMake, a C11 compiler, a C++20 compiler, Python 3 for tests, the Lua 5.4
shared library, MetaNum headers, DSLtk's `DSLtk.hpp`, Weighstone, and Boost headers. DSLtk's C++ features are
why the native DSL requires C++20; the C interface remains C11.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DMETANUM_ROOT=/mnt/nvme/metatk/MetaNum \
  -DDSLTK_ROOT=/mnt/nvme/metatk/DSLtk \
  -DWEIGHSTONE_ROOT=/mnt/nvme/metatk/MetaToolchain/weighstone
cmake --build build -j2
ctest --test-dir build --output-on-failure
build/metail verify
python3 tools/generate_kernels.py --check
```

The dependency defaults are `../../MetaNum` and `../../DSLtk`, relative to the
MetaIL source directory; Weighstone defaults to `../weighstone`. Override them when building elsewhere. Lua is found as
a system shared library; Debian-family systems provide it with `liblua5.4-dev`.

The library target is `metail_core`, exported through `MetaIL::MetaIL`; the CLI is
`metail`. C++ examples are enabled by default. Set `METAIL_BUILD_EXAMPLES=OFF` to
omit them, or `BUILD_SHARED_LIBS=ON` to build a shared core library.

For a separate compiler configuration use another build directory and set
`CMAKE_C_COMPILER` and `CMAKE_CXX_COMPILER`. Do not switch compilers in an existing
configured build tree. A successful build alone is not the semantic check: run
CTest, the rewrite verifier, and the generator reproducibility check as shown.

[Contents](README.md) · [Next: Installation](04-installation.md)
