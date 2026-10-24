# 7. Functions, application, and closures

Named definitions list their parameters before `:=`. Anonymous functions use `fn ... -> ...`; `fun` is also recognized by the parser. Functions are values, can be passed to other functions, and capture the environment needed by their bodies.

```metaml
add x y := x + y
add_ten := add 10
apply_twice f x := f (f x)
Test.equal "partial application" (add_ten 5) 15
Test.equal "higher order" (apply_twice (fn x -> x * 2) 3) 12
```

Application by whitespace is the most idiomatic form: `f x y`. Parenthesized call syntax is also supported; `f(x, y)` is interpreted as a multiargument call, whereas an explicitly nested tuple is needed when a single tuple parameter is intended. Prefer whitespace and explicit tuple patterns when documenting an API, so currying is visible.

Supplying fewer arguments returns a partially applied function. Supplying enough arguments executes the body; extra arguments can be applied to its result if that result is callable. Argument evaluation is eager and ordered by the evaluator. A closure does not imply lazy arguments or memoization.

Function parameters can be patterns, not just names. A pattern failure is an evaluation diagnostic rather than an implicit fallback to another definition. For alternative cases, write one function with a `match` expression.

Juxtaposition is also used for typed concatenative composition. The checker resolves ordinary application when compatible, and can resolve composition in function-typed situations. This is a distinctive feature, but a reason to provide signatures or use `>>` when application and composition would otherwise be hard to distinguish. Chapter 10 gives explicit alternatives.

**Pitfall:** `map fn x -> ...` needs parentheses around the lambda argument. Write `List.map (fn x -> ...) xs`.

<!-- nav:begin -->
[Contents](README.md) · [Previous](06-bindings.md) · [Next](08-tacit-parameters.md)
<!-- nav:end -->
