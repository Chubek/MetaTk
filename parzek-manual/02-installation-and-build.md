# 02 Installation and Build

## Requirements

- C++20 compiler
- CMake 3.16+
- Catch2 (optional, for tests)

## Build locally

```bash
cmake -S . -B build
cmake --build build -j
```

Artifacts:

- Library: `libparzek.a`
- CLI: `parzek`
- Tests: `test_parzek` (when Catch2 is available)

## Run tests

```bash
ctest --test-dir build --output-on-failure
```

## Install

```bash
cmake --install build --prefix /usr/local
```

Installs:

- `Parzek.hpp`
- `DSLUtils.hpp`
- `parzek` binary
- static library
- `dslutils.pc` pkg-config file

## pkg-config usage

After install, downstream code can use:

```bash
pkg-config --cflags --libs parzek-dslutils
```

## CI tips

Recommended CI steps:

1. Configure and build
2. Run unit tests
3. Run one CLI smoke compile on a sample `.pzg`
4. Optionally compile generated parser source as a smoke check
