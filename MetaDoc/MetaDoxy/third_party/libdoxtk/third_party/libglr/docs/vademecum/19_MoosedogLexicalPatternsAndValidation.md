# Chapter 19: Moosedog Lexical Patterns and the Validation Philosophy

## 19.1 Purpose and scope

Chapters 17 and 18 treat token patterns as opaque strings. This
chapter opens them up: which regex dialect Moosedog accepts, how the
frontend bridges the gap between specification habits and the engine
libglr actually runs, and the testing discipline that keeps the
whole pipeline honest — reject early with a name attached, never
crash on hostile input.

## 19.2 Two dialects, one engine

libglr's scannerless engine compiles POSIX extended regular
expressions over text input (no embedded NUL). Grammar authors,
however, write PCRE out of habit — both bundled examples select
`MainLexer = "PCRE2"`, and their patterns contain PCRE-isms. Rather
than silently miscompiling them, the frontend normalizes the two
mechanical cases and rejects the rest at validation time, before any
C is emitted.

Normalization (`md_lexer_normalize_pattern`, applied to both the
documented token table and the registered pattern):

- `\xHH` becomes the literal byte. The motivating failure:
  `[^\"\\\\\x00-\x1f]` in the JSON string pattern degraded under
  POSIX rules to a class excluding the *letter* `x`, so generated
  parsers rejected every `"x"` key while accepting `"a"`. After
  normalization the class genuinely excludes control characters.
  `\x00` cannot appear in a C-string regex, so it drops out — except
  in `\x00-`, which becomes `\x01-` (identical on text input, where
  NUL cannot occur). Escaped pairs (`\\`) never introduce an escape.
- `(?:...)` becomes `(...)`. libglr needs only the match span, so
  capturing instead of non-capturing is behavior-preserving here.

Rejection (in `md_lexer_validate`, naming the token): lookaround and
named groups (`(?=` `(?!` `(?<` `(?P` — anything `(?` except `(?:`),
`\p` classes, and `\d`/`\s`/`\w` shorthands (with parity tracking so
a literal `\\d` in the pattern is not misread). `Literal = Yes`
spellings are raw text — `"["`, `"{"` — and skip every regex check,
as do `Classifier` tokens, which have no pattern at all.

## 19.3 Contextual tokens and trivia

`TYPEDEF_NAME` is the canonical classifier token: no regex could
express "currently visible typedef", so the pattern is supplied by
the `VisibleTypedefName` classifier with branch-local scope state
(see the ANSI-C README for the shadowing contract). The generated
builder leaves classifier terminals unconfigured and notes why in a
comment — a silent literal fallback would be a correctness lie.

Trivia has a narrower contract than it appears: libglr skips a
single literal via `glr_parser_set_trivia`, so the generated
`<Base>_create_parser` installs `" "` whenever the grammar declares
skip rules. Full whitespace classes remain the lexer's job, and the
emitted comment says so.

## 19.4 Lua query validation

`Queries` entries must end in `.lua`, exist on disk, fit in 1 MiB,
and pass a block-balance check. The checker (`md_lua_check_syntax`)
skips line and long-bracket comments plus both string forms, then
tracks `function`/`if`/`for`/`while`/`repeat` against `end`/`until`
with word-boundary guards on both sides (so `append` never reads as
`end`). `then`, `do`, `else`, and `elseif` are deliberately
uncounted — an `elseif` carries a `then` without adding an `end`,
and counting it mis-validates real queries. Truncation and stray
closers fail with the file named.

## 19.5 The testing discipline

Three layers, all under the `moosedog` CTest label:

1. **Golden examples.** `--check` and full emission for both bundled
   grammars, plus the EBNF translation of `tests/mini.ebnf` and a
   valid CLI-override combination. These pin the dialect.
2. **Negative fixtures.** `tests/bad-start-rule.grm`
   (dangling `StartRule`), `tests/bad-helper.grm` (unknown
   `Entrypoint` helper, exercising the `stdext` table), and
   `--lookahead-depth 99` must all *fail*; CTest records them with
   `WILL_FAIL` so a future acceptance is itself a failure. Error
   paths are also bounded: diagnostics name the construct, and long
   paths, overlong EBNF alternatives, and oversized queries fail
   with messages rather than truncated output.
3. **No-crash fuzzing.** `tests/fuzz-smoke.sh` feeds the driver
   pure noise, brace soup, unknown-lexer configs, and truncated
   real grammars, asserting every exit code is 0, 1, or 2. Anything
   else — segfault, abort — fails the test. The lexer was
   restructured for this: stray-character skipping is iterative,
   and every allocation failure in the parser, emitter, and
   translators propagates instead of being ignored.

The through-line is deliberate: a generator that emits broken
parsers, or dies on broken input, is worse than no generator. Every
stage of Moosedog either proves its output (generated C compiles
under `-Werror` and parses in `moosedog_smoke`) or refuses its
input with a reason.
