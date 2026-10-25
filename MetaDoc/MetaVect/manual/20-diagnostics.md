# Chapter 20 — Diagnostics and Error Messages

**Status:** implemented (`frontend/SyntaxError.d`, driver rendering).

## Kinds

Lexical: `invalidEncoding`, `invalidCharacter`, `invalidNumber`,
`invalidString`, `unterminatedComment`. Structural: `unexpectedToken`
(expected construct plus actual token, EOF included), `nestingLimit`.
Semantic: `undefinedName`, `duplicateDefinition`, `unsupportedNode`.

## Format

`formatDiagnostic` renders `file:line:column: error: message`, e.g.
`bad.mvct:2:3: error: unexpected character`. The position is the start of
the offending span; zero-width EOF spans point exactly where input ran
out.

## Reading common errors

- `expected expression, found end of file` — the value after `=` is
  missing (check for a stray newline or an unclosed construct above).
- `expected identifier, found integer` after `x = 1 2` — juxtaposition is
  not application; something is missing between the two expressions.
- `undefined name 'x'` — typo, misordered binding, or a name that only
  exists inside another scope.
- `duplicate declaration of x` — the name is already bound at this level;
  rename or nest intentionally to shadow.
- `syntax nesting limit exceeded` — genuinely hostile nesting (256+
  frames), not long flat chains, which are fine.
