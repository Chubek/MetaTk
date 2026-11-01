# mulib-peg

The compiler also supports EBNF through `mulib_peg_compile_ebnf`, lexical
S-expression forms and `-> action_name` annotations. The optional owned loaders,
AST construction and action dispatch are described in
[mulib-parsec](mulib-parsec.md#owned-grammar-loading). `mulib_peg_run` remains a
capture-only recognizer and does not itself execute actions.

A byte-oriented parsing-expression grammar compiler and matcher, using the real
`mulib-parsec` evaluator. Compiled productions are composed from parsec literal,
set, sequence, ordered-choice, repetition, lookahead, reference and capture
descriptions. This is not a regular-expression approximation of PEG.

## Integration and API

Define `MULIB_IMPL_PEG`, `MULIB_IMPL_PARSEC`, and `MULIB_IMPL_STRBUF` before
including `mulib-peg.h` in one C11 translation unit. Module dependencies are
exactly **parsec and strbuf**. Headers never define dependency switches. Other
translation units can use the C/C++17 declarations without switches.

- `mulib_peg_compile(source,length,error)` copies grammar data into a new owned
  `mulib_peg_t`; NULL indicates a grammar, allocation or limit error. Source may
  contain arbitrary literal bytes. Empty grammars are errors. NULL is a valid
  source pointer only at length zero; `SIZE_MAX` is an invalid length.
- `mulib_peg_destroy(grammar)` frees all rules/nodes; NULL is harmless.
- `mulib_peg_rule_count(grammar)` reports the number of rules (zero for NULL).
- `mulib_peg_rule_name(grammar,index)` borrows the NUL-terminated name or returns
  NULL for an invalid index. Names are ordered by declaration.
- `mulib_peg_run(grammar,start,source,length,entire,options,result)` returns 1
  for success, 0 for mismatch, -1 for argument/resource/allocation failure.
  `start == NULL` uses the first rule; otherwise name lookup is exact and
  case-sensitive. Nonzero `entire` demands complete input consumption. Options,
  diagnostics, prefix lengths and result ownership are those of parsec.

A run initializes its result even on failure. Free its captures with
`mulib_parsec_result_free` before reuse. Each successful named-rule invocation
produces a postorder capture whose tag is the rule's **zero-based declaration
index**, offset/length are its byte span, and `first` identifies its descendant
captures. Lookahead and unsuccessful alternatives do not commit captures.
Capture spans borrow source bytes. Grammar names remain borrowed until grammar
destruction. Compiled grammars are immutable and can be matched concurrently.

## Concrete grammar syntax

```text
grammar     <- rule+
rule        <- Identifier '<-' choice ';'
choice      <- sequence ('/' sequence)*
sequence    <- prefix*
prefix      <- ('&' / '!') prefix / primary ('?' / '*' / '+')?
primary     <- Identifier / literal / class / '.' / '(' choice ')'
Identifier  <- [A-Za-z_] [A-Za-z_0-9]*
```

Space, TAB, CR, LF and `#`-to-LF comments separate tokens outside literals and
classes. Every rule **requires its terminating semicolon**. Forward references
are supported; duplicate and undefined names are rejected. Empty sequences,
empty alternatives and empty quoted literals accept empty input.

- `'literal'` and `"literal"` match exact bytes. Only the matching quote ends the
  literal. Escapes are `\\ \' \" \n \r \t \b \f \v \0 \xHH`; `HH` is exactly
  two hexadecimal digits. Inside classes, `\[ \] \- \^` are also accepted
  (these escapes are permitted in literals as well). Unknown escapes fail
  compilation. There is no Unicode escape or character-property syntax.
- `[abc]`, `[a-zA-Z_]` and `[^a-z]` are byte classes, inclusive ranges and byte
  complements. Classes must be nonempty. Escape `]` to include it; a hyphen at
  either edge or escaped is literal. Reversed ranges are invalid. No POSIX
  named classes or shorthand `\d`/`\w` syntax is implemented.
- `.` matches any single byte, including NUL and LF. For valid Unicode-aware
  parsing, compose explicit UTF-8 byte productions; matching is not by codepoint.
- Concatenation matches a sequence. `/` is **ordered choice**, not regex
  alternation. `('a' / 'ab') 'c'` does not match `abc`: the first choice already
  succeeded and is not reopened after the later `c` fails.
- `?`, `*`, `+` mean zero-or-one, zero-or-more, one-or-more. Repetition is greedy
  and possessive. Only one suffix is allowed. `&e` asserts `e`; `!e` asserts its
  failure. Assertions consume/capture nothing. `!.` asserts end of input.

There are no semantic actions, parameterized rules, lexical modes, automatic
whitespace insertion, bounded repetitions or left-recursion extensions. Named
captures provide the data for a separate semantic pass.

## Errors, safety and complexity

Compile errors provide a static `message`, zero-based byte `offset` and one-based
LF-delimited `line` and byte `column`; success clears the optional error.
Compilation rejects direct/indirect **left recursion**, including recursion
through nullable prefixes and predicates. Nullability is conservative for
predicates: repetition of a potentially empty predicate/production is rejected
even if a more sophisticated reachability analysis could prove it harmless.
Nullable `*`/`+` loops fail compilation; nullable `?` is permitted.

Compiler defaults are **128 nested syntax levels**, **100,000 syntax nodes**;
override `MULIB_PEG_MAX_DEPTH` and `MULIB_PEG_MAX_NODES` before implementation
inclusion. Leading-reference validation additionally bounds traversal at four
times the syntax-depth setting. Compiler allocations check overflow. Keep
override values within the platform's C-stack capacity.

Runtime uses parsec's explicit depth/step/capture limits (defaults 256 evaluator
calls deep and 1,000,000 total calls/captures). Depth includes combinator and
reference nodes; pass appropriate options for deeper right-recursive grammars.
There is **no packrat memoization**: backtracking can take superlinear or
exponential work for adversarial grammars, but the configured step budget stops
it. This implementation does not promise linear-time PEG parsing.

## Runnable example

```c
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PEG
#include "mulib-peg.h"
#include <assert.h>
int main(void) {
    const char grammar[] = "List <- Number (',' Number)*; Number <- [0-9]+;";
    mulib_peg_error_t error;
    mulib_parsec_result_t result;
    mulib_peg_t *peg = mulib_peg_compile(grammar, sizeof grammar - 1, &error);
    assert(peg);
    assert(mulib_peg_run(peg, NULL, "12,34", 5, 1, NULL, &result) == 1);
    assert(result.count == 3);
    mulib_parsec_result_free(&result);
    mulib_peg_destroy(peg);
    return 0;
}
```

```sh
cc -std=c11 -Wall -Wextra -Werror -Wpedantic -Iinclude examples/modules/peg.c -o ./peg
./peg
```
