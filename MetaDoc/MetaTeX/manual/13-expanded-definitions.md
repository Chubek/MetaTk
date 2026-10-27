# 13 — Expanded definitions

`\edef` expands replacement input while the definition is scanned. `\xdef`
does the same globally. Unexpandable commands are retained for later execution;
they are not executed merely because they occur in an expanded definition.

<!-- metatex-example {"format":"plain","text":"old/new"} -->
```tex
\def\value{old}
\edef\snapshot{\value}
\def\value{new}
\snapshot/\value
```

Use `\noexpand` to preserve a particular command token for the resulting macro:

<!-- metatex-example {"format":"plain","text":"new"} -->
```tex
\def\value{old}
\edef\later{\noexpand\value}
\def\value{new}
\later
```

Parameter references remain valid in expanded definitions: expansion and
parameter substitution are distinct operations. Register values inserted by
`\the` are convenient snapshots, because they become character tokens before
the definition is installed.

An unprotected undefined control sequence in an expanded definition is an
error. Recursive definitions remain subject to expansion budgets. The scanner
implements expansion for the native primitive set; this is not evidence that
all TeX82 expansion operations, primitive aliases, or file-reading expansion
semantics are available. Consult the command's chapter when combining it with
`\edef` rather than assuming that every command is expandable.

[Contents](README.md)
