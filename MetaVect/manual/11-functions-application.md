# Chapter 11 — Functions and Application

**Status:** syntax implemented; calls are never performed.

## Lambdas

```mvct
fn(value: Int) => value
fun(value: Float, factor: Float) => value * factor
fun() => ()
```

`fn` and `fun` are interchangeable introducers. Parameters are
`name [: type]` separated by commas; empty lists are allowed. The body is a
full expression, so `fn() => let h = 2 in h` needs no parentheses.

## Application

```mvct
identity(base + 2 * 3)
f()(1, g(2))
```

Argument lists are `[expression [, …]]` with no trailing comma: `f(1,)`
and `f(,1)` are rejected. Because calls bind tighter than any operator,
`f(x) + 1` is `(f(x)) + 1`. Chained calls nest left: `f()(1)` calls the
result of `f()`.

## Scope of parameters

Parameters live in a fresh scope covering only the body (chapter 18).
Duplicate parameter names in one list are an error; shadowing an outer
name with a parameter is allowed. There is no currying, no partial
application, and no arity checking yet.
