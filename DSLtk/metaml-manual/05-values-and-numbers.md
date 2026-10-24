# 5. Primitive values and arithmetic

The primitive types are `Unit`, `Bool`, `Int`, `Float`, and `String`. Unit has one value, `()`. Booleans are `true` and `false`. An `Int` is a signed 64-bit two's-complement value; a `Float` uses binary64. Integer literals are range checked, but ordinary integer arithmetic wraps. Division truncates toward zero and integer division by zero produces `M0402`.

```metaml
Test.equal "division" (7 / 3, (~7) / 3, (~7) % 3) (2, ~2, ~1)
Test.equal "wrapping" (Int.max_value + 1) Int.min_value
Test.equal "checked" (Int.checked_add Int.max_value 1) None
Test.equal "floating conversion" (Float.of_int 3 / 2.0) 1.5
```

Use explicit numeric conversions at API boundaries. `Float.of_int` converts an integer; `Float.truncate` converts back using saturation, with NaN mapped to zero. Arithmetic type constraints do not imply unrestricted automatic coercion of every numeric argument.

Unary negation can be written with `~`, and negative literals are recognized in suitable lexical positions. Parenthesize a negative argument, for example `f (~1)`, to avoid confusing it with infix subtraction. `&&` and `||` short-circuit; ordinary function arguments are evaluated eagerly.

Comparison is structural for lists, tuples, records, and constructors. Functions cannot be compared and cause a runtime diagnostic. Floating-point equality follows IEEE behavior: NaN is unequal to itself. The three-way comparison helper treats unordered floating comparisons differently from a mathematical total order; do not use NaN-containing data as if it had a meaningful sorted order.

Strings are immutable bytes, not arrays of Unicode characters. Chapter 27 distinguishes byte-oriented operations from the optional UTF-8 helpers. Canonical value rendering quotes strings and prints functions as `<fn>`; it is useful for diagnostics, not a universal persistence format.

<!-- nav:begin -->
[Contents](README.md) · [Previous](04-lexical-structure.md) · [Next](06-bindings.md)
<!-- nav:end -->
