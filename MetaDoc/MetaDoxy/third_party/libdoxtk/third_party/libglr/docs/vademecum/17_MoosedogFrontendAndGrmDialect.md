# Chapter 17: The Moosedog Frontend and the `.grm` Dialect

## 17.1 Purpose and scope

Moosedog is the parser-generator frontend for libglr. Where Chapter 13
shows how to assemble a grammar by calling the C API directly, Moosedog
starts one level higher: a declarative `.grm` specification describing
the language, its abstract syntax, its tokens, and its productions is
compiled into a set of C artifacts — `<Base>.ast.h`, `<Base>.lexer.h`,
`<Base>.parser.h`, `<Base>.parser.c` — that build and configure a
libglr grammar programmatically. The frontend itself (`moosedog/`) is a
C11 static library (`moosedog_lib`), a command-line driver (`moosedog`),
and a loadable EBNF plugin, all wired into the top-level CMake build
behind `BUILD_MOOSEDOG` (default `ON`).

The two normative specifications live in `moosedog/examples/`:

- `JSON/JSON.grm` — a small data-exchange grammar with Lua queries;
- `ANSI-C/ANSI-C.grm` — a C11 translation-unit grammar with an
  adaptive-lookahead configuration.

Every construct in this chapter is demonstrated by at least one of
those two files.

## 17.2 The four blocks

A `.grm` file has four top-level blocks, each parsed by a dedicated
module:

| Block | Module | Contents |
| --- | --- | --- |
| `Entrypoint` | `entrypoint/` | language metadata, configuration, CLI options, build pipeline |
| `AST` | `parser/` | abstract-syntax node declarations |
| `Lexical` | `lexer/` | skip rules and token patterns |
| `Syntactic` | `parser/` | productions with actions |

Comments are C-style (`/* ... */` and `//`). String literals use
double quotes with backslash escapes preserved verbatim — important
for regular expressions (see Chapter 19). Values may also be bare
identifiers, numbers, helper calls such as `LanguageType("...)`,
and brace-enclosed lists.

## 17.3 `Entrypoint`

`LanguageInfo` carries `Name`, `Type` (a `LanguageType("...")` helper),
and `Standard` (a `URL("...")` helper). `GrammarInfo` carries
`Authors` (a list), `License` (a `GetLicense("...")` helper), and a
`RevisionHistory` list. The helper heads are validated against a
built-in table (`LanguageType`, `URL`, `GetLicense`, `Allocators`,
`RecoveryMode`, `IntRange`); an unknown helper such as
`Frobnicate("...")` is a validation error, not a warning.

`Disambiguators` is a list of `ImportDisamb("file", "%Hook")` entries
naming `disambstd` hooks (e.g. `%IsCaseSensitive`); every entry must
reference a `%Hook`. `Rewriters` is a list of
`ImportRewriter("....grl")` entries naming `rewritelib` programs.
`Queries` lists `.lua` files shipped next to the grammar; each must
exist on disk and pass a block-balance check (Chapter 19).

`Config` is a flat key/value table. The recognized keys and their
defaults are:

| Key | Default | Meaning |
| --- | --- | --- |
| `MainLexer` | `"Scannerless"` | primary token engine (`PCRE2`, `Scannerless`, `RE2`, `libchomsky3`, `POSIX`) |
| `FallbackLexer` | `"Scannerless"` | fallback token engine |
| `UseATN` | false | master switch for adaptive prediction |
| `UseIncremental` | false | editor-oriented incremental parsing |
| `UseCache` | false | parse-table caching |
| `CacheDirectory` | `${TMPDIR}/MoosedogCache` | cache location (`/tmp` fallback) |
| `MainAllocator` / `FallbackAllocator` | `malloc` | allocation backends |
| `EmitASTListener` / `EmitASTVisitor` | true | listener/visitor hook emission |
| `CacheLexer` / `CacheParser` | false | lexer/parser result caching |
| `ErrorRecovery` | false | continue past syntax errors |
| `MaxErrors` | 10 | recovery budget (0..10000) |
| `RecoveryStrategy` | `PanicMode` | a `RecoveryMode("...")` helper |
| `OutputEncoding` | `"UTF-8"` | generated text encoding |
| `StrictUnicode` | true | reject ill-formed UTF-8 |
| `DebugLevel` | 0 | generator diagnostics |
| `ASTAnnotations` | false | source spans on AST nodes |

The `ATN` block holds `AdaptiveLookahead` (default: the value of
`UseATN`), `MaxDepth` (default 8, range 1..64),
`PreserveAmbiguity` (default true: unresolved alternatives stay in
the forest), and `EmitStatistics` (default false). Prediction is
enabled only when **both** `Config.UseATN` and
`ATN.AdaptiveLookahead` agree — `--no-atn` is the master disable.

