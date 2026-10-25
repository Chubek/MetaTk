# MetaIL manual

MetaIL has three cooperating interfaces: the textual `.mi` DSL, the C11 structured
IR API, and the C++20 native DSL built with DSLtk. All use the same scalar signed-64
IR, analyses, and Optikern optimizer. This manual describes implemented behavior.

Start with chapters 3–6 to build and run a program. C embedders should continue at
chapter 16; C++ DSL users at chapter 21. Chapters 27–34 explain optimization and
kernel discovery. Complete, compiled examples live in [`examples`](../examples).

## Contents — 40 chapters

1. [Purpose and scope](01-purpose-and-scope.md)
2. [Architecture and data flow](02-architecture.md)
3. [Building MetaIL](03-building.md)
4. [Installation and CMake consumers](04-installation.md)
5. [Your first textual program](05-first-textual-program.md)
6. [Your first native C++ program](06-first-native-program.md)
7. [Tokens, names, and comments](07-tokens-and-names.md)
8. [Signed-64 integers](08-integers.md)
9. [Expressions and operators](09-expressions.md)
10. [Calls, effects, and traps](10-effects-and-traps.md)
11. [Modules and functions](11-modules-and-functions.md)
12. [Statements and mutable locals](12-statements.md)
13. [Labels and control-flow graphs](13-control-flow.md)
14. [Branches and selections](14-branches-and-select.md)
15. [Loops and Euclid's algorithm](15-loops-and-gcd.md)
16. [C runtime contexts](16-c-contexts.md)
17. [C expression descriptors](17-c-expressions.md)
18. [C statements, functions, and modules](18-c-modules.md)
19. [C structured operations](19-c-operations.md)
20. [Ownership and error handling](20-ownership-and-errors.md)
21. [C++ Context operations](21-cpp-context.md)
22. [DSLtk AST integration](22-dsltk-integration.md)
23. [Native C++ expressions](23-cpp-expressions.md)
24. [Native C++ function builders](24-cpp-functions.md)
25. [Inspecting and editing modules](25-module-editing.md)
26. [Text and structured IR interoperability](26-interoperability.md)
27. [Optimization pipelines](27-pipelines.md)
28. [Declarative rewrites and guards](28-rewrites-and-guards.md)
29. [Procedural transformations](29-procedural-passes.md)
30. [Control-flow analyses](30-cfg-analyses.md)
31. [Variable and expression analyses](31-dataflow-analyses.md)
32. [Numeric statistics and MetaNum](32-numeric-statistics.md)
33. [KernelRegistry queries](33-kernel-registry.md)
34. [Custom kernels and metadata](34-custom-kernels.md)
35. [Command-line reference](35-cli-reference.md)
36. [Embedding and deployment](36-embedding.md)
37. [Limits and concurrency](37-limits-and-concurrency.md)
38. [Testing and debugging](38-testing-and-debugging.md)
39. [Extending and maintaining MetaIL](39-maintenance.md)
40. [End-to-end workflow and boundaries](40-end-to-end.md)

## Notation

Shell commands assume the repository root unless stated otherwise. C++ fragments
use `metail::` names, sometimes with `using namespace metail` for readability.
Fragments assume a live `metail::Context context` where needed. A fenced code
fragment is an illustration, not necessarily a complete translation unit.
`MetaIL.h` is the authoritative C signature reference; `MetaIL.hpp` is the C++ one.
