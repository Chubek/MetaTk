# Chapter 1 — Introduction to MetaVect

MetaVect is a functional vector-graphics language under development. Its
design target is a Turing-complete functional language with System-F-style
polymorphism, targeting multiple rendering formats (PDF, SVG, Cairo, Skia,
TikZ, and others) through pluggable backends.

## What this manual covers

This manual documents the language as implemented today, the compiler
pipeline, the runtime and standard library designs, and the contributor
workflow. It is organized in 32 chapters across six parts:

- Part I (chapters 1–4): orientation — what MetaVect is, how to build it,
  your first program, and a tour of the repository.
- Part II (chapters 5–13): the language — lexical structure, modules,
  bindings, types, and expressions.
- Part III (chapters 14–21): the implemented frontend — source locations,
  tokens, lexer, parser, name resolution, diagnostics, and the driver.
- Part IV (chapters 22–27): the downstream pipeline — MVIR, rewrites,
  passes, backend selection, and emission.
- Part V (chapters 28–30): runtime, standard library, and public APIs.
- Part VI (chapters 31–32): editor tooling and contributing.

## Reading status labels honestly

MetaVect is a scaffold under active implementation. Every chapter carries a
status note. As a rule:

- **Implemented** means code exists, is exercised by `ctest`, and behaves as
  described. Today this covers source mapping, UTF-8 lexing, parsing into an
  AST, name resolution, and `mvect --check-syntax`.
- **Scaffold** means files define contracts, extension points, or interface
  sketches whose behavior is not yet implemented. Do not assume a feature
  works merely because its file exists.

The guiding principle: syntax success is not compilation success. A program
that passes `--check-syntax` has valid shape; nothing has been type-checked,
evaluated, or rendered.