`Optparse` declares one `Option("--flag") { ... }` per CLI flag with
`Short`, `Define` (the GPP symbol the pipeline guards test), `Type`
(`Bool`, `String`, `Int`, `Path`), `Default`, `Range`
(`IntRange(lo, hi)`), `Description`, and an optional `Bind`
(`Config.X` or `ATN.X`, with `BindInvert` for `--no-*` flags).
The remainder of `Entrypoint` is the pipeline: bare calls such as
`Preprocess()`, `Rewrite()`, `CompileAST()`, `CompileLexer()`,
`CompileParser()`, `WriteFiles()`, `RunUnitTests()`, optionally
guarded by `@ifndef SYM ... @endif` conditionals that the parser
records unconditionally.

## 17.4 `AST`, `Lexical`, `Syntactic`

`AST` declares one `Node(Name) { ... }` per abstract-syntax type with
a C `Tag`, a `Kind` (`Union` with a `Variants` list, `Struct`, or
`Leaf`), and `Fields` of `Field("name") { Type = ...; Arity = ... }`
where arity is `One` or `Many`. The first node is the root type.

`Lexical` declares `Skip(Name) { Pattern = ... }` rules for trivia
and `Token(Name) { ... }` terminals with a `Pattern`, a
`Literal = Yes|No` marker (literal spellings are raw text, not
regexes), or a `Classifier` for contextual tokens such as
`TYPEDEF_NAME` (supplied by a scope-aware classifier, never by a
regex — see Chapter 19).

`Syntactic` sets `StartRule` and declares
`Rule(Name) -> ReturnType { Alt { Seq = "..."; Action = "..." } }`.
`Seq` is a whitespace-separated symbol list; each name must resolve
to a rule or a token. Actions come in two families, both declarative
strings rather than C code: the JSON family (`MakeVariant`,
`MakeLeaf`, `MakeNode`, `MakeList`, `AppendList`, `EmptyList`,
`return $n;`) and the ANSI-C family (`Token($n)`,
`Tree(Family, form, children...)`, `Sequence`, `Append`, `Unit`,
`return $n;`). Punctuation disappears; recursive lists flatten.

## 17.5 Preprocessing with GPP

`.grm` files are GPP sources. Macros such as `ANSI_C_GRAMMAR_NAME`
are defined in a sibling `Support.gpp` loaded with
`gpp -C --include Support.gpp`. The `-C` flag matters: modes set
inside the include are restored on return, keeping quotes and regex
escapes intact. When the driver is invoked without explicit defines,
it automatically preprocesses with a sibling `Support.gpp` if one
sits next to the grammar; `-D SYM[=VAL]` and `--alt-preprocessor`
override this. Every `-D` flag becomes a single shell-quoted word,
so values containing spaces or metacharacters survive intact. If GPP
is unavailable, or a macro is never expanded, the frontend falls back
to the `Support.gpp` defaults (`UseATN` on, depth 8, debug off)
rather than misreading the macro name as a value.

## 17.6 The `moosedog` command line

```sh
moosedog --check moosedog/examples/JSON/JSON.grm
moosedog --output-dir out --basename JSON moosedog/examples/JSON/JSON.grm
```

`--check` parses and validates without emitting. Generation accepts
`--output-dir/-o`, `--basename` (default: file stem, then grammar
name), `--emit-makefile`, `--emit-cmakefile`, `--debug/-d`,
`--no-rewrite/-R`, `--max-errors/-e`, `--no-cache/-X`,
`--cache-dir/-T`, `--strict-unicode` / `--no-strict-unicode`,
`--no-atn`, `--lookahead-depth` (1..64), `--define/-D`,
`--alt-preprocessor/-H`, `--plugin` (extra frontend `.so`),
`--moosedog-include`, `--verbose/-v`, `--version`, and `--help`.
CLI overrides are re-validated against the entrypoint limits, so
`--lookahead-depth 99` fails instead of generating a broken parser.
Exit codes are 0 (success), 1 (validation/generation error), 2
(usage error); anything else signals a crash, which the fuzz test
treats as a failure (Chapter 19).

## 17.7 Validation order

`--check` and every generation run the same six stages in order:
specification cross-references (`StartRule` exists, `Seq` names and
AST return types resolve, no empty alternatives), lexical definitions
(names unique, `Pattern` or `Classifier` present, regex sanity),
entrypoint (language name, known lexers, helper shapes, depth and
error budgets), disambiguator imports, rewriter imports, and Lua
queries (existence plus syntax). The first failure aborts with a
single diagnostic naming the offending construct.
