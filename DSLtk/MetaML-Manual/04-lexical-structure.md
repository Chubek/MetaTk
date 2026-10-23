# 4. Source text, tokens, and layout

Value identifiers usually begin with a lowercase letter or underscore; modules and constructors begin uppercase. Subsequent identifier characters include letters, digits, underscores, and apostrophes. Keywords such as `module`, `type`, `val`, `let`, `match`, and `fn` cannot be used as ordinary names.

Line comments begin with `--`. Block comments use `{-` and `-}` and may nest. A leading `#` has several roles: numbered section holes, named directives, or comment text when it does not match those forms. Prefer `--` for comments so a word after `#` is not accidentally interpreted as a directive.

```metaml
{- An outer comment {- with a nested comment -} -}
decimal := 1_024
hexadecimal := 0x400
binary := 0b10000000000
Test.equal "literal bases" (decimal, hexadecimal, binary) (1024, 1024, 1024)
```

Numeric separators belong between digits. Trailing or repeated underscores are rejected, rather than silently normalized. Strings accept common escapes, `\0`, and Unicode scalar escapes such as `\u{1f600}`. Surrogates and values above the Unicode range are invalid. Raw strings start with `r"` and leave backslashes uninterpreted.

Although blocks have explicit delimiters, the current parser uses newlines and columns to detect top-level boundaries. Put separate definitions at column one, and indent continued expressions. Parentheses make a continuation unambiguous. Do not assume arbitrary reindentation is semantics-preserving.

The lexer returns tokens, diagnostics, and an `incomplete` flag. Unterminated strings and block comments can request REPL continuation. Lexical errors use the `M01xx` family. Numbered placeholders are bounded to 1024 to prevent overflow and accidental enormous generated parameter lists; zero is rejected later as an invalid index.

**Sources:** `lex`, `LexResult`, `Token`/`MToken` in the two runtime files.

<!-- nav:begin -->
[Contents](README.md) · [Previous](03-repl.md) · [Next](05-values-and-numbers.md)
<!-- nav:end -->
