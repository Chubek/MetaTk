# Chapter 18: Moosedog Code Generation and Build Integration

## 18.1 Purpose and scope

Chapter 17 describes what a `.grm` specification says. This chapter
describes what Moosedog produces from it, how the outputs plug into
the libglr build, and how the frontend itself is extended. It assumes
the dialect from Chapter 17 and the runtime APIs from Chapter 13.

## 18.2 The four artifacts plus one

For a stem `<Base>`, generation writes five files into the output
directory:

- `<Base>.ast.h` — one C type per `AST` node: `Union` kinds become a
  forward-declared struct plus a `<Name>_kind_t` enum, `Struct`/`Leaf`
  kinds become structs whose `One` fields use the referenced node's C
  tag (`json_string_t *key`) and whose `Many` fields use the generic
  `<Base>_list_t` ordered list. Forward declarations precede the
  definitions so nodes may reference each other in any order; with no
  `AST` block the root degrades to `void *<Base>_root_t`.
- `<Base>.lexer.h` — a `<Base>_token_kind_t` enum (skip rules marked
  as such) and the `<Base>_token_def_t` table declaration carrying
  each token's normalized pattern, classifier, and flags.
- `<Base>.parser.h` — the `moosedog_info_t <Base>_info` metadata and
  the two entry points below, plus a comment recording the start
  rule, rule/token counts, and ATN mode.
- `<Base>.parser.c` — the grammar builder: every token becomes a
  `GLR_SYMBOL_TERMINAL` (literals via
  `glr_scannerless_set_literal`, regexes via
  `glr_scannerless_set_pattern`, classifier tokens left
  unconfigured), every rule a `GLR_SYMBOL_NONTERMINAL`, every `Alt` a
  production resolved with `glr_grammar_find_symbol_any`, and the
  `StartRule` installed last. Pattern failures report
  `token <Name>: ...` and abort the build; actions are preserved as
  comments alongside each production.
- `Moosedog.h` — copied from `moosedog/include/` unless the output
  directory already maintains one by hand.

With `--emit-makefile` / `--emit-cmakefile` the driver additionally
writes `Makefile.<Base>` / `CMakeLists.<Base>.txt` so a generated
parser builds standalone against an installed libglr.

## 18.3 The generated runtime contract

```c
glr_grammar_t *JSON_build_grammar (char *error, size_t error_size);
glr_parser_t *JSON_create_parser (glr_grammar_t *grammar);
```

`JSON_build_grammar` returns a fully populated grammar or `NULL`
with a diagnostic; the caller owns it. `JSON_create_parser` wraps
`glr_parser_create` and applies the specification's runtime policy:
space trivia when the grammar declares skip rules, and
`glr_parser_set_adaptive_lookahead()` plus
`glr_parser_set_adaptive_lookahead_depth()` exactly when the
`UseATN`-and-`AdaptiveLookahead` conjunction from Chapter 17 holds.
`JSON_info` exposes the same policy as data (`grammar_name`,
`use_atn`, `lookahead_depth`, `max_errors`, `strict_unicode`, ...),
so hosts can inspect a parser without constructing one. The
reference consumer is `moosedog/examples/JSON/JSONParser.c`, which
includes exactly the four headers above and nothing else.

## 18.4 Wiring into the build

`BUILD_MOOSEDOG` (default `ON`) adds three targets to the top-level
build: `moosedog_lib` (the frontend as a static library),
`moosedog` (the driver), and `moosedog_ebnf` (the EBNF plugin as a
shared object). Installation covers the binary, both libraries, the
two public headers, the CMake helper, and the data directories
(`share/moosedog/gpp-prelude`, `lua-prelude`, `confuse-prelude`,
`libsh-prelude`, `templates`, `examples`).

Downstream CMake projects generate parsers with the shipped helper:

```cmake
find_package(moosedog)
moosedog_add_grammar(TARGET mylib GRM lang.grm BASENAME Lang
                     OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen
                     MAKEFILE CMAKEFILE)
target_link_libraries(mylib PRIVATE libglr::libglr)
```

The function runs the just-built `moosedog` at build time and
attaches the generated `<Base>.parser.c` to the target, with
`NO_REWRITE` / `NO_ATN` / `EXTRA_ARGS` pass-throughs.

## 18.5 Proving the emitter in-tree

The build does not trust the emitter blindly. A custom command
regenerates the JSON parser into `build/moosedog/gen-test/` on every
relevant change; `moosedog_gen_json` compiles that output with
`-Wall -Wextra -Werror`, so any emitter regression that produces
non-clean C breaks the build. `moosedog_smoke` then links the
generated parser against `moosedog_lib` and `libglr` and asserts
end to end: valid documents (including nested objects and `null`
members) parse, malformed input is rejected, metadata matches the
spec, and the frontend APIs — spec parsing, all six validation
stages, `Optparse` lookup hits and misses, env expansion, Lua syntax
checks, `stdext` evaluation, the empty-registry plugin miss path,
and EBNF translation — behave. The ANSI-C grammar is covered the
same distance short of parsing: its 82 rules and 98 tokens generate
and its identifier/keyword patterns match through
`glr_scannerless_match` (`iffy` lexes as one identifier, per the
specification's longest-match contract).

## 18.6 Plugins and the EBNF frontend

Alternate syntaxes plug in through `Moosedog-Plugin.h`: a shared
object exporting `moosedog_plugin_entry` returns a descriptor with
an ABI tag, a name, a version, an input suffix, and a `translate`
callback producing `.grm` source text. The driver loads extra
plugins with `--plugin` and dispatches `file.ebnf` accordingly.
`stdplugin/ebnf-frontend/` is both the reference implementation and
a built-in fallback: `rule ::= alternative | alternative ;` becomes
a `Syntactic` block, quoted literals become declared `LIT_n`
terminals in a synthesized `Lexical` block (so references always
resolve), and overlong alternatives or translation failures carry
diagnostics instead of truncated output.

## 18.7 Preludes, templates, and editor support

Each `*-prelude/` directory ships reusable fragments, not build
inputs: fourteen GPP macro files (pattern shorthands, helper
aliases, banner and version helpers, all GPP-safe with no
cpp-only stringification), `lua-prelude/moosedog.lua` (member-list
and scalar helpers for query authors), `confuse-prelude/moosedog.conf`
(default cache/lexer/ATN configuration), and
`libsh-prelude/moosedog.sh` (`moosedog_build`/`moosedog_check`
shell wrappers). `templates/` documents the emitter's output shape
with `@BASE@`-style placeholders for all six generated files.
`addons/` provides `.grm` highlighting for Vim, Neovim, and Sublime
Text.
