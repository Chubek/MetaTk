# Chapter 33 — The FORTH integer-word subset

[Previous](32-jvm-integer-subset.md) · [Contents](README.md) · [Next](34-secd-machine.md)

## Entry point and result

```cpp
std::vector<int64_t> metavm::machines::forth(
    const std::string& source, uint64_t fuel = 1000000,
    size_t stack_limit = 65536);
```

This is a whitespace-tokenized, whole-input signed-i64 word interpreter.
It constructs a dictionary from all colon definitions, then executes the
remaining top-level tokens. It is not an interactive standard Forth compiler
or a persistent environment across calls. Each call returns an owned data
stack vector, bottom element first and top element last.

```cpp
#include "VMLibrary/FORTH/Forth.hpp"
auto stack = metavm::machines::forth(": square dup * ; 6 square 6 +");
// stack == std::vector<int64_t>{42}
```

## Syntax and word semantics

Colon definitions have form `: name body ... ;`. Whitespace separates all
tokens, including delimiters. Signed decimal literals must parse completely
as i64. Names and built-in spellings are case-sensitive. No comment syntax,
string literal, I/O word, or host OS vocabulary is implemented.

| Word | Stack effect / behavior |
|---|---|
| Integer literal | `( -- n )` |
| `dup` | `( a -- a a )` |
| `drop` | `( a -- )` |
| `swap` | `( a b -- b a )` |
| `over` | `( a b -- a b a )` |
| `+ - * / mod` | `( a b -- result )` |
| `=` | `( a b -- flag )`, true = -1, false = 0 |
| `<` | Signed comparison with the same flag convention |

Arithmetic uses Machinery's defined wrapping i64 semantics, including division
edges. These comparison flags are not tagged Machinery booleans, and there
is no implicit implementation of standard Forth control words such as IF/THEN.

## Dictionary behavior matters

Definitions are collected before execution, enabling forward references and
recursion. A later definition with the same name replaces its dictionary entry.
At execution, numeric literals are recognized first, then dictionary words,
then built-ins. Consequently dictionary definitions can shadow built-ins, and
references resolve through the completed dictionary rather than a historical
standard Forth early-binding model.

Unrecognized tokens in a never-executed definition are not a comprehensive
compile-time error certificate. Encountering an unknown word at runtime throws.
Malformed definition names, an unterminated definition, or a stray semicolon
fail during whole-input construction. Do not assume that accepting the source
establishes full Forth syntax or control-flow validity.

## Bounds and errors

Input is capped at 16 MiB. Stack limit must be 1–1,000,000 and defaults to
65,536 values. Explicit execution frames cap the return stack at 4096. Fuel
counts executed tokens, including dictionary invocation tokens, and defaults
to 1,000,000. Recursion uses these frames rather than recursive C++ evaluation.

Underflow, stack/frame exhaustion, unknown words, arithmetic traps, malformed
source, and fuel exhaustion throw `std::runtime_error`. Allocation may propagate
`std::bad_alloc`. There is no partial-success stack, persistent dictionary,
unregister operation, or standard environment query.

To extend the profile, first select a Forth standard/environment and specify
compilation state, dictionary binding, memory representation, and I/O policy.
Do not simply add familiar word names while claiming an entire standard.

**References:** [Forth.hpp](../VMLibrary/FORTH/Forth.hpp), [interpreter](../VMLibrary/FORTH/Forth.cpp).
