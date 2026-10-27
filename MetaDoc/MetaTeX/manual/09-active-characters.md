# 09 — Active characters

Category 13 makes a character behave as a command. Define it with `\def` after
changing its category:

<!-- metatex-example {"format":"plain","text":"A/B"} -->
```tex
\catcode`\~=13
\def~{A}
\def\~{B}
~/\~
```

The active `~` and the control symbol `\~` have distinct identities. Their
definitions can therefore coexist. Active-character macros expand in normal
execution and expanded definitions, accept parameters, and obey group scope.
`\noexpand` can preserve an active token during one expansion step; `\string`
prints the active character without adding an escape character.

Defining a macro while `~` is ordinary text stores an ordinary character.
Making it active later does not turn that stored token into a command.
Likewise, a stored active token remains active after the source category
changes back. This distinction matters for delayed execution and delimiters.

The native engine does not install Plain TeX's active-character definitions.
For example, the default `~` is an ordinary character rather than a predefined
nonbreaking-space macro. Establish the category and definition explicitly
when a document depends on active behavior. Undefined active commands are
document errors, just like undefined control sequences.

[Contents](README.md)
