# 25 — Mathematics

Use `$...$` or `\(...\)` for inline mathematics, and `$$...$$` or `\[...\]`
for display mathematics. The LaTeX plugin also accepts `equation`, `equation*`,
`align`, and `align*` environments as mathematical displays.

<!-- metatex-example {"format":"plain","contains":"x^2"} -->
```tex
Energy depends on $x^2$.
\[\frac{a+b}{\sqrt{c}}\]
```

Math source is stored in a math span. The dedicated math parser converts a
subset to MathML: fractions, roots, scripts, common Greek letters, and selected
operators. HTML embeds that MathML so browser support affects final appearance.
JSON retains the mathematical source for other consumers.

This is not a TeX math-list and font-metric layout engine. Math input does not
pass through every ordinary text expansion step, and arbitrary user macros
inside it are not guaranteed to work. `align` does not implement a complete
alignment table with equation numbering. PDF math uses the backend's basic
representation rather than browser MathML layout or TeX-quality construction.

Keep expressions small and inspect the target output. An unterminated math
delimiter is a document error; deeply nested math is bounded. For a required
complex mathematical feature, add a parser/layout regression before treating
the presence of its control-sequence spelling as evidence of support.

[Contents](README.md)
