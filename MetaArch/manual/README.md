# MetaArch manual

MetaArch is a C11 architecture-specification library, catalogue and encoding
toolkit. It validates schema-1 DomSymAsCC specs, converts four document formats,
and executes declared encoder programs. An optional C++20 layer adds a DSL and
small assembler. Python uses a compiled nanobind extension over the same core.

This manual describes the **implemented project**, not the larger development
brief in `AGENTS.md`. Planned features are not presented as working commands.
Commands below run from the MetaArch repository root unless stated otherwise.

## Contents

1. [Build, dependencies and installation](building.md)
2. [Architecture and repository map](architecture.md)
3. [Specification model and serialization formats](specifications.md)
4. [Command-line tools](cli.md)
5. [Encoder language, checked operands and relocations](encoding.md)
6. [C, C++, D and Go APIs](native-apis.md)
7. [Python bindings](python.md) — build, examples and API reference
8. [DSL and assembler](dsl.md)
9. [Adding architectures and maintaining the database](contributing.md)
10. [Testing, troubleshooting and current limitations](testing.md)
11. [Nanobind change verification](verification.md)

## Start here

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
build/archdb info ArchDB/riscv64.dsymspec.yaml
build/archdb encode ArchDB/riscv64.dsymspec.yaml addi rd=10 rs1=10 imm=-1
# 13 05 f5 ff
```

The default build includes C++20/DSLtk. Add `-DARCHDB_BUILD_DSL=OFF` for just
the C library and `archdb`. Python is opt-in; see [its guide](python.md).

Important distinctions:

- Architecture data describes a subset of instructions, not a universal
  assembler/disassembler or a guarantee of ISA completeness.
- Low-level encoding does not validate form metadata. Use checked form encoding
  when its visible-operand mapping is representable.
- Serialization preserves the typed model, **not original comments/formatting**.
- Generated backends in `ArchDB/backend/` are extractor output; do not hand-edit.
- Known unresolved encoder issues are listed in [the ISA audit](../docs/encoder-audit.md).
