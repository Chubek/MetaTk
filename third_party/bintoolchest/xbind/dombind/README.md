# DomBind language bindings

Native bindings following `dompybind`'s Q-generated, header-only design.
Each library has independent modules, a namespaced aggregate, C++ ownership
helpers, a header-generation entry point, examples and a CMake package.

| Library | Native boundary | CMake target |
|---|---|---|
| [domdbind](domdbind/README.md) | D `extern(C)`; buffers, callbacks, exception containment | `DomDBind::DomDBind` |
| [domgobind](domgobind/README.md) | cgo; buffers, callbacks, panic containment | `DomGoBind::DomGoBind` |
| [domjavabind](domjavabind/README.md) | JNI embedding and native calls | `DomJavaBind::DomJavaBind` |
| [domluabind](domluabind/README.md) | Lua embedding and extension callbacks | `DomLuaBind::DomLuaBind` |
| [domnodebind](domnodebind/README.md) | Node-API extensions | `DomNodeBind::DomNodeBind` |
| [domocamlbind](domocamlbind/README.md) | OCaml C interface and runtime callbacks | `DomOCamlBind::DomOCamlBind` |
| [dompybind](dompybind/README.md) | CPython embedding and extension modules (existing library) | `DomPyBind::DomPyBind` |
| [domrubybind](domrubybind/README.md) | CRuby embedding and extensions | `DomRubyBind::DomRubyBind` |
| [domrustbind](domrustbind/README.md) | Rust `extern "C"`; buffers, callbacks, panic containment | `DomRustBind::DomRustBind` |

The new libraries provide native API primitives and explicit ownership. Their
surfaces follow each language's runtime rather than copying Python's object
model. They do not reproduce DomPyBind's automatic C++ class/overload binder.
Go, Rust and D additionally include dependency-free native adapters and actual
shared-library export examples; they have no CPython-style interpreter API.

## Generate or build

Use the repository environment with the `q` package from `domlibs/domqlib`
importable. Generation requires Python and Tcl/Tk, without third-party binding
frameworks or generated-header downloads.

```sh
python generate.py lua /tmp/include/domluabind/capi.h
python generate.py lua /tmp/custom.h --module luaconv --prefix custom
```

Modules use `@param PREFIX <module>`, parameter-specific include guards and
`static inline` functions. Aggregate imports specify prefixes explicitly, so
operations such as `new`, `get` and `set` cannot collide. Lua headers supply
C linkage when compiled as C++; JNI wrappers support both C and C++ call syntax.

Build a single library from its directory, or opt in to selected runtimes:

```sh
cmake -S . -B /tmp/dombind-build \
  -DDOMBIND_LUA=ON -DDOMBIND_NODE=ON -DDOMBIND_RUST=ON
cmake --build /tmp/dombind-build
cmake --install /tmp/dombind-build --prefix /tmp/dombind-install
```

All options default to OFF so absent runtime SDKs do not break unrelated
consumers. The existing Python library retains its separate build. Generated
headers are installed as `include/dom<lang>bind/capi.h`. Installed packages
rediscover runtime dependencies and support `find_package(DomLuaBind CONFIG
REQUIRED)`, etc. CMake packages build the C/C++ surface; use the native language
toolchains to build Go, Rust and D exports. OCaml consumers link with the OCaml
toolchain; Node modules load inside Node rather than initializing a V8 runtime.

## Validation

```sh
python -m unittest discover -s tests -v
```

Tests compile every Q module under default and custom prefixes in both C99
and C++17, exercise binary strings, ownership, moving GC, callbacks and error
recovery, and load real Go/Rust/D shared libraries from a C ABI consumer.
See [tests/README.md](tests/README.md) for toolchain requirements and skips.
