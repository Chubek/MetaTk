# XBind

XBind is the Tourchstone binding-spec runner and Lua binding generator. It aims to serve the
same role as SWIG for this tree: read a C API, apply a Lua-written binding spec,
normalize the requested surface, map C types into a target language, then lower
the result through the DomBind runtime libraries.

The implemented slice includes recursive manifest validation, deterministic JSON
IR, compiler-checked C signatures, Lua type-map resolution, and executable
Lua 5.4 glue through DomLuaBind. The other eight language targets remain
inventories, not working emitters. There is no general C header parser. See
[`MANUAL.md`](MANUAL.md) for the spec authoring guide and current binding
coverage.

## Directory map

| Path | Purpose |
| --- | --- |
| `xbind.c`, `xbind.h`, `xbind.hpp` | Small C/C++ API plus a scaffold CLI for discovery and layout checks. |
| `xbind_qamrpp.*` | QaMRpp-backed executor for `.xbind` Lua specs. |
| `xbind_codegen.*` | Deterministic IR, exact-signature probes, Lua/DomLuaBind emitter. |
| `MANUAL.md` | Spec authoring guide, target matrix, current repository specs, and limitations. |
| `cmake/XBindQaMRpp.cmake` | CMake locator for the configured `third_party/qamrpp` checkout (`XBIND_QAMRPP_ROOT`). |
| `lxbind/` | Lua helpers used by `.xbind`/`.lua` specs. |
| `typedefs/` | Default per-target C type maps, loaded before user overrides. |
| `stdlib/` | Planned reusable spec/runtime helpers. |
| `compilers/<Target>.c/` | Per-target compiler/lowering stages from XBind IR to DomBind. |
| `dombind/` | Native target runtime/boundary libraries used by generated bindings. |
| `examples/` | Executable examples and spec fixtures. |
| `Dogfooding.xbind` | XBind's self-binding spec for the stable `xbind.h` query surface. |
| `XFeats.schema.json` | Feature-manifest schema for binding transformation features. |

## Target inventory (Lua emission implemented)

| Target | DomBind library | Default type map |
| --- | --- | --- |
| D | `dombind/domdbind` | `typedefs/D-Default.sexpr` |
| Go | `dombind/domgobind` | `typedefs/Go-Default.sexpr` |
| Java | `dombind/domjavabind` | `typedefs/Java-Default.sexpr` |
| Lua | `dombind/domluabind` | `typedefs/Lua-Default.sexpr` |
| NodeJS | `dombind/domnodebind` | `typedefs/NodeJs-Default.sexpr` |
| OCaml | `dombind/domocamlbind` | `typedefs/OCaml-Default.sexpr` |
| Python | `dombind/dompybind` | `typedefs/Python-Default.sexpr` |
| Ruby | `dombind/domrubybind` | `typedefs/Ruby-Default.sexpr` |
| Rust | `dombind/domrustbind` | `typedefs/Rust-Default.sexpr` |

## Spec files

Specs are Lua files with either `.xbind` or `.lua` extensions. By convention a
library named `hello` is described by `hello.xbind`, falling back to `hello.lua`.
The spec returns a manifest table built with `lxbind` helpers:

```lua
local xbind = require("lxbind")

return xbind.library {
  name = "hello",
  headers = { "hello.h" },
  targets = {
    xbind.target("D", { module = "hello" }),
    xbind.target("Go", { package = "hello" }),
    xbind.target("Java", { package = "stone.hello", class = "Hello" }),
    xbind.target("Lua", { module = "hello" }),
    xbind.target("NodeJS", { addon = "hello" }),
    xbind.target("OCaml", { library = "hello" }),
    xbind.target("Python", { module = "hello" }),
    xbind.target("Ruby", { extension = "hello" }),
    xbind.target("Rust", { crate = "hello_sys" }),
  },
  functions = {
    xbind.func("hello_add", {
      returns = "int",
      params = { xbind.param("left", "int"), xbind.param("right", "int") },
    }),
  },
}
```

Lua emission parses `typedefs/Lua-Default.sexpr`, then applies library overrides
and target-local overrides, in that order. Requested `stdlib` entries are retained
as metadata; their placeholder implementations are not executed.

Repository specs list all nine targets. The implemented Lua surface is deliberately
callback-free and record-free; explicit opaque pointers are unmanaged handles.
See `MANUAL.md` for the per-spec coverage table.

## Build and smoke checks

Standalone build:

```sh
cmake -S tourchstone/xbind -B /tmp/opencode/xbind-build -DXBIND_BUILD_TESTS=ON
cmake --build /tmp/opencode/xbind-build
ctest --test-dir /tmp/opencode/xbind-build --output-on-failure
```

Useful CLI probes:

```sh
/tmp/opencode/xbind-build/xbind --list-targets
/tmp/opencode/xbind-build/xbind --check-layout tourchstone/xbind
/tmp/opencode/xbind-build/xbind --find-spec hello tourchstone/xbind/examples/hello
/tmp/opencode/xbind-build/xbind --dump-spec hello tourchstone/xbind/examples/hello
/tmp/opencode/xbind-build/xbind --dump-ir hello tourchstone/xbind/examples/hello
/tmp/opencode/xbind-build/xbind --emit-check hello tourchstone/xbind/examples/hello
/tmp/opencode/xbind-build/xbind --emit-lua hello tourchstone/xbind/examples/hello
```

The `--check-spec` and `--dump-spec` commands execute the spec with the
configured QaMRpp Lua-compatible interpreter (`XBIND_QAMRPP_ROOT`, normally
`third_party/qamrpp` or `${LIBSTONE_THIRD_PARTY_DIR}/qamrpp`). The build also
emits `libqamrpp_xbind.so`, a QaMRpp native module that exposes the same
XBind/LxBind constructor surface to QaMRpp hosts.

DomBind remains opt-in because each runtime may require a different SDK:

```sh
cmake -S tourchstone/xbind -B /tmp/opencode/xbind-build \
  -DXBIND_BUILD_DOMBIND=ON -DDOMBIND_LUA=ON
```

## Implementation pipeline

1. Resolve the spec (`<library>.xbind`, then `<library>.lua`) and execute it with
   `lxbind` on the Lua path through QaMRpp.
2. Validate dense arrays, constructor kinds, names, duplicates, annotations and types.
3. Preserve typedefs, target options, function/parameter annotations in JSON IR.
4. Emit exact C++ function-pointer assertions; the compiler verifies the headers.
5. Resolve Lua defaults, library overrides and target-local overrides.
6. Emit scalar/string/opaque-pointer glue using DomLuaBind primitives.
7. Compile and execute hello and self-binding modules in the test suite.

Header AST ingestion, source spans, automatic ownership, records, callbacks,
out-buffer marshalling and the other language emitters remain future work.

See [`REGIMEN.md`](REGIMEN.md) for the staged validation plan.
