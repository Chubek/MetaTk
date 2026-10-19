# Changelog

All notable changes are documented here, following Keep a Changelog.

## [Unreleased]

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
