# Parzek

Parzek is a PEG-style parser generator for C++20 projects built on top of `DSLUtils.hpp`.
It compiles `.pzg` grammars into deterministic parser sources:

- `Name-Parzek.hpp`
- `Name-Parzek.cpp`

## Features

- Library API and CLI executable (`parzek`)
- Grammar compile from file or in-memory string
- Optional visitor header generation
- Preprocessor support (`@define`, substitutions, directives)
- PEG operators (`|`, `*`, `+`, `?`, grouping)
- Custom adjacency operator (`~`)
- Channel-aware behavior with built-in `@IGNORE`
- Structured diagnostics with source location

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## CLI usage

```bash
parzek compile Foo.pzg
parzek compile Foo.pzg --create-visitor=FooVisitor.hpp
```

Options:

- `--output-dir=DIR`
- `--output-base=NAME`
- `--stdin`
- `--help`

## Library API

Include `Parzek.hpp` and call:

- `parzek::compile_grammar_string(...)`
- `parzek::compile_grammar_file(...)`

Both return `CompileResult`, which includes:

- `success`
- output paths
- optional visitor path
- diagnostics list (`message`, `file`, `line`, `column`, severity)

## Grammar essentials

Required directive:

```pzg
@parser:name(MyParser)
```

Rule naming:

- lexical rules: `ALL_CAPS`
- syntactic rules: `kebab-case`

Example:

```pzg
@parser:name(Calc)
WS: [ \t\r\n]+ -> @IGNORE;
NUMBER: [0-9]+ when $0 != '0';
expr: NUMBER (("+" | "-") ~ NUMBER)*;
```

## Preprocessor summary

Parzek preprocesses grammar source before AST parsing, including:

- macro definitions (`@define`)
- parameter replacement (`#n`, `!#n`, `#n!`, `#@`, `#!`, `##`, `#?`, `#*`)
- hide-set style recursion control
- directive family (`@system`, `@exec`, `@eval`, `@printf`, `@sprintf`, `@foreach`)

## Channels and adjacency

- `@IGNORE` content is skippable in ordinary flow
- `~` forbids ignored-material gap between adjacent terms
- `CHAN` is available in `when` guards

## Generated code relationship to DSLUtils

Generated parsers are built on `DSLUtils.hpp` combinator primitives and include local helper sugar (`many1`, `between`, `sep_by`, `sep_by1`) only in generated/Parzek-owned files.

## Manual and examples

- Detailed chapters: `parser-manual/`
- Feature examples: `parser-examples/`
