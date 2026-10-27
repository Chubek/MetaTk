# 27 — LaTeX command definitions

The LaTeX plugin supports `\newcommand`, `\renewcommand`, and
`\providecommand`. A declaration takes a command name, optional argument
count, optional first-argument default, and a braced replacement.

<!-- metatex-example {"format":"latex","text":"Hello, reader! Hello, Ada!"} -->
```tex
\newcommand{\greet}[1][reader]{Hello, #1!}
\greet{} \greet[Ada]
```

The argument count includes the optional first argument. For example,
`[2][left]` declares one optional argument followed by one required argument:

<!-- metatex-example {"format":"latex","text":"left/right other/right"} -->
```tex
\newcommand{\pair}[2][left]{#1/#2}
\pair{right} \pair[other]{right}
```

An explicit empty `[]` overrides the default with an empty token list. Default
tokens expand when the macro is invoked, so a command in the default observes
its invocation-time definition. Definitions and defaults obey macro scope.
At least one parameter is required when a default is supplied, and the total
cannot exceed nine.

`\newcommand` diagnoses an existing name; `\renewcommand` requires an existing
name; `\providecommand` leaves an existing definition intact after reading the
declaration. Parameter references are validated before installation. Starred
declarations are accepted, with the same argument-scanning behavior as unstarred
ones. Robust-command machinery and `xparse` specifications are not available.

[Contents](README.md)
