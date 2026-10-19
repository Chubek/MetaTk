# Native and language APIs

MetaArch now provides a version-1 **opaque C ABI** plus a standalone C++17 API,
a D interface module, and a Go/cgo package. They all use the same C implementation
for loading, validation, metadata, encoding, relocation layout and serialization.
No D or Go compiler/runtime is needed to build or use the C library.

| Interface | Entry point | Dependency |
|---|---|---|
| Opaque C ABI | `MetaArchABI.h`, `metaarch_*` exported functions | C11 library |
| Standalone C++ | `MetaArchAPI.hpp`, `metaarch::api::Spec` | C++17, header-only wrapper over the C ABI |
| D | `bindings/d/metaarch.di`, module `metaarch` | D compiler; links to the C library |
| Go | `bindings/go`, module/package `metaarch` | Go 1.20+, cgo and C compiler; links to the C library |
| Existing native model | `DSymspec.h`, `ArchDB.h` | C11, exposes native model layout |
| Existing architecture DSL/assembler | `MetaArch.hpp`, `metaarch::Spec` | Optional C++20/DSLtk implementation |

The two C++ namespaces intentionally differ. The new API requires **neither
DSLtk nor `ARCHDB_BUILD_DSL`**, and does not replace or rename the existing DSL API.
Catalogue APIs and the architecture DSL are not wrapped by this ABI version;
the existing native catalogue/DSL APIs remain available. The new bindings cover
spec load/parse, explicit backend load, target/register/form summaries, encoder
lookup, all four writers, named encoding, checked form encoding and relocations.

## ABI and ownership contract

- `metaarch_abi_version()` returns `METAARCH_ABI_VERSION` (currently 1). C symbols
  have C linkage. ABI scalar types are fixed-width integers except for native
  `size_t` indices/lengths. Link binaries built for the same platform/word size.
- `MaSpec`, `MaDiagnostics` and `MaResult` are opaque owning handles. Use only
  their corresponding free functions, never libc `free`, C++ `delete` or the D
  GC. NULL free calls are harmless. Use-after-free and foreign/forged handles
  remain caller errors. `Ma*View` layouts are fixed for ABI version 1; future
  additions require new view types/functions, not extending those structs.
- C/D strings in metadata views are borrowed from their owning handle. Result
  bytes have explicit lengths (do not assume NUL termination). Diagnostic views
  also expire on `metaarch_diagnostics_clear`. Copy before freeing the owner.
- A result owns its bytes **and relocation names**, and survives freeing its
  spec. Parse/encode functions never retain caller arrays, strings or buffers.
- C++ and Go copy metadata, diagnostics, encoded bytes and relocation records
  into language-owned values. These remain usable after the spec is closed.
- Handles/results return NULL on failure. Supply a diagnostics handle to inspect
  errors; messages include core validation/encoding errors and parser positions
  where available. Diagnostics accumulate until cleared. Indexed getters return
  `MA_OK`, `MA_INVALID_ARGUMENT`, or `MA_NOT_FOUND` and zero a non-NULL output
  view on failure. Diagnostic allocation itself can fail; NULL is the fallback.
- Read-only operations may share a spec. Do not free/move/close a C/C++/D handle
  or load its backend during reads. Give concurrent calls separate diagnostics.
  Go methods use an RWMutex, so `Close` and `LoadBackend` exclude ongoing reads.
  Do not copy a Go `Spec` value. Explicit Close is required; there is no finalizer.
- Named encode is low-level: all parameters must be bound and names must be
  unique (at most 64 bindings). Form encode enforces the documented positional
  mapping, scalar bounds and register class/width. See [README](../README.md).
  New APIs validate targets rather than silently selecting a fallback target.
- Paths/names are NUL-terminated in C/D. C++/Go reject embedded NULs in those
  arguments. Parse buffers are length-delimited in every language.
- The original `Dsym*` layout-exposing API is unchanged by this addition. The
  preceding Phase B native struct ABI change does not propagate to `Ma*` handles.

## Build and install

```sh
cmake -S . -B build-api -DBUILD_SHARED_LIBS=ON -DARCHDB_BUILD_DSL=OFF \
  -DARCHDB_TEST_CPP_API=ON -DARCHDB_TEST_FOREIGN_BINDINGS=ON
cmake --build build-api -j
ctest --test-dir build-api --output-on-failure
cmake --install build-api --prefix /your/prefix
```

As before, system packages are preferred for dependencies. If offline, supply
an existing yyjson source checkout with `-DFETCHCONTENT_SOURCE_DIR_YYJSON=...`.
Fetched yyjson/libyaml are static even when the MetaArch library is shared.

`ARCHDB_TEST_CPP_API` defaults to the value of `ARCHDB_BUILD_DSL`; enabling it
tests an actual C++17 translation unit independently of the C++20 DSL. The
header and `MetaArch::api` INTERFACE target are always available to CMake
`add_subdirectory` consumers. Disable both C++ test/DSL options for a C-only build.

Foreign tests are opt-in, require a shared build and discover `ldc2`/`dmd` and
`go`. Missing language compilers produce explicit skip messages at configure
time. Test execution is offline (`GOPROXY=off`, `GOTOOLCHAIN=local`). A valid
Go installation and GOROOT, if set, are required. Do not enable the Go/D test
option against an ASan library unless those foreign runtimes are configured
for sanitizer integration; C/C++ ABI tests already run in sanitizer builds.

