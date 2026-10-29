# DSLtk — A Manual

This directory contains the thirty-six-chapter manual for **DSLtk**, the
Domain-Specific Language construction toolkit for C++20 and D. The two
implementations are distributed as one C++ header, `DSLtk.hpp`, and one D
module, `DSLtk.d`. The manual explains the established toolkit, the D port,
and the shared language facilities added to both implementations.

The manual is organised into twenty parts that progress from foundational
concepts through to the most advanced subsystems. Readers new to the library
should begin with Chapter 1 and read sequentially; experienced readers may use
the table of contents below to jump directly to the feature they need.

## Reading Paths and Development Progression

- **C++ foundations:** Chapters 1–29 introduce feature composition and the
  established C++ toolkit, including the embedded `magic_enum` API.
- **D foundations:** Chapter 31 maps every established feature family to D,
  with mixins, alias callables, CTFE, native reflection, and build instructions.
- **Shared extensions:** Chapter 30 is an overview; Chapters 32–36 provide
  focused tutorials, API contracts and examples for both languages.
- **Full development record:** [DSLtk Development Progression](progression.md)
  records the milestones from the C++ toolkit through the D translation,
  equality saturation, user backends, visitors, binary streams, preprocessing,
  optional Lua, explicit CFGs, packaging and verification.

## Table of Contents

### Part I — Foundations

- [Chapter 1 — Introduction and Design Philosophy](ch01-introduction.md)
- [Chapter 2 — Getting Started: Installation, Build, and First Program](ch02-getting-started.md)
- [Chapter 3 — The CRTP Foundation: `dsl::DSL<Derived, Features...>`](ch03-crtp-foundation.md)
- [Chapter 4 — `FixedString`: Compile-Time Strings as Template Parameters](ch04-fixed-string.md)
- [Chapter 5 — Feature Tags, Mixins, and Utility Concepts](ch05-features-and-concepts.md)

### Part II — Data Flow

- [Chapter 6 — The Pipeline Feature](ch06-pipeline.md)
- [Chapter 7 — The Operators Feature: Predicate Composition](ch07-operators.md)

### Part III — Pattern Matching

- [Chapter 8 — Pattern Matching with `match`, `when`, and `otherwise`](ch08-pattern-matching.md)
- [Chapter 9 — The `pattern<>` Compile-Time Regex Engine](ch09-pattern-regex.md)

### Part IV — Abstract Syntax Trees

- [Chapter 10 — The `ASTNode` Value Type](ch10-ast-node.md)
- [Chapter 11 — Building Trees with `leaf<>` and `node<>`](ch11-leaf-and-node.md)
- [Chapter 12 — Traversing and Dumping ASTs](ch12-ast-traversal.md)

### Part V — Rewriting

- [Chapter 13 — The Rewrite Feature: Rules and Predicates](ch13-rewrite-rules.md)
- [Chapter 14 — Rewrite Sets and Fixpoint Optimization](ch14-rewrite-sets.md)

### Part VI — Expression Templates

- [Chapter 15 — Expression Templates: Lazy Trees](ch15-expr-templates.md)
- [Chapter 16 — `BinExpr`, `UnaryExpr`, and Fused Evaluation](ch16-binexpr-evaluation.md)

### Part VII — Customization

- [Chapter 17 — Custom Literals: `lit`, `literal_set`, `parse_literal`](ch17-custom-literals.md)

### Part VIII — Caching and Laziness

- [Chapter 18 — Memoization: `memoize` and `MemoizedCallable`](ch18-memoization.md)
- [Chapter 19 — `Lazy<T>`: Deferred Single-Shot Values](ch19-lazy.md)

### Part IX — Functional Types

- [Chapter 20 — The `Maybe<T>` Type and Monadic Operations](ch20-maybe.md)
- [Chapter 21 — Monadic Helpers for `std::optional`](ch21-optional-helpers.md)
- [Chapter 22 — The `Result<T,E>` Type and Error Flow](ch22-result.md)

### Part X — Parser Combinators

- [Chapter 23 — Parser Combinators: `ParsecInput`, `ExpectedResult`, `Parser`](ch23-parser-core.md)
- [Chapter 24 — Primitive Parsers and Basic Combinators](ch24-primitive-parsers.md)
- [Chapter 25 — Parser Diagnostics, Semantic Actions, and `run_parser`](ch25-parser-diagnostics.md)

### Part XI — Task Pipelines

- [Chapter 26 — Task Pipelines: `Task`, `TaskChain`, `TaskState`](ch26-task-pipeline.md)

### Part XII — Parsing Expression Grammars

- [Chapter 27 — The PEG Definition Grammar: Rules and Channels](ch27-peg-definition.md)
- [Chapter 28 — PEG Combinators, `PEGMatch`, and the `PEGMatcher`](ch28-peg-combinators-matcher.md)

### Part XIII — Enum Reflection

- [Chapter 29 — Enum Reflection, Flags, Dispatch, and Containers](ch29-enum-reflection.md)

### Part XIV — Language Facilities (C++ and D)

- [Chapter 30 — Equality Saturation and Language Facilities](ch30-language-facilities.md)

### Part XV — The D Toolkit

- [Chapter 31 — The D Toolkit: Templates, Mixins and CTFE](ch31-d-toolkit.md)

### Part XVI — Equality-Based Rewriting

- [Chapter 32 — S-expressions, E-graphs and Equality Saturation](ch32-egraphs-and-sexpressions.md)

### Part XVII — Data Boundaries and Binary Matching

- [Chapter 33 — User Codecs, Abstract I/O and Static Binary Streams](ch33-codecs-io-and-binary-streams.md)

### Part XVIII — Visitor Generation

- [Chapter 34 — Deriving Visitors from AST Schemas](ch34-ast-derived-visitors.md)

### Part XIX — Preprocessing and Embedded Scripting

- [Chapter 35 — Built-in Preprocessing and Optional Lua](ch35-preprocessor-and-lua.md)

### Part XX — Context-Free Language Definition

- [Chapter 36 — Explicit Context-Free Grammars and Earley Parsing](ch36-context-free-grammars.md)

## Building the Manual

The manual is GitHub-Flavored Markdown, one file per chapter. It is directly
readable in a text editor or repository browser. To export the numbered
chapters with [Pandoc](https://pandoc.org/), run from this directory:

```sh
pandoc ch[0-9][0-9]-*.md --standalone --toc -o DSLtk-manual.html
pandoc ch[0-9][0-9]-*.md --standalone --toc --pdf-engine=xelatex -o DSLtk-manual.pdf
pandoc ch[0-9][0-9]-*.md --standalone -t latex -o DSLtk-manual.tex
pandoc ch[0-9][0-9]-*.md --standalone -t docbook -o DSLtk-manual.xml
```

The two-digit filenames preserve chapter ordering. PDF export requires the
selected TeX engine. The progression record is a separate reading aid; append
`progression.md` to an export command to include it.

## Conventions Used in This Manual

Code listings are fenced and marked `cpp`, `d`, `lisp`, `ebnf`, or `sh` as
appropriate. C++ uses `dsl::` names; D uses `import DSLtk`, native `!` template
arguments and CTFE. Fragments assume the corresponding header/import and
surrounding example variables; complete programs show their entry point.
Throughout the text, the names of types and functions are set in
backticks (for example, `dsl::DSL`, `dsl::leaf`), and chapter cross-references
appear as “Chapter 6, *The Pipeline Feature*”.

The numbered chapters describe implemented behavior and supported syntax.
The separate progression record captures the development milestones and
recorded verification. Protocol examples explicitly identify operations and
syntax supplied by user backends.
