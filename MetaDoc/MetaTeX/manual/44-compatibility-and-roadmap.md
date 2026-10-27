# 44 — Compatibility, troubleshooting, and roadmap

The native engine is a useful, tested semantic-document subset. It is not yet
a complete TeX82 or LaTeX implementation. Working areas include incremental
category scanning, active characters, macro parameters and expanded
definitions, integer/dimension/glue/token-list registers, selected conditionals, semantic document
plugins, four renderers, vector scenes, and scripting.

Substantial unfinished areas include box and math-glue registers,
typed box–glue–penalty layout, Knuth–Plass line breaking, TeX hyphenation and
page building, full math layout, complete primitive meanings and conditionals,
general LaTeX kernel/package loading, and native DVI emission from `Document`.
Input normalization still lacks `\endlinechar`; full Unicode category tables
and category-aware generated-source insertion are also outstanding.
Dimension scanning currently covers physical units and internal quantities;
font-relative units and magnification still require implementation.

| Symptom | First checks |
|---|---|
| Plugin cannot load | Full native build, module path, matching ABI and ID |
| Undefined command | Selected format, registered plugin, supported command set |
| Macro stops at an unexpected token | Delimiter categories, grouping, numeric lookahead |
| PDF misses glyphs | PangoCairo availability and installed fonts |
| LaTeX options have no visible effect | Whether that class/package behavior is implemented |
| Output seems stale | Exit status and batch errors preventing output replacement |
| REPL retains a definition after failure | Partial execution; use `:reset` |

The next implementation priorities follow the engineering contract: complete
scanner state and token provenance, typed layout and line breaking, shaping
and mathematics, remaining registers, native DVI, package loading, and Lua
integration. Official TRIP and broad LaTeX regressions must pass before a
compatibility claim. Use [the implementation report](../IMPLEMENTATION-STATUS.md)
for current evidence and update it alongside each completed subsystem.

[Contents](README.md)
