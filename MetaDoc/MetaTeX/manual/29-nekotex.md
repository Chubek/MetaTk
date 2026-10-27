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

`\note`, `\tip`, `\warning`, `\important`, `\summary`, and `\solution` create
note blocks with optional titles. Each also supports an environment form.
`\exercise` creates a numbered note with an optional title and label value.
`\callout[VARIANT]{BODY}` chooses a custom label, defaulting to Information.
`\component{NAME}{BODY}` adds a component title. Their body tokens execute in
a local group, allowing inline commands. Paragraphs retain their note role;
nested notes restore the outer role. JSON stores `role`; HTML uses `data-kind`.

Inline helpers include `\concept` for bold text, `\kbd` and `\inlinecode` for
code-style spans, and `\meta` for small caps. `\separator` creates a horizontal
rule. `\codeblock[LANGUAGE]{BODY}` creates a literal code block; language is
retained in JSON and HTML without syntax highlighting.

Direct-source code preserves whitespace, comments and backslashes. Brace
arguments require balanced literal braces. Use
`\begin{codeblock}{LANGUAGE}...\end{codeblock}` or `\codefile[LANGUAGE]{PATH}`
for arbitrary braces. The environment requires a language group, which may be
empty. Files resolve relative to the current source and have a 4 MiB limit.
PDF code uses Courier with preserved indentation. Already-tokenized macro
arguments retain only what survived their original scan.

<!-- metatex-example {"format":"nekotex","contains":"  value = 100 # preserved"} -->
```tex
\codeblock[python]{  value = 100 # preserved
}
```

See [the plugin contract](../docs/PLUGINS.md) for complete syntax and limits.

[Contents](README.md)
