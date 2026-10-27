# 18 — Register arithmetic

`\advance`, `\multiply`, and `\divide` operate on a count register or count
alias. The optional keyword `by` is case-insensitive. Their operands use the
ordinary integer scanner, so macros, signs, radix prefixes, and other count
registers can supply values.

<!-- metatex-example {"format":"plain","text":"-7"} -->
```tex
\countdef\total=10
\total=7
\advance\total by 5
\multiply\total by -3
\divide\total by 5
\the\total
```

Integer division truncates toward zero. In this example −36 divided by 5
becomes −7, not −8. Arithmetic assignments follow group scope; prefix the
operation with `\global` to keep its result after the current group exits.

Intermediate arithmetic is checked before storing the result. Overflow or
division by zero emits an error and preserves the destination register's
previous value. This preservation is specific to the failed arithmetic
operation, not a rollback of earlier commands in the compilation.

The range is symmetric about zero and excludes the most-negative signed
32-bit machine value. This avoids negation and division corner cases and
matches the intended TeX integer range. Use an external calculation or a
MetaML command for values outside that range, then insert validated textual
results rather than relying on wraparound.

[Contents](README.md)
