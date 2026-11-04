# Chapter 10 — Machinery assembly language

[Previous](09-modules-and-instructions.md) · [Contents](README.md) · [Next](11-verification-and-optimization.md)

## Lexical and structural rules

Assembly is line-oriented ASCII syntax. LF and CRLF work; the final newline is
optional. Spaces, other ASCII whitespace, and optional commas separate operands.
`#` or `;` begins a comment. Identifiers match
`[A-Za-z_][A-Za-z0-9_]*` and are at most 255 bytes. Literals are decimal with
an optional minus sign; counts and register indices are unsigned decimal.
Embedded NUL bytes are invalid.

```text
.import import_name arity
.function function_name arity registers
local_label:
    instruction operands
.end
```

Imports are module-level declarations. Labels occupy their own line and are
local to a function. Function, import, and label forward references are allowed.
Duplicate or unresolved names are rejected. A function must contain valid code
and may not have a fallthrough path past its last instruction.

## Instruction reference

| Syntax | Meaning |
|---|---|
| `nop` | No effect, still consumes a step |
| `const ra N`; `nil ra`; `bool ra 0\|1` | Construct values |
| `mov ra rb` | Copy a value |
| `add/sub/mul/div/mod ra rb rc` | Integer arithmetic |
| `and/or/xor/shl/shr ra rb rc` | Integer bit operations |
| `eq/lt/le ra rb rc` | Comparisons; result is boolean |
| `not ra rb` | Negate truthiness |
| `jmp label`; `jz ra label` | Unconditional/false branch |
| `call ra function rc` | Function call using contiguous arguments at `rc` |
| `host ra import rc` | Host call using the same argument-window convention |
| `ret ra` | Return a tagged value |
| `array ra rb` | Allocate `rb` nil elements |
| `get ra rb rc` | Read array `rb` at integer index `rc` |
| `set ra rb rc` | Store value `rc` into array `ra` at index `rb` |
| `len ra rb` | Array element count or string byte length |

Slash-separated names above are alternatives, not literal slash syntax.
Assembly does not provide string literals; strings enter through embedding
arguments or host callbacks.

## A complete call example

```text
.function main 0 3
const r0 40
const r1 2
call r2 sum r0
ret r2
.end
.function sum 2 3
add r2 r0 r1
ret r2
.end
```

The forward call takes two arguments because `sum` declares arity 2. Its
registers are unrelated to the caller's identically spelled registers. Run this
through `metavm-run` to obtain `42`.

## Compiler API and diagnostics

`mvm_compile(source, length, max_bytes, &module, &error)` accepts a bounded
buffer, not necessarily a NUL-terminated string. A zero byte-limit argument
selects the default. Success returns an owned, verified module; failure clears
the output and reports a status. Parser diagnostics use one-based source lines.
Binary modules do not retain these source lines, so preserve source/artifact
provenance separately when diagnostics must map back to an authoring language.

**References:** [grammar header](../Machinery/TerseSyntax.h), [Compiler.h](../Machinery/Compiler.h).
