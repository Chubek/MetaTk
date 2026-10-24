# Architecture and integration notes

The central dependency is the **original ArchBFD** `ArchBFD.c` and `.h`, not a
replacement object reader. `archbfd_object_open_memory` returns sections,
symbols, and relocation metadata. The linker retains input bytes because the
public ArchBFD API exposes offsets, not payload buffers. It uses ArchBFD's
relocation howto table to check its actual architecture-specific lowerings.

The standalone reader now exposes validated relocation provenance through
`ArchBFDObjectReloc.source_section`, `.target_section`, and `.has_addend`.
GOT planning and relocation application consume these fields directly; they
no longer independently reconstruct a relocation stream from section sizes.
The reader rejects malformed section/string/symbol/relocation links before
returning an object, including malformed non-allocatable sections.

Path, memory and extracted-archive objects converge on one owned-input
decoder/cleanup path. Link-global addresses are finalized during linking;
read-only API calls use those cached values rather than casting away `const`
to invoke diagnostic-producing resolution. Output symbol section indices
come from defining sections, not ambiguous address-boundary comparisons.

## Build layers

The standalone CMake graph is:

```text
archld_json  (standalone Dsymspec JSON support)
    ↑
archld_bfd   (description-driven object decoding and validation)
    ↑
archld      (CWEB link graph, resolution, layout, relocations, writers)
    ↑
ArchTool    (C++20 CLI and DSLtk manifests/response files)
```

The Make build keeps its original self-contained archive for existing clients.
The CMake `archld` target propagates the separate static dependency targets.
Neither build links the complete Dsymspec implementation into this program.
The toolkit path is configurable and resolves to `../../DSLtk` by default.
The supplied shared `../../third_party` tree needs no extra runtime library
for this implementation: its JSON support is deliberately standalone.

The provided ZIP did not include `DsymspecIR.h` or the Dsymspec parser.
`third_party/dsymspec` is a small standalone JSON tree implementation of the
interfaces ArchBFD calls; this version is **not ABI-compatible** with the
complete DsymspecIR runtime. To regenerate its catalogue, use the PyYAML
conversion tool, not ArchBFD's YAML loading path. Any full ArchToolchain build
with Dsymspec installed should omit this compatibility satellite and use the
native Dsymspec implementation.

Changes here apply to `ArchLD/third_party/archbfd`, whose standalone ABI differs
from the full suite. They are not automatically patches to the root
`MetaToolchain/ArchBFD.c`; integration requires an explicit port and its tests.

## Backends

The target model is currently an explicit ELF backend selection. `ELF64`
selects x86-64 static relocation rules and `Elf64_Ehdr/Phdr/Shdr/Sym` output.
`ELF32` selects i386 static relocation rules and `Elf32_*` output. Additional
formats in the ArchBFD catalogue are format descriptions, **not** automatic
linker writer implementations. New backends require an architecture-specific
relocation lowering, layout policy and serializer, not just YAML constants.

## Safety and provenance

The vendored ArchBFD source has extra bounds and entry-size checks in its ELF
reader for section tables, symbol tables and relocation tables. `DsymArena`
allocations in the compatibility satellite are zero initialized as expected
by the original implementation. The rest of the ArchBFD model remains the
supplied library. The link phase checks image and input size caps, alignment,
array growth, symbol section indices, relocation destination bounds, and
narrow signed/unsigned relocation overflows. It writes to a temporary file and
publishes only after successful serialization.

No claim of complete untrusted-input fuzzing or memory-safety certification is
made. The included tests show meaningful end-to-end use, not formal assurance.

## Literate source workflow

`src/ArchLD.w` is the **source of truth** for the core linker and API. Tangling
from the project root regenerates `src/ArchLD.c` and `src/ArchLD.h`, while
`src/main.c`, the Dsymspec adapter, and vendored ArchBFD source are ordinary C.

```sh
ctangle src/ArchLD.w - src/ArchLD.c
make clean all test
cweave src/ArchLD.w - docs/ArchLD.tex
(cd docs && pdftex -halt-on-error ArchLD.tex)
```
