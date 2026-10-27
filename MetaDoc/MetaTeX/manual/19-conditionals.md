# 19 — Conditional input

The native conditional set is `\iftrue`, `\iffalse`, `\ifdefined`, and
`\ifnum`, closed by `\fi` with an optional `\else` branch. Numeric comparisons
accept `<`, `>`, or `=` and use expanded integer input.

<!-- metatex-example {"format":"plain","text":"ready"} -->
```tex
\count0=7
\ifnum\count0>6 ready\else waiting\fi
```

`\ifdefined` tests the following control sequence without expanding it.
It is a convenience extension beyond TeX82. Undefined control sequences in a
skipped branch are not executed. Nested conditionals are counted while
skipping so that an inner `\else` does not select the outer branch.

Taken branches are scanned incrementally. Consequently, a category assignment
can affect later characters in the same branch. Definitions can also expand
to conditional commands. Stored macro bodies, however, retain the categories
they had when read.

Missing `\fi` and unexpected branch terminators produce diagnostics.
Nested token-list execution is isolated from its caller, so do not use a
closing conditional in a style argument or included execution frame to close
an unrelated caller's conditional. `\ifx`, `\ifcat`, `\ifdim`, `\ifcase`, and
the full TeX conditional family are not yet available. Rewrite such tests
using supported numeric or definition checks where that is semantically valid.

[Contents](README.md)
