# 16 — Integer syntax and conversion

The integer scanner expands input, skips leading spaces, and accepts repeated
signs. Each minus reverses the sign. Decimal is the default radix; an
apostrophe selects octal and a double quote selects hexadecimal. Hexadecimal
integer digits use `A`–`F`, unlike the lowercase digits in `^^` input notation.

<!-- metatex-example {"format":"plain","text":"42/15/255/65"} -->
```tex
\number--42/\number'17/\number"FF/\number`A
```

Backtick notation reads one character or a one-character control symbol and
uses its code. This form currently accepts the engine's single-byte character
constants; it is not a general Unicode scalar-value parser.

`\number` emits decimal characters. `\romannumeral` emits lowercase Roman
numerals for positive input and an empty token list for zero or negative input.
Both are expandable and can be used inside `\edef`.

Terminate a numeric literal deliberately. A following space is consumed;
a nonnumeric token is generally returned to the input stream. An expandable
command immediately after digits may contribute additional digits, so use a
space or `\relax` when that is unintended. Native integer arithmetic uses the
symmetric range −2147483647 through 2147483647. Missing numbers and overflow
produce diagnostics rather than undefined machine arithmetic.

[Contents](README.md)
