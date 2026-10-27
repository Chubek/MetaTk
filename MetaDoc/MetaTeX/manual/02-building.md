# 02 — Building and installing

The supported development workflow uses Linux/POSIX, a C++20 compiler, a C
compiler, and CMake 3.18 or newer. Configure and build from the source root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

The native build needs the enclosing workspace's dynalo, replxx, DSLtk,
MetaML/metalib, cpp-httplib, and MetaVect scene sources, plus SQLite3 and NNG
development libraries. CMake does not download them. Path overrides and
upstream/license information are in [the dependency guide](../docs/DEPENDENCIES.md).
An isolated source archive may therefore need additional workspace checkouts.

PangoCairo is optional. If discovered through `pkg-config`, it supplies the
shaped PDF backend. Set `-DMETATEX_ENABLE_PANGO=OFF` to select the basic writer.
Python 3 enables the integration and manual-example tests.

Install a successful build into a chosen prefix:

```sh
cmake --install build --prefix "$HOME/.local"
```

The executable and launchers go in `bin`, plugins and the core shared library
in `lib/metatex`, and the manual in `share/doc/metatex/manual`. Rebuild plugins
with the same headers and toolchain as the core library after API changes.

`METATEX_TEXSCRAPE_ONLY=ON` builds the reference compiler and helpers. Use the
regular configuration above when you need the native MetaTeX application.

[Contents](README.md)
