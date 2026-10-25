# Chapter 7 — Bindings

**Status:** syntax and duplicate-definition checks implemented; type
checking and evaluation are future work.

## Shape

```ebnf
binding = ["export"], ["let"], identifier, [":", type], "=", expression ;
```

Examples:

```mvct
export identity : Int -> Int = fn(value: Int) => value
let scale = fun(value: Float, factor: Float) => value * factor
export size : Int = let base : Int = 20 in
    if base > 0 then identity(base + 2 * 3) else 0
```

The annotation is optional and unchecked today: `x : Int = "oops"` passes
the frontend. The initializer is an arbitrary expression — literal, call,
lambda, `let`, or conditional.

## Name rules enforced now

- A top-level name may be defined once per module; redefinition is a
  `duplicateDefinition` error with a source span.
- Lambda parameters must be unique within one parameter list.
- A nested `let` may shadow an outer name; shadowing is legal by design
  (the symbol-table contract requires it).

## Not yet enforced

Types are not compared against initializers, arity is not checked at call
sites, and bindings are not evaluated — not even constant ones. Name
resolution is sequential at module level: a binding may use names defined
above it (chapter 19).
