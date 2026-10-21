# XBind manual

XBind is Tourchstone's binding-spec runner and Lua binding generator. A spec is a
Lua file (`.xbind` or `.lua`) that declares which C headers, symbols, target
languages, and type overrides a library wants. The current implementation can
resolve, execute, recursively validate, and summarize specs through QaMRpp. It
also emits JSON IR, exact C-signature probes, and Lua 5.4 bindings through
DomLuaBind. Header parsing and the other eight language emitters remain unimplemented.

## Quick start

Build the standalone driver and run the regimen:

```sh
cmake -S tourchstone/xbind -B /tmp/opencode/xbind-build -DXBIND_BUILD_TESTS=ON
cmake --build /tmp/opencode/xbind-build --parallel
ctest --test-dir /tmp/opencode/xbind-build --output-on-failure
```

Useful probes:

```sh
/tmp/opencode/xbind-build/xbind --list-targets
/tmp/opencode/xbind-build/xbind --check-layout tourchstone/xbind
/tmp/opencode/xbind-build/xbind --find-spec hello tourchstone/xbind/examples/hello
/tmp/opencode/xbind-build/xbind --check-spec hello tourchstone/xbind/examples/hello
/tmp/opencode/xbind-build/xbind --dump-spec hello tourchstone/xbind/examples/hello
/tmp/opencode/xbind-build/xbind --dump-spec Dogfooding tourchstone/xbind
```

## Target backends

Every committed `.xbind` spec lists the full target inventory. Only Lua has an
implemented emitter; the presence of a target is not a code-generation guarantee.

| Target | Target option examples | DomBind runtime | Default typedef map |
| --- | --- | --- | --- |
| D | `{ module = "name" }` | `dombind/domdbind` | `typedefs/D-Default.sexpr` |
| Go | `{ package = "name" }` | `dombind/domgobind` | `typedefs/Go-Default.sexpr` |
| Java | `{ package = "stone.name", class = "Name" }` | `dombind/domjavabind` | `typedefs/Java-Default.sexpr` |
| Lua | `{ module = "name" }` | `dombind/domluabind` | `typedefs/Lua-Default.sexpr` |
| NodeJS | `{ addon = "name" }` | `dombind/domnodebind` | `typedefs/NodeJs-Default.sexpr` |
| OCaml | `{ library = "name" }` | `dombind/domocamlbind` | `typedefs/OCaml-Default.sexpr` |
| Python | `{ module = "name" }` | `dombind/dompybind` | `typedefs/Python-Default.sexpr` |
| Ruby | `{ extension = "name" }` | `dombind/domrubybind` | `typedefs/Ruby-Default.sexpr` |
| Rust | `{ crate = "name_sys" }` | `dombind/domrustbind` | `typedefs/Rust-Default.sexpr` |

`--dump-ir` preserves target options and target-local typedefs. Lua emission
honors its `module` option (a C identifier); the default is the library name.

## Spec shape

A spec returns `xbind.library { ... }`. The constructors live in
`lxbind/lxbind.lua` and in the QaMRpp native module built as
`libqamrpp_xbind.so`.

```lua
local xbind = require("lxbind")

return xbind.library {
  name = "hello",
  version = "0.1.0",
  headers = { "hello.h" },
  stdlib = { "typedef", "sexp", "json" },
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
  typedefs = {
    xbind.typedef("const char *", "string", { ownership = "borrowed" }),
  },
  functions = {
    xbind.func("hello_add", {
      returns = "int",
      params = {
        xbind.param("left", "int"),
        xbind.param("right", "int"),
      },
    }),
  },
}
```

### Library fields

| Field | Required | Meaning |
| --- | --- | --- |
| `name` | yes | Logical library name and default spec basename. |
| `version` | no | Spec/API version. Defaults to `XBIND_VERSION`. |
| `headers` | no | Header paths relative to the spec directory. |
| `stdlib` | no | Planned XBind helper modules requested by the spec. |
| `targets` | no | `xbind.target` entries. Current repository specs list all nine targets. |
| `typedefs` | no | Spec-local type-map overrides applied after defaults. |
| `functions` | no | `xbind.func` declarations to bind. |
| `modules` | no | Future grouping surface for larger libraries. |
| `features` | no | Descriptive tags retained in IR; not executable transformation plugins. |

### Constructor contract

- `xbind.target(name, options)` requires a non-empty target name and defaults
  `features` and `typedefs` to arrays.
- `xbind.typedef(c_type, native_type, options)` requires non-empty C and native
  type names. Use options such as `ownership`, `range`, `encoding`, and
  `nullable` to document lowering requirements.
- `xbind.param(name, c_type, options)` requires non-empty names and C types.
- `xbind.func(name, options)` defaults `returns` to `void` and `params` /
  `features` to arrays.
