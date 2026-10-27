# 29 — NekoTeX documents

Select `-f nekotex`, use `nlatex`, or give the CLI a `.nltx` input without an
explicit format. NekoTeX adds semantic authoring commands on top of the LaTeX
plugin, using the same document model and renderers.

<!-- metatex-example {"format":"nekotex","contains":"Note: Keep the example small."} -->
```tex
\section{Working notes}
\note{Keep the example small.}
\tip{Inspect JSON when structure is unclear.}
\callout[Result]{The build passed.}
```

`\note`, `\tip`, `\warning`, and `\exercise` create note blocks with labels.
`\callout[VARIANT]{BODY}` chooses a custom label, defaulting to Information.
`\component{NAME}{BODY}` adds a bracketed component label. Their body tokens
are executed, allowing ordinary inline commands.

Inline helpers include `\concept` for bold text, `\kbd` and `\inlinecode` for
code-style spans, and `\meta` for small caps. `\separator` creates a horizontal
rule. `\codeblock[LANGUAGE]{BODY}` creates a code block; the language argument
is currently consumed without syntax highlighting.

Code arguments still pass through normal TeX tokenization. Comments, spaces,
and braces are therefore not a byte-exact verbatim input mechanism. For
external source with significant whitespace, construct a code block through
the C++ API. NekoTeX provides document semantics, not an additional executable
format or independent package ecosystem.

[Contents](README.md)
