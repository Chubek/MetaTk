# MetaVM manual

**A 40-chapter user, embedding, authoring, and maintenance manual.**

This edition describes the source tree as of 2026-11-04. MetaVM's framework
project is version 0.2.0; Machinery reports version 0.1.0 and serializes binary
format version 1. These are different version numbers for different interfaces.
The manual documents implemented behavior, not a promise that every catalog
identity has an executable historical runtime.

## Conventions and reading paths

Unless a chapter says otherwise, shell commands run from
`MetaToolchain/MetaVM`, with `out-manual` as the CMake build directory.
`build/` denotes the separate Python/Make fallback output. Replace installation
prefixes and external-dependency paths with paths you control. No chapter
requires privileged device discovery or implicitly enables optional backends.

Examples labeled **complete** can be saved as files. Other snippets illustrate
an API inside an application that already owns its inputs. Runnable SDK
examples are in [examples/](examples/README.md). Errors and cleanup are part of
the API, not optional production hardening.

Recommended routes:

- **First use:** chapters 1–7, then the native-machine chapter relevant to you.
- **Embed Machinery:** chapters 8–18 and 38.
- **Generate code or author machines:** chapters 19–25, then 37 and 39–40.
- **Review safety:** chapters 11, 13, 16–18, 30, 39–40.

## Part I — Orientation and tools

1. [Purpose, scope, and compatibility](01-purpose-and-scope.md)
2. [Architecture and source-tree map](02-architecture-and-layout.md)
3. [Prerequisites and dependency policy](03-prerequisites-and-dependencies.md)
4. [The Make/Python fallback build](04-fallback-build.md)
5. [CMake configurations](05-cmake-builds.md)
6. [Your first programs](06-first-programs.md)
7. [Command-line reference](07-command-line-reference.md)

## Part II — Machinery and embedding

8. [Values, arithmetic, and truthiness](08-values-and-arithmetic.md)
9. [Modules, functions, and instructions](09-modules-and-instructions.md)
10. [Machinery assembly language](10-assembly-language.md)
11. [Verification, rewriting, and optimization](11-verification-and-optimization.md)
12. [Execution engines, frames, and concurrency](12-execution-engines.md)
13. [Objects, garbage collection, and roots](13-objects-and-garbage-collection.md)
14. [Embedding through the C API](14-c-embedding.md)
15. [Embedding through the C++ wrapper](15-cpp-embedding.md)
16. [Host imports, callbacks, and diagnostics](16-host-imports-and-errors.md)
17. [Resource limits and security boundaries](17-resource-limits-and-security.md)
18. [Serialization, files, readers, and caching](18-serialization-io-and-cache.md)

## Part III — Authoring and generation

19. [The typed VMWeave Builder](19-vmweave-builder.md)
20. [Definition IR and instruction semantics](20-definition-ir.md)
21. [Definition lowering and execution](21-definition-lowering.md)
22. [Real MetaML authoring](22-metaml-authoring.md)
23. [The VMLibrary catalog and legacy adapters](23-catalog-and-legacy-adapters.md)
24. [Generation workflows and generated C](24-generation-workflows.md)
25. [Adding or extending a machine](25-adding-a-machine.md)

## Part IV — Native execution profiles

26. [CHIP-8 state and instructions](26-chip8-machine.md)
27. [CHIP-8 profiles, input, and scheduling](27-chip8-profiles-and-scheduling.md)
28. [CHIP-8 ROM validation and host integration](28-chip8-rom-validation.md)
29. [SSAVM execution and immutable arrays](29-ssavm-execution.md)
30. [SSAVM text, verification, and analysis](30-ssavm-analysis.md)
31. [DRMVM graphics and optional libdrm](31-drmvm-graphics.md)
32. [The JVM integer Code-array subset](32-jvm-integer-subset.md)
33. [The FORTH integer-word subset](33-forth-subset.md)
34. [The SECD closure-machine profile](34-secd-machine.md)
35. [The Krivine weak-head profile](35-krivine-machine.md)
36. [DVI version 2 and event replay](36-dvi-and-replay.md)

## Part V — Documentation, delivery, and maintenance

37. [VMDesc and the MetaTeX integration](37-vmdesc-and-metatex.md)
38. [Installation, packages, and SDK consumers](38-installation-and-sdk.md)
39. [Validation, troubleshooting, and measurement](39-validation-and-troubleshooting.md)
40. [Compatibility limits and development roadmap](40-compatibility-and-roadmap.md)

## Source authority

Public headers define callable interfaces; implementations and regression tests
resolve operational details. The manifest and generated coverage table define
catalog status. Older `docs/ROUND*.md`, reports, and README sections retain
historical observations: an old claim that CMake or matching DSLtk was unavailable
does not override a later validated configuration.

There are **49 catalog profiles, eight native executable subsets, and 41
catalog-only profiles**. Generated portable examples are deliberately separate.
See chapters 23–24 before interpreting a successful result of `42` as evidence
of guest-platform support.
