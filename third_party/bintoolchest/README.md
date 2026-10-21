# Tourchstone

Tourchstone contains the dynamic loading, dynamic invocation, and binding-spec
tools used by the rest of libstone.

| Subproject | Option | Purpose |
| --- | --- | --- |
| `xbind/` | `TOURCHSTONE_BUILD_XBIND` | Strict spec runner, JSON IR, compiler signature probes, Lua/DomLuaBind generator and nine-target inventory. |
| `dynload/` | `TOURCHSTONE_BUILD_DYNLOAD` | Native dynamic-loader wrapper with Yellowstone qBFD-backed object inspection. |
| `dyninvoke/` | `TOURCHSTONE_BUILD_DYNINVOKE` | libffi/dyncall-style ABI layout, call, and closure support over qBFD and Sandstone ISA data. |

XBind defaults to `ON`. The native loader/invocation options default to `ON`
only when the active Yellowstone qBFD component is available. With Yellowstone
decommissioned, they default to `OFF`; requesting them explicitly without qBFD
produces a configuration error rather than reviving the retired source tree.

## Build and test

```sh
cmake -S tourchstone -B /tmp/opencode/tourchstone-build \
  -DTOURCHSTONE_BUILD_XBIND=ON \
  -DTOURCHSTONE_BUILD_DYNLOAD=ON \
  -DTOURCHSTONE_BUILD_DYNINVOKE=ON
cmake --build /tmp/opencode/tourchstone-build --parallel
ctest --test-dir /tmp/opencode/tourchstone-build --output-on-failure
```

For XBind-only work:

```sh
cmake -S tourchstone/xbind -B /tmp/opencode/xbind-build -DXBIND_BUILD_TESTS=ON
cmake --build /tmp/opencode/xbind-build --parallel
ctest --test-dir /tmp/opencode/xbind-build --output-on-failure
```

## XBind specs

Tourchstone owns the aggregate `Tourchstone.xbind` plus component specs for
`xbind`, `dynload`, and `dyninvoke`:

| Spec | Surface |
| --- | --- |
| `Tourchstone.xbind` | Aggregate version/status/query functions from `xbind`, `dyninvoke`, and `dynload`. |
| `xbind/Dogfooding.xbind` | Self-binding for the stable `xbind.h` query surface. |
| `dynload/DynLOAD.xbind` | Pointer-return loader/environment lifecycle plus object-inspection queries. |
| `dyninvoke/DynINVOKE.xbind` | Execution capability, named builtin types, ABI/layout queries and cif destruction. |

Each spec lists D, Go, Java, Lua, NodeJS, OCaml, Python, Ruby, and Rust. Only the
Lua scalar/string/explicit-opaque emitter is implemented. Callbacks, records and
automatic buffer/ownership marshalling remain unsupported. The test suite checks
all repository C spec signatures against their headers and compiles/executes Lua
hello and self-binding modules. See [REGIMEN.md](REGIMEN.md) for hardening coverage.

See `xbind/MANUAL.md` for the spec authoring guide and current repository-wide
binding coverage.
