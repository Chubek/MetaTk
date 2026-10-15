# MetaVect — a Multi-Output Graphics Language

MetaVect is a scaffold for a functional vector-graphics language targeting multiple rendering formats. The intended language is Turing-complete and uses System-F-style polymorphic typing. The repository separates parsing, semantic analysis, MVIR optimization, runtime services, and backend emission.

> **Status:** early scaffold. Most components define contracts and extension points; do not assume the full compiler or renderer is production-ready.

## Repository map
- `frontend/` — source input, tokens, lexer, parser, AST, symbol table, semantic checks.
- `mvir/` — intermediate representation and declarative rewrite specifications.
- `compiler/` — optimization orchestration and backend emission.
- `internals/` — runtime, values, memory, modules, diagnostics, serialization, and concurrency primitives.
- `stdbe/` — output backends and selector rules.
- `stdlib/` — standard-library module interfaces and native implementation entry points.
- `include/` — public backend, module, renderer, and shader APIs.
- `extern/` — editor syntax and TeX/LaTeX integration.

## Build
A CMake project is provided as a starting point. Use a recent CMake and a C/C++ compiler:

```sh
cmake -S . -B build
cmake --build build
```

The current scaffold may configure or compile only the components that have been implemented for your platform. D modules require a D compiler such as DMD or LDC; Lua rewrite specs can be syntax-checked with `luac -p mvir/rewrite-specs/*.lua`.

## Compiler pipeline
1. Read source and preserve source locations.
2. Tokenize and parse into an AST.
3. Run name resolution and type/semantic checks.
4. Lower the AST into MVIR.
5. Apply validated, ordered rewrite passes.
6. Select a backend and emit output.
7. Report diagnostics with source spans and actionable messages.

## Design principles
- Deterministic builds and transformations.
- Explicit errors instead of silent recovery for invalid programs.
- Backend independence in the front end and MVIR.
- Conservative optimization by default.
- Stable public interfaces and documented extension points.

See `AGENTS.md` for contributor and agent instructions.
