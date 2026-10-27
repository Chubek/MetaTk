# MetaTeX manual

This manual describes the native MetaTeX 0.1 C++ engine, its command-line tools,
and its LaTeX, NekoTeX, and MetaVect plugins. It contains **44 chapters**.
Commands are run from the MetaTeX source directory unless stated otherwise.
`build/metatex` denotes the executable produced by a full native build.

MetaTeX currently implements a tested semantic-document subset. TeX82 TRIP
compatibility and full LaTeX compatibility have not been established. Consult
[chapter 44](44-compatibility-and-roadmap.md) for the supported boundary and
[the implementation report](../IMPLEMENTATION-STATUS.md) for current evidence.

## Contents

1. [The document model](01-overview.md)
2. [Building and installing](02-building.md)
3. [Your first document](03-first-document.md)
4. [Command-line reference](04-command-line.md)
5. [Formats and plugin loading](05-formats-and-plugins.md)
6. [Interactive sessions](06-interactive-sessions.md)
7. [Source text and input notation](07-source-text.md)
8. [Category codes](08-category-codes.md)
9. [Active characters](09-active-characters.md)
10. [Groups and delayed tokens](10-groups.md)
11. [Defining macros](11-macros.md)
12. [Delimited parameters](12-delimited-parameters.md)
13. [Expanded definitions](13-expanded-definitions.md)
14. [Controlling expansion](14-expansion-control.md)
15. [Constructed names and strings](15-names-and-strings.md)
16. [Integer syntax and conversion](16-integers.md)
17. [Registers and measurements](17-registers.md)
18. [Register arithmetic](18-arithmetic.md)
19. [Conditional input](19-conditionals.md)
20. [Multi-file documents](20-input-files.md)
21. [Paragraphs and inline styles](21-paragraphs-and-styles.md)
22. [Titles and sectioning](22-titles-and-sections.md)
23. [References and named counters](23-references-and-counters.md)
24. [Lists and quotations](24-lists-and-quotes.md)
25. [Mathematics](25-mathematics.md)
26. [Environments and theorems](26-environments.md)
27. [LaTeX command definitions](27-latex-commands.md)
28. [Classes and package declarations](28-classes-and-packages.md)
29. [NekoTeX documents](29-nekotex.md)
30. [Links, figures, and annotations](30-links-and-figures.md)
31. [MetaVect graphics](31-vector-graphics.md)
32. [HTML and MathML output](32-html-and-mathml.md)
33. [PDF output and fonts](33-pdf-and-fonts.md)
34. [JSON and text output](34-json-and-text.md)
35. [Embedding the C++ engine](35-cpp-engine.md)
36. [The C++ document DSL](36-cpp-dsl.md)
37. [Writing native plugins](37-native-plugins.md)
38. [Readers and sinks](38-readers-and-sinks.md)
39. [MetaML commands](39-metaml.md)
40. [External script commands](40-external-scripts.md)
41. [Diagnostics and resource limits](41-diagnostics.md)
42. [Testing and reference comparisons](42-testing.md)
43. [Architecture and contribution workflow](43-architecture.md)
44. [Compatibility, troubleshooting, and roadmap](44-compatibility-and-roadmap.md)

## Reading and verification

New authors can start with chapters 2–6 and 21–34. Macro authors should read
chapters 7–20 before relying on expansion and scoping. Embedders should start
with chapters 35–38 and consult the public headers for complete declarations.

Executable examples have an adjacent `metatex-example` metadata comment.
The `manual-examples` CTest target compiles those examples to text, JSON, HTML,
and PDF, checks their expected results, and validates the 44-chapter index.
Unmarked shell and C++ snippets illustrate setup or integration and are not
automatically executed by that target. The manual is installed under
`share/doc/metatex/manual/`.
