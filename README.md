# MetaTk

**MetaTk** is a C++ metaprogramming toolkit focused on language tooling.
It currently ships two core components:

- `DSLUtils.hpp`: a parser-combinator and parsing utility library.
- `Parzek`: a PEG-style parser generator built on top of `DSLUtils.hpp`.

MetaTk is designed for teams that want to define custom grammars, generate readable C++ parsers, and keep parsing logic composable and testable.

## What MetaTk Includes

### 1) DSLUtils.hpp
`DSLUtils.hpp` provides reusable parser combinator primitives and parsing utilities, including:

- parser construction helpers
- sequencing, choice, repetition, and optional composition
- parse input/state abstractions
- diagnostics-oriented helpers for clearer parse failures

Use it directly when you want full control over parser construction in C++.

### 2) Parzek
`Parzek` is MetaTk’s grammar-to-C++ parser generator.

With a `.pzg` grammar, Parzek emits deterministic parser sources:

- `Name-Parzek.hpp`
- `Name-Parzek.cpp`

Parzek supports:

- CLI workflow (compile grammar files)
- library workflow (compile from files or in-memory strings)
- optional visitor header generation
- PEG operators, channels, and predicate-oriented grammar features

## Typical Workflow

1. Write a grammar file, for example `Foo.pzg`.
2. Compile it with Parzek.
3. Build the generated C++ parser in your project.
4. Parse your domain-specific input using the generated parser.

Example:

```bash
parzek compile Foo.pzg
parzek compile Foo.pzg --create-visitor=FooVisitor.hpp
```

## Why MetaTk

- **Composability**: low-level parser combinators + high-level generator.
- **Determinism**: generated outputs are stable and readable.
- **C++-native**: integrates directly with modern C++ projects.
- **Extensibility**: suitable for DSL compilers, config languages, and code tooling.

## Future Plans: MetaTk Roadmap

To evolve MetaTk into a metaprogramming powerhouse, the next five strategic plans are:

1. **Incremental & IDE-Aware Parsing**  
   Add incremental parse support, source maps, and editor diagnostics APIs for fast LSP integrations.

2. **Typed AST Generation Layer**  
   Extend Parzek codegen to optionally emit strongly-typed AST node classes, builders, and traversal utilities.

3. **Semantic Pass Framework**  
   Introduce a reusable pass pipeline (name resolution, validation, rewriting) for building full DSL compilers.

4. **Multi-Target Backend Generation**  
   Expand generators beyond C++ parsers to include JSON IR, C bindings, and optional Rust-compatible outputs.

5. **Macro/Meta Expansion Engine**  
   Build a robust compile-time macro and transformation engine with traceable expansion diagnostics and debug views.

## Project Layout (Current)

- `DSLUtils.hpp` — core parser utility header
- `Parzek.hpp` / `Parzek.cpp` — parser generator library
- `main.cpp` — CLI entry for `parzek`
- `parzek-manual/` — detailed chapters and documentation
- `parzek-examples/` — feature-focused grammar examples
- `tests/` — smoke and feature tests

## Contributing

Contributions are welcome in these areas:

- grammar language ergonomics
- diagnostics quality and error recovery
- generated code quality/performance
- documentation and examples
- testing and fuzzing coverage

If you are exploring parser generators, DSL compilers, or language tooling in C++, MetaTk is built to be a practical foundation.
