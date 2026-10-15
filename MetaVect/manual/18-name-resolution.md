# Chapter 18 — Name Resolution and Scopes

**Status:** implemented (`frontend/Symtbl.d`).

## Scopes

Three scope kinds form a parent chain: `module_` (one per unit),
`lambda` (parameters plus enclosing names), and `letExpression` (the local
name plus enclosing names). `define` introduces a name and reports whether
it was new; `lookup` walks outward. Shadowing is legal: an inner `let`
reusing an outer name defines cleanly in its own scope.

## Built-ins

A fresh module scope is pre-populated with `Int Float Bool String Unit
Path Color Vector Matrix true false`. These are conveniences for a checker
that does not load imported modules — source spans stay exact for every
declaration and use site, while built-ins carry a placeholder span.

## Design note

The table resolves *names*, not *meanings*. It cannot say whether `Int`
is a type or a value, whether an import exists, or whether two same-named
bindings in different modules collide. Those are later milestones; the
table's job is lexical structure, and it does that job completely.
