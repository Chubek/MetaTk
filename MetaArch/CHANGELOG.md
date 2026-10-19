# Changelog

All notable changes are documented here, following Keep a Changelog.

## [Unreleased]

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
