# mulib-json

Owned JSON values, RFC 8259 syntax recognition and UTF-8 serialization in ISO
C11. The public declarations are the stable MuLIB JSON consumer contract.

## Integration

In one translation unit define **all four** switches before including the
header:

```c
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#include "mulib-json.h"
```

Direct/transitive module dependencies are exactly **parsec, strbuf, utf8**.
No implementation switch is implicitly defined by a header. Other translation
units include `mulib-json.h` without switches. C++17 declaration-only inclusion
is supported; compile the implementation as C11. Standard C locale, allocation,
floating-point and formatting facilities are used; no external library is used.

## Values, access and ownership

`mulib_json_t` is opaque. Types are `MULIB_JSON_NULL`, `BOOL`, `NUMBER`, `STRING`,
`ARRAY`, `OBJECT` (each enumerator has the `MULIB_JSON_` prefix).

| API (`mulib_json_` prefix) | Contract |
| --- | --- |
| `new(type)` | Allocate null/false/zero/empty value; invalid type returns NULL |
| `bool(value)`, `number(double)` | Allocate a boolean or **finite** double |
| `string(text)`, `string_n(text,length)` | Copy valid UTF-8; `_n` preserves NUL bytes |
| `destroy(value)` | Release an owned tree; NULL is harmless |
| `typeof(value)` | NULL pointer reports JSON null |
| `size(value)` | Array/object member count, otherwise zero |
| `text(value,&length)` | Borrow string bytes, otherwise NULL and length zero |
| `numeric(value)`, `boolean(value)` | Retrieve scalar, otherwise zero |
| `at(value,index)` | Borrow an array element or an object's indexed value |
| `key(object,index,&length)` | Borrow an indexed object's key, including NUL bytes |
| `get(object,key)`, `get_n(object,key,length)` | Borrow a member, or NULL if absent |
| `append(array,value)` | Append an owned value; return 1/0 |
| `set(object,key,value)`, `set_n(object,key,length,value)` | Copy key and insert/replace; return 1/0 |
| `parse(source,length,&error)` | Parse exactly one value, returning an owned root or NULL |
| `stringify(value,pretty)` | Return malloc-owned text or NULL; caller calls `free` |

Strings and keys are length-delimited UTF-8, with a convenience trailing NUL.
Lengths exclude the terminator. `string_n(NULL,0)` and an empty `_n` key with
NULL bytes pointer are valid. Length outputs are optional. NULL-terminated
constructors/lookups require non-NULL input. Borrowed pointers live until their
owner is destroyed or the containing member is replaced. Missing lookups return
a NULL pointer; a present JSON null is a non-NULL value with null type.

Ownership transfers **only on successful** append/set. NULL value pointers,
already-owned values, self-insertion and ancestor cycles are rejected. Construct
JSON null with `new(MULIB_JSON_NULL)` rather than passing NULL to append/set.
Successful replacement destroys the previous member. Setting the same key to its
existing value is a successful no-op. Failed insertion leaves the value owned
by the caller and existing members unchanged. Never independently destroy a
borrowed child or reuse a transferred value as an owner. Destruction is iterative
and does not recurse down manually constructed deep trees.

Objects preserve insertion order. Duplicate parsed keys and repeated `set`
calls use **last value wins**, retaining the original position. Equality includes
the full key length, so `"a"` and `"a\u0000b"` are different members. Object
lookup is linear; this is a small DOM, not a hash-indexed database.

## Parsing, Unicode and numeric scope

The parser validates source UTF-8 and then runs an actual **parsec combinator
grammar**. Public sequence/choice/repeat/reference/capture primitives recognize
the complete JSON grammar, including structural recursion and strict numeric
tokens. Transactional postorder captures build owned values only after the
entire input matches. There is no unrelated recursive-descent syntax parser
hidden behind a nominal combinator wrapper.

Supported syntax includes all six value types, scalar roots, and only the RFC
whitespace bytes space/TAB/CR/LF. Comments, trailing commas, leading BOMs,
additional top-level values, leading-zero numbers, bare plus signs, hexadecimal
numbers, incomplete fractions/exponents, NaN and infinity are rejected. The
explicit source length is always respected; a raw NUL is not an input terminator
and is not legal JSON syntax.

Raw strings must contain valid shortest-form UTF-8 scalar encodings and no
unescaped controls. Escape decoding supports `\" \\ \/ \b \f \n \r \t` and
`\uXXXX`, combining high/low surrogate pairs into UTF-8. Lone or mismatched
surrogates are rejected, including the unpaired code units that RFC 8259's ABNF
can technically spell. Overlong UTF-8, encoded surrogates, truncation and values
above U+10FFFF are rejected. There is no normalization or Unicode case folding.

The value model stores **double**, not arbitrary-precision decimal. Correctly
rounded library conversion is used: finite results are accepted, overflow is
rejected, underflow may become a subnormal or signed zero. Integers outside exact
double precision may round. Serialization uses 17 significant decimal digits for
double round trips, not preservation of the original spelling. LC_NUMERIC's
decimal separator is translated to/from JSON's dot without changing the process
locale. Do not change the global C locale concurrently with these operations.

Parsing and serialization allow **128 nested arrays/objects** by default.
Define `MULIB_JSON_MAX_DEPTH` before implementation inclusion to change this
limit, keeping C stack capacity in mind. The parser additionally applies bounded
parsec evaluator depth, proportional work limits and capture limits to reject
malformed deeply recursive input safely. Manually constructed trees may exceed
the nesting limit, but stringify then returns NULL. All allocation arithmetic
checks for overflow. A source/string length of `SIZE_MAX` is invalid.

## Errors and output

Constructors return NULL on invalid input or allocation failure; mutation returns
zero. Parse diagnostics have a static `message`, zero-based byte `offset`, and
one-based LF-delimited `line` and byte `column`; the optional error is cleared on
success. Farthest speculative parsec expectations can supply syntax diagnostics.
Numeric conversion/allocation failures share a diagnostic because the fixed API
has no separate status code. NULL source is valid only at length zero (which
still fails for lack of a value).

Stringify returns escaped, valid UTF-8 JSON. `pretty != 0` selects two-space
indentation, with no final newline. Embedded NUL/control bytes are escaped;
non-ASCII scalar bytes remain UTF-8. A NULL value pointer stringifies as `null`.

## Runnable example

```c
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#include "mulib-json.h"
#include <assert.h>
#include <stdlib.h>
int main(void) {
    const char source[] = "{\"items\":[true,null,2.5]}";
    mulib_json_error_t error;
    mulib_json_t *value = mulib_json_parse(source, sizeof source - 1, &error);
    char *text;
    assert(value && mulib_json_size(mulib_json_get(value, "items")) == 3);
    text = mulib_json_stringify(value, 1);
    assert(text);
    free(text);
    mulib_json_destroy(value);
    return 0;
}
```

```sh
cc -std=c11 -Wall -Wextra -Werror -Wpedantic -Iinclude examples/modules/json.c -lm -o ./json
./json
```
