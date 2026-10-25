# Chapter 5 — Lexical Structure

**Status:** implemented (`frontend/Lexer.d`, `frontend/Token.d`).

## Source text

Input is UTF-8 without a BOM. Identifiers are ASCII:
`[A-Za-z_][A-Za-z0-9_]*`. Columns count bytes, not Unicode characters or
tab stops. Line breaks are LF, CRLF, or lone CR. Whitespace (space, tab,
CR, LF) is insignificant; newlines never terminate anything.

## Comments

- `--` starts a line comment running to end of line.
- `{- ... -}` is a nestable block comment, scanned iteratively (deeply
  nested comments cannot overflow the call stack).

Because `--` wins at the lexer, `x--1` is `x` followed by a comment. Write
`x - -1` for subtraction of a negative value.

## Numbers

Integers are decimal digits: `42`, `01`. Floats need a fraction, an
exponent, or both: `1.25`, `1e3`, `4.5E-2`. The fraction requires digits on
both sides of the dot; the exponent requires digits after an optional sign.
Rejected: numeric suffixes (`12abc`, `0xff`), bare exponents (`1e`, `1e+`),
hex notation, `.5`, and `1.`. Note `1.` lexes as integer `1` plus `.`, and
`.5` as `.` plus integer `5`.

## Strings

Double-quoted, UTF-8 allowed. Valid escapes are exactly `\n`, `\r`, `\t`,
`\"`, and `\\`. Raw newlines, control characters, other escapes (`\q`),
and unterminated strings are errors. Token lexemes keep their exact source
bytes: the lexer never decodes escapes or converts numbers to host values.

## Operators and delimiters

Two-character operators (longest match wins): `->`, `=>`, `::`, `==`,
`!=`, `<=`, `>=`, `&&`, `||`. Single-character operators: `+ - * / = ! <
> | &`. Delimiters: `( ) [ ] { } , . : ;`. `()` reaches the parser as two
paren tokens; only the parser reads the pair as a unit literal.