Headers are installed under `include/metaarch/`, D's interface under
`include/d/`, and Go module sources under `share/metaarch/bindings/go/`.
This does not claim to implement the outstanding full CMake package/CI work.

## C example

```c
#include <MetaArchABI.h>
#include <stdio.h>

int main(void) {
    MaDiagnostics *d = metaarch_diagnostics_create();
    if (!d) return 1;
    MaSpec *s = metaarch_spec_load("ArchDB/riscv64.dsymspec.yaml", MA_AUTO, 0, d);
    const char *names[] = {"rd", "rs1", "imm"};
    int64_t values[] = {10, 10, -1};
    MaResult *r = s ? metaarch_spec_encode(s, "addi", 0, names, values, 3, d) : NULL;
    if (r) {
        for (size_t i = 0; i < metaarch_result_size(r); ++i)
            printf("%02x ", metaarch_result_data(r)[i]); /* 13 05 f5 ff */
    } else {
        for (size_t i = 0; i < metaarch_diagnostics_count(d); ++i) {
            MaDiagnosticView v;
            if (metaarch_diagnostics_get(d, i, &v) == MA_OK) puts(v.message);
        }
    }
    int status = r ? 0 : 1;
    metaarch_result_free(r);
    metaarch_spec_free(s);
    metaarch_diagnostics_free(d);
    return status;
}
```

For a shared library, compile with `cc example.c -I/prefix/include/metaarch
-L/prefix/lib -lmetaarch` and configure the platform runtime library search path.
Static consumers must also link the codec dependencies and `m` on Unix; using
the CMake target supplies its existing transitive link requirements.

## C++17 example

```cpp
#include <MetaArchAPI.hpp>
#include <iostream>

int main() {
    try {
        auto s = metaarch::api::Spec::load("ArchDB/riscv64.dsymspec.yaml");
        auto out = s.encode("addi", {{"rd", 10}, {"rs1", 10}, {"imm", -1}});
        std::cout << out.bytes.size() << " bytes\n";
    } catch (const metaarch::api::Error &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
```

Specs are move-only RAII objects. Operations throw `metaarch::api::Error` with
copied structured diagnostics, `std::out_of_range` for invalid lookup indices,
`std::invalid_argument` for embedded NUL names and `std::logic_error` for use
after close/move. Standard allocation exceptions remain possible. Link the
same shared C library; in a parent CMake build use `MetaArch::api`.

## D example (`.di` module)

```d
import metaarch;
import std.string : toStringz;

int main() {
    auto d = metaarch_diagnostics_create();
    if (d is null) return 1;
    scope(exit) metaarch_diagnostics_free(d);
    auto s = metaarch_spec_load(toStringz("ArchDB/riscv64.dsymspec.yaml"), MA_AUTO, 0, d);
    if (s is null) return 1;
    scope(exit) metaarch_spec_free(s);
    const(char)*[3] names = ["rd".ptr, "rs1".ptr, "imm".ptr];
    long[3] values = [10, 10, -1];
    auto r = metaarch_spec_encode(s, "addi", 0, names.ptr, values.ptr, values.length, d);
    if (r is null) return 1;
    scope(exit) metaarch_result_free(r);
    assert(metaarch_result_data(r)[0 .. metaarch_result_size(r)] == [0x13, 0x05, 0xF5, 0xFF]);
    return 0;
}
```

Example Linux LDC invocation:

```sh
ldc2 -Ibindings/d example.d -L-Lbuild-api -L-lmetaarch -L-rpath="$PWD/build-api"
```

For installed headers use `-I/prefix/include/d`. D `long` is always 64-bit and
maps to C `int64_t`. `nothrow @nogc` describes the foreign calls' interaction
with D exceptions/GC, **not** an absence of C heap allocation. For borrowed
strings use `std.string.fromStringz`, then `.idup` when ownership is needed.

## Go example

The package is a local module named `metaarch`. In another module use a replace
directive (or a workspace) pointing to this checkout's `bindings/go` directory:

```text
require metaarch v0.0.0
replace metaarch => /path/to/MetaArch/bindings/go
```

```go
package main

import (
    "fmt"
    "log"
    "metaarch"
)

func main() {
    s, err := metaarch.Load("ArchDB/riscv64.dsymspec.yaml", metaarch.Auto, 0)
    if err != nil { log.Fatal(err) }
    defer s.Close()
    out, err := s.Encode("addi", []metaarch.Binding{{Name:"rd", Value:10},
        {Name:"rs1", Value:10}, {Name:"imm", Value:-1}}, 0)
    if err != nil { log.Fatal(err) }
    fmt.Printf("% x\n", out.Bytes) // 13 05 f5 ff
}
```

From `bindings/go`, build/test with the shared library available:

```sh
CGO_ENABLED=1 CGO_LDFLAGS="-L$PWD/../../build-api -Wl,-rpath,$PWD/../../build-api" go test ./...
```

For an installed library set `CGO_CFLAGS=-I/prefix/include/metaarch` and
`CGO_LDFLAGS='-L/prefix/lib -Wl,-rpath,/prefix/lib'` on Linux. Header lookup in
the checkout is automatic; cgo always adds `-lmetaarch`. No third-party Go
modules are downloaded. Use `errors.As(err, &metaarchError)` with a
`*metaarch.Error` variable to access structured diagnostic positions/messages.
