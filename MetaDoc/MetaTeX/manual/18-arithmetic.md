# 18 — Register arithmetic

`\advance`, `\multiply`, and `\divide` operate on count, dimension, or glue
registers and their aliases. The optional keyword `by` is case-insensitive.
For counts, operands use the ordinary integer scanner, so macros, signs,
radix prefixes, and other count registers can supply values.

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

The count range is symmetric about zero and excludes the most-negative signed
32-bit machine value. This avoids negation and division corner cases and
matches the intended TeX integer range. Use an external calculation or a
MetaML command for values outside that range, then insert validated textual
results rather than relying on wraparound.

For dimensions, `\advance` scans a distance, while `\multiply` and `\divide`
scan an integer factor. Arithmetic is performed in scaled points; division
truncates toward zero at scaled-point precision. The magnitude limit is
1073741823sp, and an overflowing operation leaves the prior value intact.

Glue multiplication and division act on width, stretch, and shrink together,
preserving their orders. Addition sums widths. Stretch and shrink are combined
separately: equal orders add, and the higher nonzero order dominates when
orders differ. Zero components do not override a nonzero component's order.

<!-- metatex-example {"format":"plain","text":"3.0pt plus 2.0fil"} -->
```tex
\skip0=1pt plus 2fil\relax
\advance\skip0 by 2pt plus 4pt\relax
\the\skip0
```

All glue components are checked before committing a result. Overflow in just
the stretch or shrink component preserves the complete previous glue value.
Arithmetic on token-list registers is diagnosed rather than silently ignored.

[Contents](README.md)
