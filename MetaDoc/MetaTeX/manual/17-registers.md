# 17 — Integer registers

There are 256 native integer registers, addressed as `\count0` through
`\count255`. Unassigned registers read as zero. Assignment permits an optional
equals sign; `\the` expands a register's current value into decimal tokens.

<!-- metatex-example {"format":"plain","text":"7/9"} -->
```tex
\count0=7
\edef\saved{\the\count0}
\count0=9
\saved/\the\count0
```

Use `\countdef` to give a register a command name:

<!-- metatex-example {"format":"plain","text":"42"} -->
```tex
\countdef\answer=42
\answer=42
\the\answer
```

The alias refers to a register index, not a copy of its current value. Aliases
and assignments are group-local unless prefixed with `\global`. A global
assignment removes earlier local saves for that register; a later local
assignment can still temporarily override it.

Registers persist across `compileFragment` submissions but reset for a fresh
`compile` call. They are separate from LaTeX-style named counters accessed
with `\setcounter` and `\arabic`. Dimension, glue, token-list, and box registers
are not implemented in the native engine; their names are not alternate
spellings for integer registers.

[Contents](README.md)
