# Native and foreign APIs

## Choose an interface

| API | Best use |
|---|---|
| `DSymspec.h`, `DsymspecIR.h` | Full C11 model/tree/program inspection or construction |
| `ArchDB.h` | C catalogue scan, lookup and lazy caching |
| `MetaArchABI.h` | Stable opaque ABI v1; foreign-language integration |
| `MetaArchAPI.hpp` (`metaarch::api`) | Standalone C++17 RAII over ABI v1 |
| `MetaArch.hpp` (`metaarch`) | C++20 DSL and assembler; `Result<T>` errors |
| `bindings/d/metaarch.di` | Direct D foreign declarations |
| `bindings/go` | Go/cgo wrapper with copied data and locking |
| `bindings/python` | Nanobind C++17 extension with Python exceptions |

The two C++ APIs deliberately have different namespaces and error models. Neither
changes the schema. Opaque ABI v1 covers spec loading/parsing/backend loading,
target/register/form summaries, encoder lookup, writers, named/form encoding
and relocations; it does not expose catalogue or DSL APIs.

## C model example

```c
#include <DSymspec.h>
#include <stdio.h>

int main(void) {
    DsymDiags d;
    dsym_diags_init(&d);
    DsymSpec *s = dsym_spec_load_file("ArchDB/riscv64.dsymspec.yaml",
                                    DSYM_FORMAT_AUTO, 0, &d);
    const char *names[] = {"rd", "rs1", "imm"};
    int64_t values[] = {10, 10, -1};
    DsymEncoded result;
    int ok = s && dsym_encode_named(s, "addi", 0, names, values, 3, false, &result);
    if (ok) {
        for (size_t i = 0; i < result.size; ++i) printf("%02x ", result.bytes[i]);
        putchar('\n');
    } else if (s) {
        fprintf(stderr, "%s\n", result.error);
    }
    dsym_diags_print(&d, stderr);
    dsym_spec_free(s);
    dsym_diags_free(&d);
    return ok ? 0 : 1;
}
```

`dsym_spec_load_buffer` parses length-delimited text with an explicit format.
`dsym_spec_from_tree` binds an existing tree. Programmatic construction uses
`dsym_spec_new`, arena-owned fields and `dsym_spec_resolve`. Read paths return
borrowed pointers; no pointer survives spec destruction. Native lookup returns
-1 when absent. `dsym_spec_write` writes into an initialized `DsymBuf`, which
must be freed separately. On failed encoding discard partial output and inspect
`out.error`. Never edit/free/resolve/load a backend during concurrent reads.

## Opaque C ABI

Call `metaarch_abi_version()` and compare with `METAARCH_ABI_VERSION`. Allocate
`MaDiagnostics`, `MaSpec`, `MaResult` only through the ABI and free each through
its matching function. Failure returns NULL or status codes; diagnostics are
optional and accumulate until cleared. Getter statuses are `MA_OK`,
`MA_INVALID_ARGUMENT`, `MA_NOT_FOUND`. All input buffers/arrays are borrowed for
the call, never retained. Strings and `Ma*View` members are borrowed until owner
free/diagnostics clear. Results own copied relocation names and outlive specs.
Byte lengths are explicit, not NUL-terminated text lengths. Use native `sizeof`
for view layouts; do not pack structs.

Native model struct ABI can change independently from opaque ABI v1. In
particular the relocation bit metadata enlarged `DsymEncodedReloc` and
`DsymEncoded`: rebuild native-layout consumers together. The opaque view layouts
are frozen within ABI v1.

## C++17

```cpp
#include <MetaArchAPI.hpp>
auto spec = metaarch::api::Spec::load("ArchDB/riscv64.dsymspec.yaml");
auto out = spec.encode("addi", {{"rd", 10}, {"rs1", 10}, {"imm", -1}});
```

Specs are move-only RAII handles. Errors throw `metaarch::api::Error`, whose
`diagnostics()` owns copied records. Metadata/results own their data. Invalid
indices throw `std::out_of_range`, embedded NULs `std::invalid_argument`, closed
or moved-from use `std::logic_error`. Standard allocation exceptions may occur.

## Catalogue

`archdb_open` accepts a catalogue or directory; `archdb_scan` reads specs to
populate summaries. Iterate with `archdb_count`/`archdb_entry`; find with
`archdb_find`. `archdb_load`/`archdb_load_name` cache lazily until `archdb_close`.
Load flags apply only on the first cached load. Entries/specs are borrowed from
the catalogue. Cache mutation is not thread-safe; use external locking.

## D and Go

See [the detailed bindings guide](../docs/BINDINGS.md) for compilable examples.
D module `metaarch` is a direct interface: explicit frees and borrowed C-string
lifetimes apply. `nothrow @nogc` does not mean the C library performs no allocation.

Go module `metaarch` requires Go 1.20+, cgo and the shared C library. Use a local
`replace` directive/workspace for the checkout, plus `CGO_CFLAGS`/`CGO_LDFLAGS`
for installed headers/libraries. `Load`, `Parse`, metadata access, `Encode`,
`EncodeForm`, `Write`, and explicit `Close` copy values into Go storage. An
RWMutex excludes Close/LoadBackend from reads. Do not copy a `Spec` value.
Use `errors.As` with `*metaarch.Error` for structured diagnostics. No finalizer
substitutes for explicit Close.