- `xbind.module(name, options)` groups future function/typedef sets.
- `xbind.validate_library(manifest)` checks dense arrays, constructor kinds,
  supported canonical target names, duplicate targets/functions/params/typedefs,
  nonempty fields, identifiers and boolean nullability. Nonempty `modules` are
  rejected rather than silently discarded.

The driver and loadable QaMRpp module share their constructor and validation
implementation. Stock Lua validates the same structural contract. No spec is a
sandbox: execute only trusted Lua. Header checking is a separate compile step;
`--check-spec` alone does not certify the declared C ABI.

## Current repository specs

| Spec | Library | Headers | Bound slice |
| --- | --- | --- | --- |
| `yellowstone/Yellowstone.xbind` | `yellowstone` | `binscript/binscript.h`, `ld/ld.h` | Stable C APIs for binscript and qobjld. qBFD remains header-only C++ for now. |
| `sandstone/Sandstone.xbind` | `sandstone` | `include/sandstone.h` | DomSymAs version/probe/lifecycle/count/builder-save subset. |
| `limestone/Limestone.xbind` | `limestone` | `limestone/*.h` | Compile/module/target/program/buffer query subset. Callback-heavy APIs deferred. |
| `blackstone/Blackstone.xbind` | `blackstone` | `blackstone.h` | Runtime load/query subset. `blackstone_run` is deferred until callbacks land. |
| `firestone/Firestone.xbind` | `firestone` | `Firestone.hpp`, `FireIR.hpp` | Header inventory and all-target matrix only; Firestone has no stable C ABI yet. |
| `jiterati/Jiterati.xbind` | `jiterati` | `include/Jiterati*.hpp` | Optional checkout's C++ JIT/DocIR/DSL/SIMD/native/disassembly inventory; no invented C ABI. |
| `tourchstone/Tourchstone.xbind` | `tourchstone` | `xbind/xbind.h`, `dyninvoke/dyninvoke.h`, `dynload/dynload.h` | Aggregate version/status/query surface. |
| `tourchstone/dyninvoke/DynINVOKE.xbind` | `dyninvoke` | `dyninvoke.h` | ABI execution capability, named builtin types, layout queries and cif destruction. Calls and closures deferred. |
| `tourchstone/dynload/DynLOAD.xbind` | `dynload` | `dynload.h` | Environment creation/destruction/search paths, pointer-return library load/close and inspection queries. Records deferred. |
| `tourchstone/xbind/Dogfooding.xbind` | `dogfooding` | `xbind.h` | XBind's own callback-free query surface. |
| `tourchstone/xbind/examples/hello/hello.xbind` | `hello` | `hello.h` | Minimal fixture with integer, double, and borrowed-string functions. |

## Type maps

Lua emission parses `typedefs/Lua-Default.sexpr`, applies library-local
`xbind.typedef` entries, then Lua target-local overrides. Other targets' map files
are inventories only. Defaults cover:

- `void`
- `bool`, `_Bool`
- `int`, `unsigned int`, `size_t`
- `float`, `double`
- `const char *`, `char *`
- `const unsigned char *`
- `void *`

Any ABI-visible type outside that set must be listed in `typedefs`. Opaque handle
patterns used by the repository specs include:

```lua
xbind.typedef("library_handle *", "opaque", { ownership = "opaque" })
xbind.typedef("const library_handle *", "opaque", { ownership = "opaque" })
xbind.typedef("status_t", "int", { range = "status" })
xbind.typedef("uint64_t", "int", { range = "uint64" })
```

Lua supports `integer` (including the spec alias `int`), `number`, `boolean`,
borrowed `const char *` strings, and `lightuserdata` (including `opaque`). Integer
conversions check the actual compiled C type, including enum underlying types.
Unsigned results above `LUA_MAXINTEGER` raise an error rather than losing bits.
Inputs require exact arity and types; embedded NULs in C strings are rejected.
`nullable=true` permits Lua nil only for pointer inputs. Null pointer results
become nil. Mutable strings, owned-string returns and out-string annotations
are rejected. Buffers may be passed only as explicit, caller-managed opaque
pointers; no length validation or allocation is inferred.

`ownership="owned"` and `ownership="consumed"` describe opaque-handle lifetimes
in IR; they do **not** install a finalizer or prevent double destruction. Call
the matching destructor once and invalidate all aliases. Borrowed strings are
copied into Lua strings; opaque pointers retain no owner.

## IR, signature checks and Lua generation

