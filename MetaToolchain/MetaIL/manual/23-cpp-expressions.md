# 23. Native C++ expressions

Create variables with `var("name")`, constants with an integral `Expr` construction
or `integer(value)`, and calls with `call("callee", {args...})`. Expressions own
their trees and copy by value. Their construction performs no represented runtime
arithmetic or function calls.

```cpp
auto x = metail::var("x");
auto masked = (x + 1) & 255;
auto selected = metail::select(x != 0, masked, 0);
auto observed = metail::call("observe", {selected});
```

Overloaded operators include arithmetic, shifts, bitwise operations, comparisons,
unary negation, complement, and logical not. `land(a,b)` and `lor(a,b)` are named
builders; `&&` and `||` are deliberately not provided. `select` is also named
because C++ cannot overload its conditional operator.

Comparison results are expressions, not booleans. Use `a.same_as(b)` for a host
boolean structural comparison. `Expr` has no conversion to bool, preventing
accidental use as a host `if` condition. C++20 operand rewriting still produces
the intended expression for forms such as `0 == x`.

The C++ compiler determines grouping before builders run. Parenthesize bitwise
and comparison combinations explicitly: `(x & 7) == 0`. Host arithmetic involving
only ordinary integers happens before conversion, so write `integer(INT64_MAX)+1`
rather than overflowing a native integer expression first.

`Expr::operation("add", {a,b})` provides a programmatic alternative when the
operator is selected dynamically. Unknown names and wrong arities are rejected.

[Contents](README.md) · [Next: Function builders](24-cpp-functions.md)
