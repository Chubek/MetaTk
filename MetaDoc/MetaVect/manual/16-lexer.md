# Chapter 16 — The Lexer

**Status:** implemented (`frontend/Lexer.d`).

`lex(SourceText)` returns `{ tokens, diagnostics }`. Tokens always end in
exactly one EOF token. Scanning stops at the first error; a failed token
stream must never be used as a program.

## Determinism rules

- UTF-8 is validated up front so malformed bytes cannot hide inside
  comments or strings; bad bytes yield `invalidEncoding` with a span.
- Operators use an explicit longest-match table (`==` before `=`), with
  comment starts consumed first so `--` and `{-` never lex as operators.
- Block comments nest via an iterative depth counter — 2,000-deep nesting
  scans without recursion.
- Numbers and strings follow chapter 5 exactly, including the
  invalid-suffix and bad-escape errors.

## Failure taxonomy

| Kind | Example |
| --- | --- |
| `invalidEncoding` | A `\xff` byte anywhere |
| `invalidCharacter` | `@`, `é`, NUL |
| `invalidNumber` | `1e`, `12abc`, `0xff` |
| `invalidString` | Unterminated, raw newline, `\q`, control chars |
| `unterminatedComment` | `{- outer {- inner -}` |

Every diagnostic carries the offending span — a multi-byte character
reports its full byte range — and the driver renders it as
`file:line:column: error: message`.
