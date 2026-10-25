# 7. Tokens, names, and comments

Use ASCII identifiers matching `[A-Za-z_][A-Za-z0-9_]*`. Names are case-sensitive:
`value` and `Value` identify different locals. The same spelling convention applies
to module, function, parameter, local, label, and call names.

Whitespace separates tokens but indentation has no semantic role. Statements end
with semicolons except labels, which end with a colon. Three line-comment forms
are recognized in textual programs:

```text
# a comment
// another comment
-- another comment
fn identity(x) { return x; }
```

The `--` comment token means that repeated unary negation should be parenthesized
or separated: write `-(-x)` rather than `--x`.

`true` and `false` are integer literals 1 and 0 in expressions. Avoid using them
as local names. Canonical operator names such as `add` and `select` are recognized
as operators when followed by parentheses; they cannot identify ordinary call
expressions. Statement keywords such as `return` cannot be label names.

Dollar-prefixed captures, such as `$x`, belong to rewrite patterns, not programs.
The program parser rejects them. The native APIs validate names too; passing a
string like `x; return 1` does not inject source because structured construction
never assembles a program from those strings.

[Contents](README.md) · [Next: Integers](08-integers.md)
