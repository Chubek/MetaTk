# mulib-parsec

Composable, length-delimited **byte** parsers, implemented in ISO C11. This is a
real parser-combinator engine: the same sequence, choice, repetition, lookahead,
reference and capture operations implement MuLIB JSON and compiled PEG grammars.
It does not tokenize or interpret Unicode implicitly.

## Integration and lifetime

Define `MULIB_IMPL_PARSEC` in exactly one translation unit before inclusion.
There are **no module dependencies**. The declarations have C++ linkage guards.

Parser descriptions are inexpensive, copyable values. Constructors do not
allocate and do not own their arguments. Child arrays, literals, bitmaps,
reference slots, labels and predicate user data must remain alive and unmoved
through a run. A reference slot can be assigned after constructing its parser,
allowing recursive grammars. Predicates must be pure: backtracking can call a
predicate more than once, and semantic side effects would not be rolled back.
Independent runs do not modify parser descriptions; immutable grammars can be
shared by threads.

`mulib_parsec_run` initializes its result, including on failure. Call
`mulib_parsec_result_free` before reusing an already populated result. Do not
pass an uninitialized result to `result_free`. The result owns only its capture
array; source bytes remain borrowed. A diagnostic's `expected` is a static
message or a borrowed parser label, not allocated text.

## API

| Constructor | Meaning |
| --- | --- |
| `empty()` | Accept without consuming |
| `byte(b)`, `range(lo,hi)` | Consume one exact byte or inclusive range |
| `set(bitmap)` | Consume a member of a borrowed 32-byte bitmap; bit `b & 7` of byte `b >> 3` |
| `any()` | Consume any byte, including NUL/LF |
| `literal(bytes,length)` | Match exact bytes, including embedded NUL |
| `end()` | Accept only at input end |
| `sequence(items,count)` | Match every child in order; empty sequence succeeds |
| `choice(items,count)` | Try alternatives in order; empty choice fails |
| `repeat(child,min,max)` | Greedy repetition; `SIZE_MAX` means unbounded |
| `lookahead(child,positive)` | Positive (`positive != 0`) or negative assertion |
| `capture(child,tag)` | Record a successful child's span |
| `ref(&target)` | Late-bound, borrowed reference |
| `satisfy(predicate,user,label)` | Consume one byte satisfying a pure callback |
| `named(parser,label)` | Copy a description with a borrowed diagnostic label |

All constructor names above have prefix `mulib_parsec_`. All child arguments
except sequence/choice arrays are pointers to parser descriptions.

`run(parser,source,length,options,result)` returns **1** on a prefix match,
**0** on mismatch, **-1** on invalid arguments/descriptions, allocation failure,
progress failure or a resource limit. A NULL source is permitted at length zero.
Append `end()` in a sequence when the entire input must match. On success,
`result.consumed` is the prefix length and `error` is cleared. Failure has no
committed captures and consumes nothing.

### Backtracking and progress

Every failed parser restores both position and capture count. Sequence failure
therefore restores its successful earlier children; choice tries the next
alternative from that original position. **Choice is ordered PEG choice**:
after an alternative succeeds, later sequence failure does not reopen that
choice. Repetition is similarly possessive: it does not give bytes back to a
later parser. Lookahead always discards its captures and consumes nothing.

Repetition with `max > 1` treats a successful zero-byte child as
`MULIB_PARSEC_NO_PROGRESS`, rather than looping forever or silently inventing a
result. Optional parsers (`max == 1`) may accept empty input. Fatal errors are
never swallowed by a choice, optional parser or negative lookahead.

Capture records are in **postorder**. For capture at array index `i`, its fields
are `tag`, `offset`, `length`, and `first`; records in `[first,i)` are all of its
descendants. A leaf has `first == i`. This makes transactional AST construction
possible without executing non-rollbackable semantic actions during recognition.

### Diagnostics and limits

On failure `error` contains a zero-based byte `offset`, one-based LF-delimited
`line` and byte `column`, an error code, and `expected`. Mismatch diagnostics
retain the farthest failed expectation, including speculative alternatives and
repetition termination; negative lookahead suppresses its internal expectations.
For a named composite, deeper child expectations take precedence over its label.

`mulib_parsec_options_t` contains `max_depth`, `max_steps`, `max_captures`.
Zero fields or NULL options select defaults: **256 nested evaluator calls**,
**1,000,000 calls**, **1,000,000 captures**. Speculative work counts toward these
limits. There is no packrat memoization or left-recursion algorithm: right
recursion is supported, left recursion terminates with a depth-limit error.
Use reasonable limits for untrusted grammars. Depth counts combinators, not
just semantic nesting. The evaluator uses the C call stack; callers must not
raise depth beyond their platform's stack budget. Allocation sizes are checked
for overflow. Input length `SIZE_MAX` is invalid.

## Runnable example

```c
#define MULIB_IMPL_PARSEC
#include "mulib-parsec.h"
#include <assert.h>
int main(void) {
    mulib_parsec_parser_t digit = mulib_parsec_range('0', '9');
    mulib_parsec_parser_t digits = mulib_parsec_repeat(&digit, 1, SIZE_MAX);
    mulib_parsec_parser_t parts[2], root;
    mulib_parsec_result_t result;
    parts[0] = mulib_parsec_capture(&digits, 7);
    parts[1] = mulib_parsec_end();
    root = mulib_parsec_sequence(parts, 2);
    assert(mulib_parsec_run(&root, "123", 3, NULL, &result) == 1);
    assert(result.count == 1 && result.captures[0].tag == 7);
    mulib_parsec_result_free(&result);
    return 0;
}
```

The checked-in example additionally checks a mismatch:

```sh
cc -std=c11 -Wall -Wextra -Werror -Wpedantic -Iinclude examples/modules/parsec.c -o ./parsec
./parsec
```
