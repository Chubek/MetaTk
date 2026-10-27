# 15 — Constructed names and strings

`\csname` expands input until `\endcsname` and constructs a control-sequence
name from the resulting characters. Spaces within the name are significant.
An otherwise undefined constructed name is initialized to an inert command.

<!-- metatex-example {"format":"plain","text":"OK"} -->
```tex
\expandafter\def\csname report:item\endcsname{OK}
\csname report:item\endcsname
```

The `\expandafter` lets the constructed control sequence become the name
consumed by `\def`. A constructed name can contain punctuation that would
normally end a control word. This makes it useful for generated identifiers,
but does not change how a literal `\report:item` would be scanned.

`\string` reads one unexpanded token and emits its spelling as character
tokens. Ordinary control sequences include a backslash; active characters do
not. The generated spelling is text, so braces and parameter characters in
it are not executed as grouping or macro syntax.

<!-- metatex-example {"format":"plain","text":"#${}"} -->
```tex
\string#\string$\string{\string}
```

Native `\string` uses a fixed escape spelling; configurable `\escapechar`
semantics are not present. `\csname` diagnoses unexpandable command tokens
inside a name and missing terminators.

[Contents](README.md)
