# 11 — Defining macros

`\def` stores a token replacement. Parameters are numbered consecutively from
`#1` through `#9`; a replacement can refer only to declared parameters.

<!-- metatex-example {"format":"plain","text":"Hello, reader!"} -->
```tex
\def\greet#1{Hello, #1!}
\greet{reader}
```

An undelimited argument is either one non-space token or one balanced group.
The group's outer braces are removed. Arguments are collected before their
replacement tokens are expanded, so a command passed as an argument keeps its
identity. An invocation with too few arguments produces a diagnostic.

Use `\gdef` for a global definition. `\let` copies an existing macro's current
replacement and parameter structure, rather than referring to future
redefinitions of that macro. General aliases to every primitive meaning are
not yet complete; restrict portability assumptions about `\let` accordingly.

When defining a macro inside another macro's replacement, double the parameter
marker intended for the inner definition. The outer expansion turns `##`
into one parameter token. Definition-time validation rejects skipped numbers,
invalid replacement references, and trailing unpaired parameter markers.

There is no implicit recursion termination. Recursive expansion is subject to
the processing budget described in [chapter 41](41-diagnostics.md).
`\long` and `\outer` definition prefixes are not currently implemented.

[Contents](README.md)
