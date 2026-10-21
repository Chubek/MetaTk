# Changelog

All notable changes are documented here, following Keep a Changelog.

## [Unreleased]

### Added — ArchBFD C++, XBind specs and language bindings
- `ArchBFD.hpp`: standalone C++17 RAII API over the object-format catalogue
  (`archbfd::api`): `Catalogue`, `Spec`, `SpecView` and `Object`, with copied
  values, `std::logic_error` for closed handles and `archbfd::api::Error`
  carrying the diagnostics.  It needs neither DSLtk nor `MetaArch.hpp`.
- `MetaArch.xbind`, `ArchBFD.xbind`, `MetaBFD.xbind`: XBind binding specs for
  the architecture component, the object-format component and the aggregate of
  both.  They list the full target inventory and bind the query and lifecycle
  functions; the out-parameter and buffer APIs are deliberately left out because
  XBind cannot marshal them yet.  The specs validate, their C signatures match
  the headers and the Lua emitter lowers them (checked by CTest when the XBind
  tool is configured).
- `bindings/d/archbfd.di`: the D interface of the object-format catalogue,
  including the diagnostics collector of the core library.
- `bindings/go/archbfd`: a Go/cgo package over the same API with copied
  values, an explicit `Close` and structured diagnostics.
- `bindings/python`: the `metaarch.archbfd` subpackage over the nanobind
  extension, with type stubs, plus the `ArchBFD*` classes in the extension.
- Tests: `tests/test_archbfd_cpp.cpp` (the C++ API), `tests/test_bindings_bfd.d`
  (the D interface) and `bindings/go/archbfd/archbfd_test.go` (the Go package),
  and `bindings/python/tests/test_archbfd.py` (the Python subpackage).

### Added — ArchBFD object-format catalogue
- `ArchBFD.h` / `ArchBFD.c`: a second catalogue next to `ArchDB`, describing the
  object formats BFD (binutils/bfd) implements — the container layouts, the
  numeric constants and the relocation ("howto") tables.
- `ArchBFD/*.bfd.yaml`: documents for ELF64, ELF32, the HP PA-RISC and m68k
  ELF emulations, COFF, PE, Mach-O, IRIX/ECOFF and WebAssembly, with the
  binutils files they were derived from recorded in every document.
- `ArchBFD/schema/MetaBFD.schema.json`: the document schema (draft 2020-12).
- `ArchBFDCatalogue.yaml`: the catalogue index, regenerated with
  `archbfd catalogue ArchBFD --root ArchBFD -o ArchBFDCatalogue.yaml`.
- `tools/extract_bfd.py`: generates the documents from a binutils checkout
  (2.22 was used; `--binutils` selects it) by parsing the byte layouts in
  `include/elf/external.h`,
  `include/coff/*.h` and `include/mach-o/external.h` and the relocation tables
  in `bfd/elf64-x86-64.c`, `bfd/elf32-i386.c`, `bfd/elf32-m68k.c`,
  `bfd/elf-hppa.h`, `bfd/coff-i386.c` and `bfd/coff-mips.c`.  The output is
  deterministic; `--check` fails when the committed documents are out of date.
- `tools/archbfd.c`: the `archbfd` tool — `validate`, `convert`, `list`,
  `catalogue`, `info`, `relocs`, `records` and `object`.
- Object reader: `archbfd_object_open()` reads ELF, COFF and PE objects using
  the description (record layouts, constant groups and relocation table), and
  reports the sections, symbols and relocations it found.
- Container inheritance: an emulation document may inherit the records and
  constants of its container document (`container: ELF32`); the catalogue
  resolves the reference.
- Tests: `tests/test_archbfd.c` (round trips through the four formats, the
  binutils facts, the diagnostics, the object reader and the catalogue),
  `tests/check_bfd_schema.py` (schema validation) and `tests/check_bfd_data.py`
  (cross-check against an object produced by the system compiler), all wired
  into CTest.
- `manual/bfd.md`: the object-format guide.

### Fixed — packed field encodings
- An encoding is read as `(value >> shift) & mask`; the mask describes the
  width of the extracted part.  The first version masked before shifting, which
  made the symbol index of an ELF64 relocation always read as zero.

### Added — Python bindings
- `bindings/python`: Python 3.9+ nanobind extension over ABI v1, built using a
  configurable local checkout or installed nanobind; static/shared core support.
- Spec load/parse, metadata, named/checked encoding, copied relocations,
  positioned diagnostics, backend loading and all four serialization formats.
- Context-managed ownership, native destructor fallback, GIL-serialized calls,
  and checked integer/path/name conversion without silent truncation.
- 24 unittest cases, optional build/test CMake integration, installed extension,
  PEP 561 type information and full Python usage/API guide. No native ABI changes.
- `manual/`: project-wide guide covering builds, data flow, schema/formats, CLI,
  encoder semantics, native/foreign APIs, DSL, contribution workflow and limitations.

