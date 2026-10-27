# 01 — The document model

MetaTeX turns UTF-8 TeX-like source into a semantic `Document`. A document
contains metadata, an ordered sequence of blocks, references, and diagnostics.
Paragraphs, headings, lists, mathematical displays, and vector graphics are
different block kinds. Inline text, emphasis, links, and mathematics are spans
inside those blocks.

The processing path is source → incremental scanner → expansion and command
execution → document → renderer. The same document can be rendered as HTML,
PDF, JSON, or text. Renderers accept the document rather than reparsing its
source. This also lets a C++ application construct a document without TeX input.

Choose a format to select available commands: `plain` loads the native engine;
`latex` adds the LaTeX plugin; `nekotex` adds both LaTeX and NekoTeX. Here,
`plain` is a CLI format name, not a claim that Knuth's complete Plain TeX macro
format is loaded.

TeXScrape supplies a separate TeX82 reference implementation. Native regression
tests compare selected results against it. Running a MetaTeX document does not
delegate typesetting to that process. Consequently, native HTML/PDF output and
TeXScrape DVI output have different capabilities and verification histories.

For a first working example, follow [chapter 3](03-first-document.md).

[Contents](README.md)