```sh
xb=/tmp/opencode/xbind-build/xbind
"$xb" --dump-ir hello tourchstone/xbind/examples/hello > /tmp/opencode/hello.json
"$xb" --emit-check hello tourchstone/xbind/examples/hello > /tmp/opencode/hello-check.cpp
c++ -std=c++20 -fsyntax-only -Itourchstone/xbind/examples/hello /tmp/opencode/hello-check.cpp
"$xb" --emit-lua hello tourchstone/xbind/examples/hello > /tmp/opencode/hello-bind.cpp

# Generate the actual DomLuaBind C API, using its bundled Q compiler.
PYTHONPATH=tourchstone/xbind/dombind/domqlib \
  python3 tourchstone/xbind/dombind/domluabind/generate.py /tmp/opencode/domluabind/capi.h
cc -fPIC -c tourchstone/xbind/examples/hello/hello.c -o /tmp/opencode/hello.o
c++ -std=c++20 -shared -fPIC -I/tmp/opencode -Itourchstone/xbind/examples/hello \
  /tmp/opencode/hello-bind.cpp /tmp/opencode/hello.o \
  $(pkg-config --cflags --libs lua5.4) -o /tmp/opencode/hello.so
LUA_CPATH='/tmp/opencode/?.so' lua5.4 -e 'assert(require("hello").hello_add(2,5)==7)'
```

IR schema version 1 preserves declaration order, sorted annotation keys,
library/version/path, headers, stdlib and feature tags, target options and
typedefs, function signatures and parameter annotations. The path records spec
provenance; repeated invocations with the same path produce identical bytes.
The IR is a spec IR, not a header AST. `--emit-check` emits `std::is_same_v`
assertions against the included declarations, diagnosing missing symbols and
return/parameter mismatches without linking. Generated Lua source includes the
same assertions. Empty C surfaces (Firestone/Jiterati) cannot emit a Lua module.

Generation writes source to stdout, diagnostics to stderr, and never emits a
partial source on lowering failure. Exit codes: 0 success, 1 spec/lowering error,
2 invalid command usage. Lua generation defaults to the source-tree map;
set `XBIND_DATA_DIR=/prefix/share/xbind` for installed or relocated data. The
C++ `emit_lua` API accepts an explicit data root.

## Validation regimen

Use the full Tourchstone build when changing shared loader/invoker pieces:

```sh
cmake -S tourchstone -B /tmp/opencode/tourchstone-build \
  -DTOURCHSTONE_BUILD_XBIND=ON \
  -DTOURCHSTONE_BUILD_DYNLOAD=ON \
  -DTOURCHSTONE_BUILD_DYNINVOKE=ON
cmake --build /tmp/opencode/tourchstone-build --parallel
ctest --test-dir /tmp/opencode/tourchstone-build --output-on-failure
```

For spec-only work, run the signature and generated-module tests as well:

```sh
cmake -S tourchstone/xbind -B /tmp/opencode/xbind-build -DXBIND_BUILD_TESTS=ON
cmake --build /tmp/opencode/xbind-build --parallel
ctest --test-dir /tmp/opencode/xbind-build --output-on-failure

for spec in \
  yellowstone/Yellowstone.xbind \
  sandstone/Sandstone.xbind \
  limestone/Limestone.xbind \
  blackstone/Blackstone.xbind \
  firestone/Firestone.xbind \
  tourchstone/Tourchstone.xbind \
  tourchstone/dyninvoke/DynINVOKE.xbind \
  tourchstone/dynload/DynLOAD.xbind \
  tourchstone/xbind/Dogfooding.xbind \
  tourchstone/xbind/examples/hello/hello.xbind
do
  /tmp/opencode/xbind-build/xbind --check-spec "$spec" .
done
```

Optional stock Lua validation:

```sh
lua5.4 -e 'package.path="tourchstone/xbind/lxbind/?.lua;"..package.path; local x=require("lxbind"); local m=assert(loadfile("tourchstone/xbind/examples/hello/hello.xbind"))(); x.validate_library(m); print(m.name, #m.targets, #m.functions)'
```

## Current limitations

- Header parsing and source-span preservation are not implemented yet.
- `compilers/` directories remain reserved; the working Lua emitter is
  `xbind_codegen.cpp`. The other eight target emitters are not implemented.
- Enums explicitly mapped to integers work. Records, callbacks, automatic
  `T **` out-parameter/buffer marshalling and ownership-transfer wrappers do not.
- `Yellowstone.xbind` requires out-string marshalling: signature checks pass,
  but Lua emission fails explicitly instead of producing unsafe fake wrappers.
- `firestone/Firestone.xbind` is intentionally a header inventory because
  Firestone currently exposes C++ headers, not a stable extern-C surface.
- `--dump-spec` remains the concise summary; use `--dump-ir` for annotations,
  typedefs, target options and feature tags.

See `REGIMEN.md` for staged implementation goals.
