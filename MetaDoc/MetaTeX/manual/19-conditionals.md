# 19 — Conditional input

The native conditional set is `\iftrue`, `\iffalse`, `\ifdefined`,
`\ifnum`, and `\ifdim`, closed by `\fi` with an optional `\else` branch. Integer comparisons
accept `<`, `>`, or `=` and use expanded integer input.

<!-- metatex-example {"format":"plain","text":"ready"} -->
```tex
\count0=7
\ifnum\count0>6 ready\else waiting\fi
```

`\ifdim` scans two distances and compares their exact scaled-point values:

<!-- metatex-example {"format":"plain","text":"equal"} -->
```tex
\ifdim1pt=65536sp equal\else different\fi
```

Dimension aliases and the natural widths of glue registers can supply either
operand. The same `<`, `>`, and `=` relation tokens are accepted. `\ifdim`
also works in expanded definitions and is counted in skipped nested branches.

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
an unrelated caller's conditional. `\ifx`, `\ifcat`, `\ifcase`, and
the full TeX conditional family are not yet available. Rewrite such tests
using supported numeric or definition checks where that is semantically valid.

[Contents](README.md)
