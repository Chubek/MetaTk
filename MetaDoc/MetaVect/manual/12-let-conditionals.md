# Chapter 12 — Local Bindings and Conditionals

**Status:** syntax and scoping implemented; branching never executes.

## Local `let`

```mvct
let base : Int = 20 in
    if base > 0 then identity(base + 2 * 3) else 0
```

`let name [: type] = initializer in body` evaluates — eventually — the
initializer, binds it for the body only, and yields the body. The
initializer cannot see the new name; the body can. Nesting is the norm:

```mvct
let x : Int = 1 in let y = 2 in x + y
```

An inner `let` may reuse an outer name (shadowing). A missing `in`, a
missing initializer, or a missing body are all syntax errors.

## Conditionals

```mvct
if base > 0 then identity(base) else 0
```

`if` requires all three parts: condition, `then`-branch, `else`-branch.
There is no `else if`; nest conditionals explicitly, associating to the
right in the usual way. The condition need not be Boolean today — no types
are checked — but writing non-Boolean conditions is a documented future
error.

## Top-level `let` vs local `let`

A top-level `let x = …` (chapter 6) introduces a module binding with no
`in`. A local `let x = … in …` introduces a scoped name with no
persistence. The parser distinguishes them by position: `in` is mandatory
inside expressions and forbidden at module level.
