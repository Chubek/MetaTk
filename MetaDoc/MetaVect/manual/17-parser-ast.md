# Chapter 17 — Parser and AST

**Status:** implemented (`frontend/Parser.d`, `frontend/AST.d`).

`parse(SourceText)` lexes first, then returns either a complete `Module`
with no diagnostics or a null AST with exactly one diagnostic. Partial
trees are never exposed: a failed parse is not a usable program.

## Strategy

Recursive descent for declarations and types, a Pratt loop for
expressions. Recursion is capped at `maxSyntaxDepth` (256 frames);
exceeding it is a `nestingLimit` diagnostic, not a crash. Flat operator
and call chains (`1 + 1 + …`, `f()()…`) iterate, so 1,000-long chains
parse within the limit.

## Node catalog

`Node` carries a span. `Module` holds an optional name, imports, and
bindings. `Binding` records `exported`, name, optional annotation, and
value. Types are `TypeExpression` trees (name, or domain→range).
Expressions: `Identifier`, `Literal` (integer, floating, string, boolean,
unit — spelling preserved, never converted), `Group`, `Unary`, `Binary`,
`Call`, `Lambda` (with `Parameter` list), `Let`, `If`.

The AST records shape without meaning: no names resolved, no types
compared, nothing folded. `x : Int = "oops"` is a well-formed tree.