### Added — opaque C ABI and language bindings
- `MetaArchABI.h` / `MetaArchABI.c`: version-1 owning handles, structured
  diagnostics, metadata views, load/parse/write and named/form encoding.
  Results own relocation names and can outlive the spec; views have explicit
  lifetime rules and no dependency on native Dsym structure layouts.
- `MetaArchAPI.hpp`: standalone header-only C++17 RAII API (`metaarch::api`),
  independent of the existing C++20/DSLtk API (`metaarch::Spec`).
- `bindings/d/metaarch.di`: native C-linkage D interface with matching fixed
  view layouts, integer widths and explicit ownership.
- `bindings/go`: dependency-free Go module using cgo, copied results/errors,
  explicit Close and synchronization against concurrent reads/close/backend load.
- C/C++ ABI tests and optional shared-library D/Go interoperability tests;
  installation rules and examples in `docs/BINDINGS.md`.
- Fetched libyaml/yyjson remain static for shared MetaArch builds, avoiding
  accidental new shared-library runtime dependencies for binding consumers.

### Changed — relocation bit metadata (Phase B, second increment)
- `DsymEncodedReloc` now exposes field-relative LSB `bit_offset`, `bit_width`,
  and resolved field `endian`, retaining the existing byte offset/size fields.
  This changes the ABI of both `DsymEncodedReloc` and `DsymEncoded`; rebuild consumers.
- Honour relocation `offset` and validate containment without overflowing
  `offset + width`. Subword relocations remain valid.
- `DSYM-W004` diagnoses non-fitting fields under lint, with strict promotion;
  encode-time rejection applies regardless of load flags.
- Tests cover partial fields, explicit/target byte order, repeated relocations,
  multiple emitted fields and extreme metadata values. No spec data changes.

### Added — checked form encoding (Phase B, first increment)
- `dsym_encode_form`: validates explicit positional operand mapping, scalar
  bounds/signed widths, and hardware register encodings against class and width.
- Mapping skips hidden and implicit operands; unrepresentable count and
  relocation-parameter mappings fail rather than guessing.
- Stable lint codes `DSYM-W002` (count) and `DSYM-W003` (relocation kind),
  promoted to errors with strict + lint. No spec data or schema changes.
- Focused boundary, register-alias, mapping, null-input and real RISC-V tests.
- Encoding entry points reject NULL output/argument arrays instead of crashing.

### Fixed — ppc32 / mulli
Use a full 65536 wrap, not a 32768 bias. Reference: LLVM PowerPC MULLI,
DForm_base (PPCInstrInfo.td / PPCInstrFormats.td); five llvm-mc boundary cases.

### Fixed — ppc32 / li
Use a full 65536 wrap. Reference: LLVM LI, DForm_2_r0; five llvm-mc cases.

### Fixed — ppc32 / addi
Use a full 65536 wrap. Reference: LLVM ADDI, DForm_base; five llvm-mc cases.

### Fixed — ppc32 / addis
Use a full 65536 wrap. Reference: LLVM ADDIS, DForm_base; five llvm-mc cases.

### Fixed — ppc64 / mulli
Use a full 65536 wrap. Reference: LLVM MULLI, DForm_base; five LE llvm-mc cases.

### Fixed — ppc64 / li
Use a full 65536 wrap. Reference: LLVM LI, DForm_2_r0; five LE llvm-mc cases.

### Fixed — ppc64 / addi
Use a full 65536 wrap. Reference: LLVM ADDI, DForm_base; five LE llvm-mc cases.

### Fixed — ppc64 / addis
Use a full 65536 wrap. Reference: LLVM ADDIS, DForm_base; five LE llvm-mc cases.

### Fixed — mips32 / addi
Use a full 65536 wrap. Reference: LLVM MipsInstrInfo.td ADDi,
simm16_relaxed and ADDI_FM; five llvm-mc cases.

### Fixed — mips32 / addiu
Use a full 65536 wrap. Reference: LLVM ADDiu, simm16_relaxed and ADDI_FM;
five llvm-mc cases. The instruction's unsigned name does not zero-extend its immediate.

### Fixed — mips64 / addi
Use a full 65536 wrap. Reference: LLVM ADDi, simm16_relaxed and ADDI_FM;
five llvm-mc cases.

### Fixed — mips64 / addiu
Use a full 65536 wrap. Reference: LLVM ADDiu, simm16_relaxed and ADDI_FM;
five llvm-mc cases.

### Added
- Table-driven independent-assembler regressions in `tests/encodings.tsv`.
- Folded-RPN biased-modulo lint `DSYM-W001`, strict rejection and explicit
  trailing `# biased` opt-out; no public API changes.
- Complete 51-site inventory, references and 39 unresolved sites in
  `docs/encoder-audit.md`.
