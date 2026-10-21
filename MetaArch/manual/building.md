# Build and installation

## Dependencies

| Component | Requirements |
|---|---|
| Core | CMake 3.16+, C11 compiler, libyaml >= 0.2.5, yyjson >= 0.9, libxml2 >= 2.9; Unix math library |
| C++17 API | C++17 compiler; header-only wrapper around core ABI |
| DSL and assembler | C++20 compiler, sibling `DSLtk/DSLtk.hpp` or `ARCHDB_DSLTK_DIR` |
| Python | CMake 3.18+, Python >= 3.9 plus development headers, C++17, nanobind |
| D / Go tests | D compiler (`ldc2` or `dmd`), Go >= 1.20 with cgo, shared MetaArch |

System packages are preferred. When missing, `ARCHDB_FETCH_DEPS=ON` permits
FetchContent downloads of pinned codec dependencies. Disable it for system-only
builds. Offline builds may provide source directories using
`FETCHCONTENT_SOURCE_DIR_YYJSON`, `FETCHCONTENT_SOURCE_DIR_LIBYAML` and
`FETCHCONTENT_SOURCE_DIR_LIBXML2`. Those supplied versions need not match the
default pins; record the version when reporting results.

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release \
  -DARCHDB_BUILD_DSL=OFF
cmake --build build-release -j
ctest --test-dir build-release --output-on-failure
```

Use `-G Ninja` when Ninja is installed; the development environment used Unix
Makefiles. Select GCC/Clang with `CMAKE_C_COMPILER` and `CMAKE_CXX_COMPILER` in
a new build directory. Do not switch compiler/generator in an existing tree.

## Options

| CMake setting | Default / effect |
|---|---|
| `ARCHDB_FETCH_DEPS` | ON; fetch missing codec dependencies |
| `ARCHDB_BUILD_TOOLS` | ON; build `archdb`, plus `metaarch` when DSL is enabled |
| `ARCHDB_BUILD_TESTS` | ON; register CTest suite |
| `ARCHDB_BUILD_DSL` | ON; build C++20 DSL layer |
| `ARCHDB_DSLTK_DIR` | `../DSLtk`; location of `DSLtk.hpp` |
| `ARCHDB_TEST_CPP_API` | Follows DSL option; standalone C++17 API test |
| `ARCHDB_TEST_FOREIGN_BINDINGS` | OFF; D/Go tests, requires shared library |
| `ARCHDB_BUILD_PYTHON` | OFF; build nanobind extension |
| `ARCHDB_TEST_PYTHON_BINDINGS` | Follows foreign-test option; implies Python build |
| `ARCHDB_NANOBIND_DIR` | Empty; use local checkout if set, installed package otherwise |
| `ARCHDB_PYTHON_INSTALL_DIR` | `lib/pythonX.Y/site-packages/metaarch` (uses configured libdir) |
| `BUILD_SHARED_LIBS` | Standard CMake switch; unset means static MetaArch |

Tests require `ARCHDB_BUILD_TESTS=ON`. Disabling both DSL and the C++17 test
keeps the core build C-only, unless Python is enabled. Fetched codec libraries
are static/private even when MetaArch is shared.

## Install and consume

```sh
cmake --install build-release --prefix /your/prefix
```

Headers install to `include/metaarch`, libraries to the configured libdir,
tools to `bin`, specs/catalogue to `share/metaarch`, and this manual under the
CMake documentation directory. D declarations install to `include/d`; Go
sources to `share/metaarch/bindings/go`. Python installs only when built.
Manual links to source headers, fixtures and other repository guides are
checkout-relative; use the source checkout for those cross-references.

Within a CMake parent project, use `add_subdirectory` and link to
`MetaArch::metaarch`, `MetaArch::api` or `MetaArch::dsl`. A complete installed
`find_package(MetaArch)` config/pkg-config package is not implemented yet.
Manual static linking must include codec libraries and `m` on Unix. For shared
linking, configure the platform loader path. See [native APIs](native-apis.md).
