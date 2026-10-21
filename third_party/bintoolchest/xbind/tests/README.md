# XBind tests

The suite verifies both spec contracts and executable generated bindings.

| Test | Purpose |
| --- | --- |
| `regimen.py` | Verifies the project layout, target matrix, typedef files, JSON schema, hello fixture, CLI probes, and dogfooding spec dump. |
| `test_core.c` | C ABI target queries, status handling, spec resolution and short-buffer/invalid-input cases. |
| `test_codegen.py` | Checks every repository spec's deterministic JSON IR and exact C signatures, malformed inputs in both runtimes, failure atomicity, and compiled/executed Lua hello/self-bindings. |
| `qamrpp_module.cpp` | Loads `libqamrpp_xbind.so`, checks annotation/version preservation and rejects duplicate functions through the shared contract. |

The Tourchstone aggregate also runs `tourchstone.bindings`: it compiles and
executes DynLOAD, DynINVOKE and aggregate Lua modules against the built native
libraries, for both static/shared and sanitizer configurations.

Generated module execution needs Lua 5.4, its development SDK, `pkg-config`, and
the C/C++ compilers configured by CMake. Missing SDKs are explicit unittest skips;
compile/link/runtime errors are failures. Q uses Python's Tcl support. Core spec
and signature checks do not require a target SDK. Linux compiler flags are used
for generated-module tests; native platform coverage remains separate.

Run them through CTest:

```sh
cmake -S tourchstone/xbind -B /tmp/opencode/xbind-build -DXBIND_BUILD_TESTS=ON
cmake --build /tmp/opencode/xbind-build --parallel
ctest --test-dir /tmp/opencode/xbind-build --output-on-failure
```

`regimen.py` can also run directly when a built `xbind` CLI is available:

```sh
python3 tourchstone/xbind/tests/regimen.py \
  --root tourchstone/xbind \
  --cli /tmp/opencode/xbind-build/xbind
```
