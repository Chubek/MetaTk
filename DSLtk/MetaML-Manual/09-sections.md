# 9. Operator sections and explicit holes

Sections turn an operator expression into a function. The familiar `(+ 1)` increments its input, and `(* 2)` doubles it. An infix operator by itself in parentheses, such as `(+)`, denotes the corresponding curried function.

```metaml
inc := (+ 1)
double := (* 2)
reverse_sub := (#2 - #1)
Test.equal "sections" (inc 4, double 4, reverse_sub 3 10) (5, 8, 7)
Test.equal "operator reference" (List.fold_left (+) 0 [1, 2, 3]) 6
```

Explicit `#1`, `#2`, and subsequent holes describe argument positions within a parenthesized section. The parser rewrites the section into a lambda before type checking. Repeating a hole reuses that argument, rather than generating another formal parameter. The largest index determines the generated arity; sparse numbering can therefore create unused parameters.

Indices must be positive, and indices above 1024 produce a lexical diagnostic. The bound protects both host implementations from overflowing an integer conversion or generating an enormous lambda. Oversized input must produce a normal language error, not a C++ exception or D arithmetic wraparound.

Sections follow the same operator precedence rules as ordinary expressions. Parenthesize a larger computation when needed, and prefer named lambdas if its meaning depends on remembering several hole indices. For subtraction involving negative values, explicit parentheses avoid ambiguity with unary negation.

Sections are not directives. `#type` is a directive name, `#1` is a hole token, and an ordinary comment is better written with `--`. The parser rejects holes outside a suitable section context rather than providing global placeholder variables.

**Related:** chapter 8 distinguishes tacit formals; chapter 10 shows sections in pipelines.

<!-- nav:begin -->
[Contents](README.md) · [Previous](08-tacit-parameters.md) · [Next](10-pipelines.md)
<!-- nav:end -->
