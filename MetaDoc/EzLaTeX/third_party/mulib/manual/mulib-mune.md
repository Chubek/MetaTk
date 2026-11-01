# mulib-mune

Mune is a header-only interpreter for a **numeric subset of Lua syntax**, not a
full Lua implementation or drop-in Lua C API. Define `MULIB_IMPL_MUNE` once;
include `mulib-mune.h`, link the platform math library if required. No MuLIB
dependencies or third-party runtime.

Create an owned persistent environment with `mulib_mune_create`, evaluate using
`mulib_mune_eval(m, source, length, &number, &error)`, and release it using
`mulib_mune_destroy` (NULL safe). Input is borrowed only for the call. Objects
require external synchronization; independent environments have no shared state.
`mulib_mune_get` copies a named value and leaves output unchanged if absent.

Grammar: global numeric assignments (`x = expression`), optional semicolons,
whitespace, single-line `--` comments, and an optional final `return expression`.
Expressions support decimal/exponent literals, variables, parentheses, unary
minus, `+`, `-`, `*`, `/`, and `%` with usual precedence and left associativity.
Modulo follows the divisor's sign, as in Lua. Values are finite C doubles;
require the C numeric locale for literal conversion. Division/modulo by zero,
nonfinite results and out-of-range literals are errors. An empty script or a
script without return produces zero. Assignments are transactional across the
entire evaluation: an error leaves both environment and output unchanged.

Unsupported syntax is rejected: strings, booleans/nil, local declarations,
functions/calls, multiple assignment/return, comparisons, concatenation,
exponentiation, floor division, control flow, tables, metatables and libraries.
All Lua keywords besides final `return` are rejected. The numeric restrictions,
transaction semantics and limits are deliberate deviations from full Lua.

Limits: 64 KiB source, 10000 lexer steps, 64 expression frames, 128 variables,
63-byte ASCII identifiers, 127-byte decimal literals. Length-delimited embedded
NULs are errors, not end-of-input. Source may be NULL at length zero. Diagnostics
hold a static message, status and byte offset. Cleanup accepts NULL; allocation
failure at creation returns NULL. Evaluation uses bounded temporary storage.

See `examples/modules/mune.c` and `tests/test_new_modules.c`.

`mulib_mune_version()` returns API revision 1.
