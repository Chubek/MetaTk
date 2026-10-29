# mulib-regex

A self-contained, case-sensitive **byte regular-expression** compiler and
Thompson-NFA matcher in ISO C11. This module does not call POSIX regex or use
third-party code. It has **no module dependencies**. Define `MULIB_IMPL_REGEX`
in exactly one translation unit; other units can use C/C++17 declarations.

## API and ownership

- `mulib_regex_compile(pattern,length,error)` returns an owned compiled regex or
  NULL. Pattern bytes are copied into internal states and need not remain alive.
  Empty patterns match empty input. NULL pattern is valid only at length zero.
- `mulib_regex_destroy(regex)` releases all states; NULL is harmless.
- `mulib_regex_search(regex,source,length,start,match)` finds the
  **leftmost-longest** match whose offset is at least `start`, returning 1 on a
  match, 0 on no match, or -1 on invalid arguments/allocation failure. A non-NULL
  `mulib_regex_match_t` receives byte `offset`/`length` and is cleared on failure.
  `match == NULL` is permitted for boolean testing.
- `mulib_regex_full_match(regex,source,length)` requires a match from byte zero
  to byte `length`, returning 1/0/-1 under the same rules.

Input is borrowed only for the call, may contain arbitrary bytes including NUL,
and is never modified. A NULL source is valid at zero length. `start == length`
permits an end-of-input empty match; `start > length` and length `SIZE_MAX` are
invalid. There are no substring allocations or hidden mutable match objects.
Compiled expressions are immutable and can be shared by concurrent matches.

Compile errors provide a static `message`, zero-based byte `offset`, and
one-based LF-delimited `line` and byte `column`. The optional compile error is
cleared on success. Matching failures do not allocate diagnostics.

## Concrete supported syntax

```text
expression  := sequence ('|' sequence)*
sequence    := (atom quantifier?)*
atom        := literal | escape | class | '.' | '^' | '$' | '(' expression ')'
quantifier  := '?' | '*' | '+' | '{m}' | '{m,}' | '{m,n}'
```

- Literals match exact bytes. Escape the metacharacters
  `\ . ^ $ | ? * + ( ) [ ] { }` to match them literally. Escaped `-` is also
  supported. Whitespace is ordinary literal data; there are no pattern comments.
- `.` matches any one byte **except LF**. `^` asserts absolute input beginning;
  `$` asserts absolute input end (not the position before a final LF). They are
  assertions wherever they occur, not context-dependent literal punctuation.
  Starting a search later does not redefine the anchor positions.
- `[abc]`, `[a-zA-Z_]`, `[^0-9]` support byte sets, inclusive ranges, and
  complements across all 256 byte values. Empty classes, reversed ranges and
  shorthand sets as range endpoints are invalid. Escape `]` to include it;
  `-` at an edge or escaped is literal. A leading unescaped `^` negates the set.
- `\d` is `[0-9]`; `\w` is `[A-Za-z_0-9]`; `\s` is space, TAB, CR, LF, FF and
  VT. `\D`, `\W`, `\S` are their full byte complements. They work inside and
  outside classes and are independent of locale.
- Control escapes are `\n \r \t \f \v \0` and `\xHH` (exactly two hex digits).
  `\0` means one NUL, not an octal prefix. Binary pattern bytes are also allowed
  directly because compilation takes an explicit length.
- Parentheses group without capturing. `|` means regular-language alternation,
  **not** ordered PEG choice. Empty groups/sequences/alternatives are supported.
- `?`, `*`, `+` mean 0..1, 0..infinity, 1..infinity. Counted bounds are unsigned
  decimal integers with no whitespace. `{0}` is legal. Nullable expressions may
  be repeated safely; the NFA closes epsilon cycles without looping forever.

The only returned capture is the **whole match**. There are no numbered/named
groups, backreferences, lookaround, word boundaries, Unicode properties,
normalization, case-insensitive/multiline flags, POSIX named classes, lazy or
possessive quantifiers, conditionals or atomic groups. Unknown escapes and
repeated quantifiers fail compilation rather than silently changing meaning.
UTF-8 is treated as bytes; `.` can match one continuation byte. This module does
not claim PCRE or complete POSIX syntax compatibility.

### Match selection and repeated searches

The earliest possible starting byte wins; among matches at that byte the longest
one wins regardless of alternative ordering. Thus `a|ab` matches `ab` rather
than `a`. This is not Perl's first-alternative policy. Search includes empty
matches. To enumerate safely, advance to `offset + length` for nonempty matches;
after an empty match advance one byte, unless already at input end.

## Limits and complexity

Default compile limits are **128 group nesting levels**, **100,000 AST nodes
and NFA states**, **1,000 per explicit repeat bound**, and **1,000,000 emission
steps**. Override `MULIB_REGEX_MAX_DEPTH`, `MULIB_REGEX_MAX_STATES`,
`MULIB_REGEX_MAX_REPEAT`, `MULIB_REGEX_MAX_EMIT_STEPS` before implementation
inclusion. Nested bounded repetitions expand into NFA states and fail
compilation if the state budget is exhausted. Each AST visit during expansion
also consumes an emission step, independently of whether it creates a state.
This bounds nested repetition of empty groups and `{0}` children, which can
otherwise require enormous work while producing only an accept state. Exhausting
this budget returns NULL with the static diagnostic **`regex compilation work
limit`**, releasing all partial compiler/NFA allocations. The limit may reject
otherwise valid expressions; it never changes their matching meaning or treats
nullable-but-consuming expressions as empty. Every compile has a fresh budget.

Compiler traversal is also depth-limited to four times the group-depth setting.
Keep overrides within the platform's stack budget. Allocation arithmetic,
repeat-count arithmetic and the emission counter are checked for overflow.

Matching uses parallel NFA states, **not recursive exponential backtracking**.
It uses O(states) working memory, independent of source length, and does not use
the C call stack. The epsilon-closure worklist preserves earliest start offsets;
a state can be revisited when an earlier origin arrives, so a conservative
worst-case bound is O(input bytes × states²), not a promise of O(bytes × states).
Typical simple patterns need linear work per byte. Large input/state products
can still be expensive; there is no runtime timeout or work-budget parameter.

## Runnable example

```c
#define MULIB_IMPL_REGEX
#include "mulib-regex.h"
#include <assert.h>
int main(void) {
    const char pattern[] = "[A-Za-z_][A-Za-z_0-9]*";
    mulib_regex_error_t error;
    mulib_regex_match_t match;
    mulib_regex_t *re = mulib_regex_compile(pattern, sizeof pattern - 1, &error);
    assert(re);
    assert(mulib_regex_search(re, "42 answer_1!", 12, 0, &match) == 1);
    assert(match.offset == 3 && match.length == 8);
    mulib_regex_destroy(re);
    return 0;
}
```

```sh
cc -std=c11 -Wall -Wextra -Werror -Wpedantic -Iinclude examples/modules/regex.c -o ./regex
./regex
```
