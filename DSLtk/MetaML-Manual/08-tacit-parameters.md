# 8. Tacit functions and numbered parameters

Tacit definitions refer to formal parameters with `$1`, `$2`, and so on. The parser discovers the highest index and elaborates the binding into a function. The indices are one-based, bounded to 1024, and belong to the enclosing binding's formal parameters—not to arbitrary runtime stack slots.

```metaml
add :: Int -> Int -> Int
add := $1 + $2
square := $1 * $1
named_square x := x * x
Test.equal "tacit arithmetic" (add 20 22) 42
Test.equal "same behavior" (square 9) (named_square 9)
```

Use one parameter style consistently within a binding. A definition that uses both a named parameter and numbered references to express its computation is rejected as `M0201`. The implementation also permits declared parameter names that are merely placeholders for an otherwise fully numbered body, but this is less readable than a pure tacit definition.

Repeated occurrences of `$1` refer to the same argument value. They do not evaluate the argument repeatedly, duplicate IO, or consume successive items from an implicit stack. A bare `$1` can simply return its argument. In function-valued positions, the checker may resolve juxtaposition as composition, just as it can for named functions.

Tacit notation is especially useful for short arithmetic, field projections, and wrapper definitions. Named parameters usually communicate intent better for branching, recursion, or functions with many arguments. There is no requirement to make an entire module tacit simply because one helper is.

Do not confuse `$n` with `#n`: numbered hashes are local section holes handled by the parser, described in the next chapter. Both forms are checked before evaluation, but they have different binding scopes and syntax.

<!-- nav:begin -->
[Contents](README.md) · [Previous](07-functions.md) · [Next](09-sections.md)
<!-- nav:end -->
