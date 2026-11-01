# microjs: checked JS-like expressions

`mulib-microjs.h` embeds a small, synchronous expression language. It is **not
ECMAScript**, does not execute browser scripts, and does not claim JavaScript
compatibility beyond the syntax described here. There are no third-party or
other MuLIB runtime dependencies. Its typed values differ from minivm's integer
stack machine, so it uses a bounded direct evaluator rather than coercing values
through that VM.

## Integration

Define `MULIB_IMPL_MICROJS` in exactly one C11 translation unit. Other translation
units include the header normally. Declarations have C++ linkage guards. On Unix:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/microjs.c -lm -o microjs
./microjs
```

## Supported language

Programs are sequences of `let name = expression`, `name = expression`, or
expression statements, separated by semicolons. The final semicolon is optional.
Empty statements and an empty program are valid. Newlines are whitespace, **not**
automatic semicolons. Assignments are statements, not subexpressions. `let` must
initialize a new variable; assignment requires an existing variable. Variables
persist across calls to `mulib_microjs_eval` on the same interpreter.

* Names use ASCII `[A-Za-z_$][A-Za-z0-9_$]*`, are case-sensitive, and cannot be
  reserved keywords. No Unicode identifiers or property access.
* Values: finite `double` numbers, booleans `true`/`false`, `null`, and owned ASCII
  strings. Decimal numbers have digits before the optional fractional point;
  a point requires following digits. An optional `e`/`E` exponent accepts a sign.
  Hex, octal, leading-dot numbers, `NaN`, and `Infinity` are unsupported.
* Strings use either quote style. The only escapes are `\\`, `\'`, `\"`, `\n`,
  `\r`, and `\t`. Raw string bytes must be printable ASCII (32..126); no embedded
  NUL, Unicode escapes, template strings, or raw newline.
* Precedence, lowest first: `||`, `&&`, `=== !==`, `< <= > >=`, `+ -`, `* / %`,
  unary `! + -`, and parentheses. Binary operators associate left. Equality is
  typed: unlike types compare unequal. String equality compares bytes.
* Arithmetic and ordering require two numbers. `+` also concatenates two strings;
  it never coerces numbers or booleans into strings. `%` uses C `fmod` semantics.
* `!`, `&&`, and `||` require booleans. Logical operations short-circuit and return
  booleans. Skipped expressions are still fully syntax-checked and charged to
  parsing limits, but do not resolve names, allocate string values, or perform
  arithmetic/type checks. `false && missing/0` is valid.
* `//` comments continue through end-of-line. Block comments are unsupported.

Functions, calls, objects, arrays, control flow, closures, `var`/`const`, implicit
conversions, loose equality, increment, and host callbacks are not implemented;
they fail diagnostically. This finite statement/expression subset cannot loop.

```c
mulib_microjs_t *js = mulib_microjs_create(NULL);
mulib_microjs_value_t result = {0};
mulib_microjs_error_t error;
const char source[] = "let total=7*6; total===42";
if (js && mulib_microjs_eval(js, source, sizeof source-1, &result, &error)
          == MULIB_MICROJS_OK) {
    /* result.type == MULIB_MICROJS_BOOLEAN; result.boolean == 1 */
}
mulib_microjs_value_free(&result);
mulib_microjs_destroy(js);
```

## Ownership, errors, and limits

The interpreter owns copied variable names and values. Input is borrowed only
for the call. Successful evaluation writes an independently owned last value
(null for an empty program); initialize outputs to zero and call
`mulib_microjs_value_free` before reuse. Even a string result remains valid after
interpreter destruction. Do not shallow-copy owned values. Destroy/free accept
NULL; freeing a value clears it.

Evaluation returns a typed status and optional static diagnostic with byte offset
and 1-based line/column. Arithmetic overflow, division/remainder by zero, unknown
names, duplicate declarations, type mismatches, allocation failures, unsupported
syntax, and resource exhaustion are explicit. Numeric conversion is locale
independent. Conversion retains up to 19 significant decimal digits in bounded
scratch, combines the decimal-point and explicit exponents before scaling, and
uses long-double intermediates. Long mantissas and tiny fractions can therefore
be compensated by an exponent without premature overflow/underflow. Conversion
is approximate, not a correctly rounded decimal-to-binary guarantee; final
magnitudes beyond `DBL_MAX` are rejected. Underflow to zero is accepted.

Output is unchanged on failure. A statement assigns only after its expression
and statement boundary have been validated and all required allocations succeed.
Previously completed statements persist: evaluation is **not** a program-wide
transaction. No interpreter reentry or concurrent mutation is supported.

`mulib_microjs_default_limits()` returns: 1 MiB source, 100000 lexer/evaluator
steps, expression depth 64, 256 variables, 65536 bytes per string/name. A supplied
limits struct uses exact values, including zero. Depth above 128 and overflowing
variable table allocations make creation fail. There are no implicit unlimited
settings. Each token advance and expression invocation consumes a step; scanning
one token/comment is additionally bounded by source length. Strings are copied
for variable reads and assignments. Live string storage is bounded by the
variable limit plus O(depth) temporaries and the final result, each subject to the
per-string limit. Host-owned source/output memory is outside the environment's
lifetime. Creation returns NULL on invalid limits or allocation failure.

`tests/test_language_tools.c` covers expression precedence, persisted assignment,
typed string/boolean operations, short-circuiting, malformed/empty/NUL input,
numeric faults, boundary budgets, allocation failure, and bounded mutations.
