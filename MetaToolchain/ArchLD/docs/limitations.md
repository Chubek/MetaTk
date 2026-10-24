# Unsupported features and output constraints

This implementation is intentionally **fail-closed** where a construct has no
known-correct translation. It is not a drop-in replacement for a general linker.

**Currently supported:** ordinary little-endian ELF64/x86-64 and ELF32/i386
static ET_EXEC; read-only and writable allocatable sections, COMMON/BSS,
mergable input sections without interning, COMDAT SHT_GROUP, SysV/GNU/BSD
ordinary archives, local/global/weak/absolute resolution, executable segment
writing, ELF static symbol tables, x86-64 GOTPCREL relocation lowering.

**Not yet supported:** non-ELF output (COFF, PE, Mach-O, WASM, HPPA and other
formats in the ArchBFD catalogue); non-x86 ELF; big-endian objects; ELF shared
objects, PIE, dynamic symbol resolution and loader metadata; GNU version
scripts/linker scripts; `--gc-sections`; `--wrap`, `--defsym`, linker-defined
`__start/__stop` arrays; GNU `-r` relocatable linking; ELF TLS and IFUNC;
relocation relaxation, dynamic PLT/GOT, standard libc/CRT startup generation,
general C++ exception/unwind-section handling, section address scripts,
incremental linking, thin archives and LTO bitcode.

**Archive search:** archives are scanned to a fixed point regardless of their
command-line order; this is more permissive than traditional ld one-pass
semantics and should not be relied upon to emulate complex `--start-group`
policies. Nested archives/thin archives are not handled. Members using other
architectures, malformed ELF members and non-object members are rejected
during discovery, with archive/member context. GNU/BSD index metadata is
skipped. Manifest `INPUT`, `LIB`, `SEARCH_DIR` and `INCLUDE` encounter order is
preserved, but this does not change the global fixed-point extraction policy.

**ELF metadata:** allocated input sections are organized into the output
`.text`, `.rodata`, `.data`, and `.bss` regions; per-input section boundaries,
non-allocated debug sections, exact `.eh_frame` index metadata, symbol
visibility, C++ unwind tables and original object-file metadata are not all
reproduced. The writer emits ordinary ET_EXEC rather than shared or position-
independent binaries. Symbols in the emitted `.symtab` have simplified types.

**Testing:** linker output is end-to-end exercised for ELF64 programs without
libc on Linux x86-64, plus ELF32 structural checks. The host used to build
the package does not provide an i386 execution personality. More architecture,
compiler-version, hostile-input, ABI-compatibility and performance testing is
necessary before claiming broad production suitability.

**Reader scope:** extended ELF section/symbol numbering and multiple symbol
tables are explicitly rejected. Ordinary ELF32/64 table entry sizes must match
their descriptions. The standalone COFF/PE inspector paths have not received
the ELF validation/fuzzing coverage described in `REPORT.md`.
