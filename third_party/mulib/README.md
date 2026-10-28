# MuLIB

A C11 modular single-header utility library with **67 implemented modules**.
Inspired by the simplicity of CTL and Klib; code is original, not vendored from the provided archives.

## Correct implementation-switch syntax

The example shorthand `#MULIB_IMPL_STACK` is not valid C preprocessing syntax.
Use `#define MULIB_IMPL_STACK` **before** `#include <mulib-stack.h>`.

```c
#define MULIB_IMPL_SEXPR
#define MULIB_IMPL_MANPAGE
#define MULIB_IMPL_MICROTCL
#define MULIB_IMPL_ARGPARSE
#define MULIB_IMPL_STACK
#include <mulib-sexpr.h>
#include <mulib-manpage.h>
#include <mulib-microtcl.h>
#include <mulib-argparse.h>
#include <mulib-stack.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    mulib_sexpr_t *sexpr = mulib_sexpr_cont();
    mulib_manpage_t *manpage = mulib_manpage_linux();
    mulib_microtcl_t *microtcl = mulib_microtcl_loadall();
    mulib_argparse_t *args = mulib_argparse_load();
    mulib_stack_t *stack = mulib_stack_create();
    /* Use objects, then release in reverse or any safe order. */
    mulib_stack_destroy(stack);
    mulib_argparse_destroy(args);
    mulib_microtcl_destroy(microtcl);
    mulib_manpage_destroy(manpage);
    mulib_sexpr_destroy(sexpr);
    return 0;
}
```

Build: `cc -std=c11 -Iinclude examples/quickstart.c -lm -o quickstart`.
Run tests: `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`.

## Integration

For multi-translation-unit use, define each implementation macro in exactly one `.c` file and include normal declarations everywhere else. Do not define implementation macros in globally distributed compiler flags. Headers should compile standalone. C++ clients should compile at least the implementation TU as C11 unless they specifically test C++ compatibility.

## Scope and limitations

The term *header-only* here means no prebuilt or separately shipped `.c` files; implementations live in headers behind feature switches. These modules are lightweight building blocks, not complete substitutes for a production Tcl distribution, POSIX manuals toolchain, JSON Schema validator, full Unicode database or cryptographic engine. `mulib-microtcl.h` `loadall()` registers **all MuLIB MicroTcl built-ins**, not external Tcl packages. Documented support and actual APIs determine feature completeness. All names and responsibilities are enumerated in `MANIFEST.yaml`; every header has a dedicated manual in `manual/` and embedded Doxygen text.

## Licensing

MuLIB's new source is MIT-licensed, with no code copied from the supplied CTL and Klib reference archives.

## Extensive examples and test coverage

Each of the 67 module manuals includes a concrete, independently compilable C example.
Those examples are also provided as `examples/modules/*.c` files. Run
`python3 tools/check.py` to compile all 67 with `-Wall -Wextra -Werror` and run them.
Optionally use `cmake -S . -B build -DMULIB_BUILD_MODULE_EXAMPLES=ON` to build and run them
under CTest. `tests/functional.c` covers multi-module usage and representative edge cases;
`tests/all_headers.c` checks that all implementations can coexist in a single C translation unit;
`tests/declarations.c` checks declaration-only inclusion.
