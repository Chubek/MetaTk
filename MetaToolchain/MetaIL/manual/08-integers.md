# 8. Signed-64 integers

Every current value has signed 64-bit integer semantics. The range is
`-9223372036854775808` through `9223372036854775807`. There are no separate floating,
pointer, vector, or boolean program types. Conditions interpret zero as false and
nonzero as true; comparisons produce integer 0 or 1.

Addition, subtraction, multiplication, and negation wrap modulo 2^64. For example,
folding `9223372036854775807 + 1` produces `-9223372036854775808`. Optimizations must
preserve this behavior rather than assume unbounded mathematical integers.

Text accepts decimal and hexadecimal integer tokens. Hexadecimal tokens are
limited to 64 bits and follow Lua integer bit-pattern interpretation; for example,
`0xffffffffffffffff` denotes -1. The parser specially recognizes the decimal
minimum signed value when its magnitude follows unary minus.

In C, use `metail_expr.value` with `kind=METAIL_INT`, and use `INT64_MIN`/`INT64_MAX`
for boundary constants. In C++:

```cpp
auto minimum = metail::integer(INT64_MIN);
auto wrapped = metail::integer(INT64_MAX) + 1;
```

The latter builds an addition node; it does not overflow a host C++ integer.
`Expr` accepts integral inputs that fit signed-64 and rejects out-of-range unsigned
values. Floating-point construction is intentionally unavailable.

[Contents](README.md) · [Next: Expressions](09-expressions.md)
