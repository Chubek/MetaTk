# 21 — Paragraphs and inline styles

A blank line or `\par` ends a paragraph. Ordinary whitespace inside a paragraph
collapses to a pending space between visible spans. For semantic styling, use
`\textbf`, `\textit`, `\emph`, `\texttt`, `\textsc`, and `\underline`.

<!-- metatex-example {"format":"plain","text":"A bold word and code."} -->
```tex
A \textbf{bold} word and \texttt{code}.
```

Each argument is executed under the selected style. Declarations `\bfseries`,
`\itshape`, and `\rmfamily` affect subsequent text in the current group.
The IR stores one style kind per span, so arbitrary combinations of nested
font properties are not a complete NFSS-style font model.

The LaTeX plugin adds `\textnormal`, `\textsf`, `\textrm`, and paragraph-spacing
commands. Some are semantic fallbacks: `\textsf` currently produces ordinary
text, and small/medium/big skips end a paragraph rather than inserting exact
TeX glue. `\noindent` is accepted but does not configure TeX paragraph shape.

Backslash-backslash inserts a newline character in text. Its visual treatment
depends on the renderer; it is not a paragraph or page-building command.
Use structural blocks for document organization. Exact baseline spacing,
interword glue, penalties, and TeX line-breaking measurements are not exposed
by the current semantic paragraph model.

[Contents](README.md)
