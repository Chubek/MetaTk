# Architecture and repository map

## Data pipeline

```text
YAML / JSON / XML / S-expression
                |
      format-neutral DsymNode tree
                |
 descriptor-driven schema binding
                |
       typed, arena-owned DsymSpec
                |
 reference resolution + encoder compilation
                |
 metadata lookups / encoder VM / model-to-tree writers
```

The four readers produce the same ordered tree: null, boolean, signed/unsigned
integer, real, string, sequence or map. Source positions are attached where
available. Descriptors in `Dsymspec.c` define the binder/unbinder constraints;
the external JSON Schema is in `ArchDB/schema/dsymspec.schema.json`. Sharing
descriptors prevents binder/writer duplication, but the external schema is
still maintained separately.

Resolution creates lookup tables, verifies references and cycles, and compiles
encoder bodies to constant-folded RPN bytecode. The VM runs programs with
per-call integer operands and returns bytes plus relocation metadata. No machine
code is executed by MetaArch itself.

## Source map

| Location | Responsibility |
|---|---|
| `DsymspecIR.h` | Arena/buffer, diagnostics, document tree, codecs, encoder IR |
| `DSymspec.h` | Typed records, loading, resolution, lookup and encoding |
| `Dsymspec.c` | Shared infrastructure, schema descriptors, compiler and VM |
| `ToYAML.c`, `ToJSON.c`, `ToXML.c`, `ToSExpr.c` | Four tree codecs |
| `ArchDB.h`, `ArchDB.c` | Catalogue scanning, summaries and lazy spec cache |
| `ArchBFD.h`, `ArchBFD.c` | Object-format binder, catalogue and description-driven object reader |
| `MetaArchABI.h`, `MetaArchABI.c` | Versioned opaque ABI for foreign languages |
| `MetaArchAPI.hpp` | C++17 RAII wrapper, namespace `metaarch::api` |
| `MetaArch.hpp`, `MetaArch.cpp` | Optional C++20 DSL/assembler, namespace `metaarch` |
| `tools/archdb.c` | Core command-line frontend |
| `tools/archbfd.c` | Object-format command-line frontend |
| `tools/extract_bfd.py` | Generate/check the `ArchBFD` documents from binutils |
| `tools/metaarch.cpp` | DSL conversion/assembly frontend |
| `tools/compile_archdb.py` | Generate/check `.march` database |
| `bindings/{d,go,python}` | Foreign interfaces; Python uses nanobind |
| `ArchDB/` | 28 hand-authored specs, schemas and generated backend records |
| `ArchDBCatalogue.yaml` | Committed catalogue and summary metadata |
| `ArchBFD/` | Object-format descriptions, generated from binutils/bfd |
| `ArchBFDCatalogue.yaml` | Committed index of the object-format descriptions |
| `MetaArchDB/` | Generated DSL specs with backend symlinks |
| `tests/`, `examples/`, `docs/` | Regressions, executable examples and focused guides |

`MetaArch.xbind` is currently empty. `daemon/archserve.py` is an unfinished
scratch example with undefined placeholders and references to an older binding
design; it is **not a working server** or part of the supported build. The
nanobind API does not provide its `Native()` interface.

## Ownership and concurrency

The native model owns arena memory; its record pointers expire when the spec is
freed. Diagnostics and buffers have separate initialization/free APIs. After
resolution, read-only operations do not mutate the spec; each encoding needs
separate output storage. Loading a backend, resolving, editing or freeing
requires exclusive access. Catalogue lazy cache operations require external
locking. Opaque bindings copy values rather than exposing native layouts.

Python retains the GIL for all native calls. Go protects handles with an
RWMutex. Native C/C++/D users must enforce ownership/concurrency rules themselves.
