# 28 — Classes and package declarations

`\documentclass[OPTIONS]{NAME}` accepts the structure of a LaTeX class
declaration. Recognized names include `article`, `report`, `book`, `standalone`,
and `minimal`. They do not load class files or switch to complete class-specific
layout engines. Other names produce a fallback warning.

`\usepackage[OPTIONS]{NAMES}` reads a comma-separated list. Familiar names such
as `amsmath`, `graphicx`, `hyperref`, and `geometry` are recognized because
some associated commands have native implementations or approximations.
Recognition does not evaluate a `.sty` file or implement every option; it emits
a native-subset warning. `\RequirePackage` uses the same checks and diagnostics.

For migration, inventory the commands your document actually uses. Begin with
simple text, headings, references, and supported math. Test each added
environment or command against the required backend. A source file that exits
successfully may still rely on accepted declarations with no layout effect,
such as geometry settings or color changes.

There is currently no general package resolver, NFSS font-selection system,
LaTeX kernel bootstrapping, BibTeX/Biber integration, TikZ interpreter, or
arbitrary class compatibility. Add native plugins for concrete semantic
features, or extend the engine with verified primitive behavior before
claiming support for a package that depends on it.

[Contents](README.md)
