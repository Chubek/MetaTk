# Chapter 10 — Operators and Precedence

**Status:** implemented in the parser's Pratt loop.

All binary operators are syntactically left-associative: `9 - 3 - 1` groups
as `(9 - 3) - 1`. Precedence, low to high:

| Operators | Role |
| --- | --- |
| `\|\|` | Boolean or |
| `&&` | Boolean and |
| `==` `!=` | Equality |
| `<` `<=` `>` `>=` | Comparison |
| `+` `-` | Addition / subtraction |
| `*` `/` | Multiplication / division |
| unary `+` `-` `!` | Prefix (bind tighter than `*`) |
| `f(...)` | Calls, including chains |

So `a || b && c == d < e + f * g` groups as
`a || (b && ((c == (d < (e + (f * g))))))`, and `-f(1)` is `-(f(1))`.

## Disambiguation rules

- Lambda bodies, `let` bodies, and conditional branches consume complete
  expressions. Parenthesize a compound expression before applying an
  operator or call to the whole thing.
- `x--1` is `x` plus a comment (chapter 5); write `x - -1`.
- `::`, `|`, `&`, brackets, and braces lex but have no expression meaning
  yet — they are rejected outside the narrow positions the grammar allows.
