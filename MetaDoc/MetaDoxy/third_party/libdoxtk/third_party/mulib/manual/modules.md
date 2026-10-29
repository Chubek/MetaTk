# MuLIB module and composition guide

MuLIB has 114 single-header C11 modules. Every module has a corresponding
`manual/mulib-<name>.md` and `examples/modules/<name>.c`. The machine-readable
inventory is `MANIFEST.yaml`. Names are not promises of full external standards;
consult each manual's exact supported syntax, errors, and limits.

## New module groups

| Area | Modules |
| --- | --- |
| Memory and portability | [alloc](mulib-alloc.md), [mempool](mulib-mempool.md), [strpool](mulib-strpool.md), [memtkx](mulib-memtkx.md), [port](mulib-port.md) |
| Parsing | [parsec](mulib-parsec.md), [peg](mulib-peg.md), [regex](mulib-regex.md), [atn](mulib-atn.md), [glr](mulib-glr.md), [sppf](mulib-sppf.md), [gss](mulib-gss.md) |
| Structured data | [json](mulib-json.md), [yaml](mulib-yaml.md), [toml](mulib-toml.md), [template](mulib-template.md), [xml](mulib-xml.md), [xpath](mulib-xpath.md) |
| Documents and graphics | [html](mulib-html.md), [css](mulib-css.md), [markdown](mulib-markdown.md), [svg](mulib-svg.md), [graphviz](mulib-graphviz.md), [vector algebra](mulib-vector.md) |
| Storage and native formats | [archive](mulib-archive.md), [mmfile](mulib-mmfile.md), [kvdb](mulib-kvdb.md), [elf](mulib-elf.md), [dynload](mulib-dynload.md), [drm](mulib-drm.md) |
| Terminals | [terminfo](mulib-terminfo.md), [termkey](mulib-termkey.md), [vterm](mulib-vterm.md), [tui](mulib-tui.md) |
| Protocols | [uri](mulib-uri.md), [http](mulib-http.md), [rpc](mulib-rpc.md), [dbus](mulib-dbus.md) |
| Runtime utilities | [minivm](mulib-minivm.md), [minish](mulib-minish.md), [Python bindings generator](mulib-pybind.md), [Ruby bindings generator](mulib-rubybind.md) |
| Indexes and differences | [avl](mulib-avl.md), [b+tree](mulib-b+tree.md), [hattrie](mulib-hattrie.md), [txtdiff](mulib-txtdiff.md), [mmdiff](mulib-mmdiff.md) |

## Dependency ownership

MuLIB does not automatically turn on dependency implementation switches.
This avoids duplicate symbols when independent modules share a dependency.
An application may put all implementation switches in one C11 source file,
before including any header, and include declarations everywhere else.

For JSON and consumers of its value model, the foundation is:

```c
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_JSON
#include <mulib-json.h>
```

Add the consumer's switch and header in that same translation unit. Its manual
may list additional dependencies. A JSON value is owned by its creator until a
successful append/set transfers ownership. Borrowed children must not be freed
separately. Strings and object keys are length-delimited and may contain NUL.
Serialize to an owned `char *` and release it with `free`.

Memory allocators are per-instance value objects, never process-global hooks.
For example, passing a `mulib_memtkx_allocator` view to a memory pool or string
pool measures its real allocations. The tracker must outlive those objects.

## Validation

`python3 tools/check.py` validates inventory, runs examples and focused tests,
compiles headers in isolation with their transitive dependencies, and checks
all implementations together. Declaration-only C++17 inclusion is checked when
a C++ compiler is available. Tests of wire encoders and OS failure paths are
not a claim of interoperability with every service or hardware device.

The C implementation is the supported implementation target. OS modules use
native APIs, not external convenience libraries. Build and runtime platform
restrictions are documented per module, and hardware is not required for the
normal test suite.

The separate-translation-unit `tests/composition.c` also exercises one value
model across JSON parsing, YAML emission/parsing, TOML emission/parsing, and
template rendering, alongside tracked memory pools, interned strings, and
numerical vectors. This checks real cross-module reuse rather than merely
including unrelated headers in one file.
