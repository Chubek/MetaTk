# MetaArchIR manual

This manual describes the implemented C++20 API and `.mair` language in
[`MetaArchIR.hpp`](../MetaArchIR.hpp), and the `marchir` command-line tool.
MetaArchIR is **portable assembly**: explicit instructions, physical registers,
data and relocation records, backed by MetaArchDB. Retargeting does not imply
automatic translation between instruction sets.

## Reading conventions

- Run shell commands from the **MetaArch source directory**, which contains
  `CMakeLists.txt`, `MetaArchIR` and `MetaArchDB`.
- Commands use `build/marchir`; substitute your build directory if necessary.
- Positive `.mair` examples state their target or carry a verification annotation.
  Native assembler syntax and deliberately invalid examples are identified in prose.
- C++ fragments belong inside a function unless they show a complete translation
  unit. Include `MetaArchIR/MetaArchIR.hpp` and link `MetaArch::ir`.
- Hexadecimal output examples describe bytes in output order, not host integers.
- Form indices and available encoders belong to a particular database revision.
  Recheck them when updating MetaArchDB.

## Contents: 35 chapters

### Foundations
1. [The portable-assembly model](01-portable-assembly.md)
2. [Components and data flow](02-components.md)
3. [Building and installing](03-building.md)
4. [Your first program](04-first-program.md)
5. [Finding and loading MetaArchDB](05-database-loading.md)
6. [Inspecting targets and instruction metadata](06-target-inspection.md)

### Textual language
7. [Language grammar and statement structure](07-language-grammar.md)
8. [Names, whitespace and comments](08-names-and-comments.md)
9. [Integers and numeric boundaries](09-integers.md)
10. [Byte strings and escaping](10-byte-strings.md)
11. [Native assembly statements](11-native-assembly.md)
12. [Named encoder statements](12-encoders.md)
13. [Checked instruction forms](13-checked-forms.md)
14. [Physical registers and operand constraints](14-registers.md)
15. [Labels and global symbols](15-symbols.md)
16. [Sections and section-relative layout](16-sections.md)
17. [Alignment and padding](17-alignment.md)
18. [Data directives and byte order](18-data-and-endianness.md)

### C++ API and outputs
19. [Building programs in C++](19-program-api.md)
20. [Typed operands and explicit bindings](20-operand-api.md)
21. [Parsing and diagnostics](21-parsing-and-errors.md)
22. [Ownership, lifetimes and concurrency](22-ownership.md)
23. [Rendering assembler text](23-rendering.md)
24. [Assembling section images and flat bytes](24-images.md)
25. [Relocations and linker handoff](25-relocations.md)
26. [The JSON image format](26-json-images.md)

### Tools and practical work
27. [CLI option reference](27-cli-reference.md)
28. [Files, pipelines and exit statuses](28-files-and-pipelines.md)
29. [Resource limits](29-resource-limits.md)
30. [RISC-V: a checked decrement function](30-riscv-walkthrough.md)
31. [AArch64: addition and return](31-aarch64-walkthrough.md)
32. [amd64: sum of squares](32-amd64-compute.md)
33. [amd64: Euclid's GCD algorithm](33-amd64-gcd.md)
34. [Retargeting and application integration](34-retargeting.md)
35. [Verification, troubleshooting and maintenance](35-verification.md)

Start with chapters 1–4 for a tutorial. Chapters 7–18 form the language reference;
19–26 explain embedding and output contracts. CLI users can turn directly to
27–29. The final six chapters connect the reference material to executable
examples and an independent verification workflow.

For a shorter overview, see the [project README](../README.md).
