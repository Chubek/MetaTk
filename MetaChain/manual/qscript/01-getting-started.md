# 1. Getting started

## Building

QScript is part of the MetaChain CMake project and needs a C++20 compiler and
the DSLtk header (`DSLtk.hpp`; a copy ships in `satie/include`).

```sh
cmake -S . -B build -DMETACHAIN_DSLTK_DIR=$PWD/satie/include -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target qscript qscript-repl
ctest --test-dir build -R qscript --output-on-failure
```

This produces:

| Target | What it is |
|--------|------------|
| `libQScript.a` (`MetaChain::QScript`) | the C/C++ library; link it to embed QScript |
| `qscript` | the command-line driver |
| `qscript-repl` | a standalone interactive prompt (same as `qscript --repl`) |

The grammar (`qscript/grammar/QScript.grammar`) and the standard library
(`qscript/stdlib/*.qs`) are embedded into the library at build time, so the
binaries need no files at run time.

> `METACHAIN_BUILD_BACKEND` builds an optional backend-algorithms library that
> QScript does not need. Pass `-DMETACHAIN_BUILD_BACKEND=OFF` if its third-party
> dependency (`satie/third_party/memtkx`) is not present.

## A first program

Save as `hello.qs`:

```qscript
# Sum the odd squares below 100.
my total = 0;
for (my i = 1; i * i < 100; i += 2) {
    total += i * i;
}
say total;
return total % 7;
```

```sh
$ qscript run hello.qs
165
result 4
```

`say` (or `print`) writes one integer per line. `return` at the top level ends
the program; its value is the program's result, reported as `result N` by
`qscript run` and used as the exit status of native executables.

## Five minutes with the command line

```sh
qscript run prog.qs              # run on the interpreter
qscript --jit run prog.qs        # run on MetaJIT
qscript check prog.qs            # parse and check only
qscript build prog.qs -o prog    # static x86-64 Linux executable
./prog; echo $?                  # output, then the result as exit status
qscript ir prog.qs               # the MetaIL it compiles to
qscript call prog.qs -- fib 30   # call one function with arguments
qscript ast prog.qs              # the AST, as an S-expression
qscript tree prog.qs             # the raw grammar match tree
qscript grammar                  # the QScript grammar itself
qscript libs                     # the standard library
qscript --repl                   # interactive
```

See [chapter 8](08-cli.md) for every option.

## Using a library

```qscript
use date;
say day_of_week(2026, 10, 16);    # 5 (Friday)
say easter(2027);                 # 20270328
return date::is_leap(2028);       # qualified names work too
```

## Embedding

```c
#include "qscript/QScript.h"
#include "qscript/include/QScript-Runtime.h"

qs_context *c = qs_create();
const char *src = "use math; fn hyp(a, b) { return isqrt(a*a + b*b); }";
qs_program *p = qs_compile(c, src, strlen(src));
int64_t args[] = {3, 4}, r;
if (p && qs_call(c, p, "hyp", args, 2, &r) == QS_OK) printf("%lld\n", (long long)r);  /* 5 */
else fprintf(stderr, "%s\n", qs_error(c));
qs_program_destroy(p);
qs_destroy(c);
```

Link with `MetaChain::QScript`. [Chapter 6](06-api.md) documents the whole API.
