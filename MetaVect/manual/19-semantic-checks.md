# Chapter 19 — Semantic Checks

**Status:** implemented for names and duplicates
(`frontend/Semacheck-Visitor.d`); type checking is future work.

## What runs today

`checkSemantically(module)` walks every binding with the chapter-18
scopes and reports:

- `undefinedName` — e.g. `y = x` with no `x` in scope; a use inside a
  lambda body must see a parameter, an enclosing binding, or a built-in.
- `duplicateDefinition` — a repeated top-level name, a repeated parameter
  in one list (`fn(value: Int, value: Int) => …`), or a repeated `let`
  name in the same scope.
- Correct shadowing passes silently: `let value = 1 in let value = 2 in
  value` is legal, and a name bound inside a lambda is invisible outside
  it (`f = fn() => let hidden = 2 in hidden` followed by `y = hidden`
  fails on the second binding).

Module bindings resolve sequentially: a binding sees the names defined
above it, matching the top-to-bottom reading of the file.

## What does not run

Type comparison, arity checks, import loading, exhaustiveness, effect
tracking, and numeric range validation. `Semacheck-Visitor.d` is a
conservative layer — undefined names and duplicates — and says so in its
header. The next contract is name-and-type validation before MVIR
lowering.
