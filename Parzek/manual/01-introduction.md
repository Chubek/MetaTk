# 01 Introduction

Parzek is a PEG-style parser generator for C++20 projects built around `DSLUtils.hpp`.
It compiles `.pzg` grammars into deterministic C++ source files:

- `Name-Parzek.hpp`
- `Name-Parzek.cpp`

Parzek has two usage modes:

- **CLI** for grammar compilation in build pipelines
- **Library API** for embedding grammar compilation in tooling

## Design goals

- Keep parser generation transparent and readable.
- Use `DSLUtils.hpp` combinators in generated code.
- Keep helper combinators local to Parzek/generated code.
- Preserve grammar intent with simple, deterministic emission.
- Report structured diagnostics with file/line/column context.

## Parsing model

Parzek follows a staged architecture:

1. Preprocess source grammar (`@define`, directives, substitutions)
2. Tokenize grammar text
3. Parse into grammar AST
4. Validate and normalize AST
5. Emit generated parser C++ files
6. Optionally emit visitor header

This separation keeps failures understandable and makes the generator maintainable.

## Why PEG here?

PEG gives ordered choice and explicit operator semantics that map cleanly to parser combinators (`|`, `&`, `*`, `optional`, `try_parse`, labeling). It also keeps grammar authoring concise while preserving deterministic parse behavior.

## What Parzek is not

- Not a runtime scripting engine
- Not a replacement for `DSLUtils.hpp`
- Not a custom parser runtime outside combinator foundations

Parzek intentionally keeps scope focused: compile grammars to parser code with practical diagnostics and predictable output.
