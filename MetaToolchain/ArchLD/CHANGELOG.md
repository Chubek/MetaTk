# Changelog

## [Unreleased]

### Added

- `archld_add_memory`: copied object/archive input with diagnostic names.
- Standalone, layered CMake build, compiler/sanitizer options and CTest suites.
- ELF-reader and full-link libFuzzer targets with assembler/compiler seeds.
- ELF32/64 malformed-input regressions, output metadata checks, memory input
  ownership tests, concurrent read-only API tests, CWEB consistency check.
- ArchBFD relocation provenance fields: source/target section and explicit
  addend status. Standalone ArchBFD clients must rebuild for this struct change.

### Fixed

- ELF identification/version/header-size, section extents/alignment, string
  references, symbol/relocation table links, entry sizes and symbol indices
  are checked before the reader exposes an object.
- Bounded descriptor-field reads avoid overflowing offset arithmetic.
- Both relocation consumers use the same validated ArchBFD provenance.
- ELF32 RELA sign-extends negative explicit addends, as required by the
  [System V GABI §6.1](https://gabi.xinuos.com/elf/06-reloc.html#relocation-entry).
  No relocation numbers or architecture HOWTO definitions were changed.
- Ordinary BSS remains included in `.bss`; output symbols retain their defining
  section even at category boundaries. BSS starts at its advertised alignment.
- Unsupported W+X and executable NOBITS sections, TLS/IFUNC definitions,
  malformed archive names/padding/members are rejected explicitly.
- Relocation errors include input, target section, offset and symbol context.
- Read-only post-link queries no longer mutate diagnostics through const casts.
- Map-writing error paths close their stream; ELF32 null/empty output paths
  are checked before dispatching to the writer.
- DSLtk resolves to the workspace dependency, and header edits trigger Make
  rebuilds. Link manifests preserve input/library/search-directory ordering.

## 2026-10-24

- Added optional `larchld` Lua module and documented the Lua scripting API.
  The existing restricted `-T` LDScript-compatible manifest path remains
  available for deterministic command-line linking.
