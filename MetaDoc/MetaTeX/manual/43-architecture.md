# 43 — Architecture and contribution workflow

The native implementation is organized around a few public contracts:

| Location | Responsibility |
|---|---|
| `core/Lexer.cpp` | UTF-8 validation, incremental tokenization, category lookup |
| `core/Engine.cpp` | Source frames, macro arguments, grouping, semantic execution |
| `core/Expansion.cpp` | Expandable primitives and integer scanning |
| `core/Registers.cpp` | Count registers, arithmetic, category assignments |
| `core/Math.cpp` | Supported math-source conversion |
| `device/` | Rendering and DVI validation helpers |
| `plugins/` | LaTeX, NekoTeX, and MetaVect handlers |
| `driver/` | CLI, plugin loading, and interactive sessions |
| `include/` | Public engine, document, builder, and I/O interfaces |
| `tests/` | Native, integration, and reference comparisons |

Preserve the `metatex::Engine` API and `metatex_register_plugin` entry point.
Renderers should consume `Document`; they should not independently interpret
TeX source. Protect scanning, expansion, nesting, and generated output against
nontermination and unbounded growth.

Use TeXScrape as a semantic reference, and verify a behavior before describing
it as compatible. Its separate executable does not finish native integration.
The primitive YAML manifest is reference material, not a code generator or
proof that every named feature works.

For a contribution, identify the semantic contract, implement it in the
appropriate layer, register exact-source integration tests, run the relevant
suite, and update the manual and implementation report. Confirm dependency
repository and license information before adding third-party code. Empty
scaffolds and declarations should remain clearly distinguished from working
subsystems.

[Contents](README.md)
