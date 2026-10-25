# Chapter 15 — Tokens

**Status:** implemented (`frontend/Token.d`, the single source of truth).

## Token kinds

`eof`, `identifier`, `integer`, `floating`, `stringLiteral`; one kind per
reserved word (`kwLet` … `kwFalse`); operators and delimiters from `(` to
`||` (chapter 5 lists the spellings).

## Two total functions

- `tokenName(kind)` spells any kind for diagnostics via a `final switch`:
  adding or renaming a kind fails to compile until it is named there.
- `keywordKind(spelling)` returns the reserved kind for a spelling, or
  `identifier`. It derives its table from `tokenName`, so the two cannot
  drift. Matching is case-sensitive; `Let` is an identifier, `Int` is an
  identifier, `letdown` is an identifier.

## One-file modules

A sibling `.di` file is deliberately absent: D emits no code for
interface-only modules, and when both exist the compiler silently uses the
`.d` and ignores the `.di`. A source-tree test fails the build if any
module ever gains both. `Token` itself is a plain struct — kind, exact
lexeme, span — with compiler-generated equality.
