# Development notes

## Configure and test
```sh
cmake -S . -B build -DDOXTK_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

CMake 3.20+ and a C++20 compiler are required. Ninja is used by the presets; use `cmake -S . -B build` if Ninja is unavailable.

The test build compiles every public header independently. Runtime checks remain active in Release builds. Tests cover core results, encoding boundaries, parsing, serialization, registry behavior, I/O (including delayed write failures on Linux), CLI success/error paths, and explicit unsupported backends. Test files and generated header translation units live in the build directory.

For a sanitizer build with Clang:
```sh
cmake -S . -B build-sanitize -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```

To check the installed package with a separate consumer:
```sh
cmake --install build --prefix "$PWD/build/install"
cmake -S tests/package -B build/consumer -DCMAKE_PREFIX_PATH="$PWD/build/install"
cmake --build build/consumer
build/consumer/consumer
```

Documentation is optional: configure with `-DDOXTK_BUILD_DOCS=ON` (requires Doxygen), then build the `doxtk_docs` target. `docs/Frontpage.md` indexes the manual and is configured as Doxygen's main page. Generated HTML remains under the build tree.

## Style
- Use four spaces, braces on the same line, and descriptive names.
- Keep headers self-contained and avoid non-standard includes in the core.
- Return structured errors for routine failures.
- Test CLI help and error paths as well as library behavior.
- Add regression fixtures for every format behavior claimed as supported.

## Optional dependencies
Vendored trees may exist, but the default build must also work without them. Optional integration switches currently fail explicitly rather than pretending an adapter is available. When implementing one, document the expected package target/version and test both enabled and disabled configurations.
