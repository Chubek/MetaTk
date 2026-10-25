# 9. Expressions and operators

Expressions are integer leaves, variable leaves, operator nodes, or call nodes.
Operators have fixed arity and use these canonical names:

| Family | Names |
|---|---|
| Arithmetic, binary | `add sub mul div mod` |
| Bitwise, binary | `band bor bxor shl shr` |
| Comparison, binary | `eq ne lt le gt ge` |
| Logical, binary | `land lor` |
| Unary | `neg bnot lnot` |
| Ternary | `select` |

Text can use infix symbols or canonical prefix forms: `x + 1` and `add(x,1)`
produce equivalent IR. Precedence, from lowest to highest, is `||`, `&&`, `|`,
`^`, `&`, equality, relational comparisons, shifts, addition/subtraction, and
multiplication/division/remainder. Unary operators bind more tightly.

```text
fn expressions(x) {
  let a = (x + 1) * 8;
  let b = band(a, 255);
  return select(b != 0, b, 1);
}
```

C descriptors use the canonical name in `metail_expr.op`. Query
`metail_operator_arity` to check whether an operator exists and how many arguments
it needs. Unknown names and mismatched argument counts are errors.

The C++ DSL maps overloaded arithmetic, comparison, shift, and bitwise operators
to these same nodes. Use parentheses deliberately: C++ expression parsing follows
C++ precedence, not MetaIL's parser.

[Contents](README.md) · [Next: Effects and traps](10-effects-and-traps.md)
