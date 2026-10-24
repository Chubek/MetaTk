# Chapter 8 — Names, whitespace and comments

## Identifier alphabet

Names begin with an ASCII letter, underscore or dot. Subsequent characters may
also contain decimal digits or `$`. Examples include `entry`, `_start`, `.loop`
and `value$1`. A name beginning with a digit, containing a slash, or containing
embedded whitespace is not an identifier.

Section names have an additional rule: they begin with a dot. Labels beginning
with a dot are still ordinary names for MetaArchIR's duplicate checks; there is
no per-function scope that makes repeated `.loop` labels distinct.

Directive spellings are lowercase. Identifier comparison is generally exact;
native mnemonic support can follow a database mnemonic's case-insensitive flag.
Do not assume that this also makes register lookup or `Target::forms` insensitive
to case.

## Comment forms

<!-- verify: target=amd64 emit=hex -->
```mair
# A whole-line comment.
    // An indented whole-line comment.
entry: # A comment after a completed label is accepted too.
encode ret // A trailing comment after an instruction.
```

Use `//` for trailing comments on ordinary statements. `#` is not a general
trailing-comment delimiter: retaining it allows native ARM operands such as
`#42`. A `//` sequence inside a quoted string does not end the string.

## Whitespace and preservation

Use spaces or tabs for indentation and token separation. The parser removes the
CR component of a CRLF ending. Raw control characters in native operands are
rejected; encode byte-string controls with escapes instead.

Parsing does not retain indentation or source comments. The C++ method
`Program::comment` creates an explicit comment statement that `render` writes
with `#`; `assemble` ignores it. That method accepts one line and rejects control
characters other than tab, semicolons and `//`, so it is not an arbitrary
multiline text insertion facility.

[Previous](07-language-grammar.md) · [Contents](README.md) · [Next](09-integers.md)
