# The QScript Manual

QScript is a small, statically checked scripting language for signed 64-bit
integers, built entirely from MetaChain parts: its parser is a Raku-style
scannerless grammar running on ParseTk, it compiles to MetaIL, and it runs on
the MetaVM interpreter, on MetaJIT, or as a static x86-64 Linux executable
produced by MetaChain's own assembler and linker.

```qscript
use math, prime;

fn collatz(n) {
    my steps = 0;
    until (n == 1) { n = n % 2 ? 3 * n + 1 : n / 2; steps++; }
    return steps;
}

for (my i = 1; i <= 10; i++) {
    say collatz(i) if is_prime(i);
}
return gcd(84, 36);
```

```sh
$ qscript run collatz.qs
1
7
5
16
result 12
```

## Contents

1. [Getting started](01-getting-started.md): building, the first program, the command line in five minutes
2. [The language](02-language.md): lexical structure, values, operators, statements
3. [Functions and constants](03-functions.md): `fn`, recursion, `const`, scoping rules
4. [Libraries and modules](04-libraries.md): `use`, writing libraries, search paths, multi-file modules
5. [Grammars](05-grammars.md): the Raku-style scannerless grammar engine on ParseTk
6. [The C and C++ API](06-api.md): every function in `QScript.h` and `qscript/include/`
7. [Standard library reference](07-stdlib.md): all 25 libraries, function by function
8. [Command line and REPL](08-cli.md)
9. [Internals](09-internals.md): pipeline, AST, calling convention, limits and diagnostics

## At a glance

| | |
|---|---|
| Values | one type: signed 64-bit integers, two's-complement wrap-around |
| Syntax | C-like, with Perl touches: `my`, `say`, `unless`, `until`, `elsif`, `last`/`next`, `loop`, statement modifiers (`next if i % 2;`), `**`, `#` comments |
| Program structure | top-level statements, `fn` functions (recursive), compile-time `const`, `use` libraries |
| Checks | names, arity, scopes, `break` placement, constant cycles, all before running |
| Runs on | MetaVM interpreter, MetaJIT, or native x86-64 ELF (`qscript build`) |
| Safety | every run is bounded: step limit, stack limit; traps on division by zero, bad shifts, failed `assert` |
| Embedding | C ABI (`qs_compile`, `qs_run`, `qs_call`, ...) and a C++ API |
| Library | 25 embedded libraries: math, bits, primes, number theory, dates, hashing, checksums, fixed point, ... |

What QScript is not: it has no strings, arrays, floating point or heap. Values
are integers; characters are integer codes (`'a'` is 97); several small values
can be packed into one integer (see the `pack`, `date` and `color` libraries).
These limits come from MetaIL, the register IL QScript targets; they keep every
program verifiable and easy to compile to native code.
