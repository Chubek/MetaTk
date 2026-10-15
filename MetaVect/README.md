# MetaVect — a Multi-Output Graphics Language

MetaVect is a functional vector-graphics language under development, targeting multiple rendering formats. The intended language is Turing-complete and uses System-F-style polymorphic typing. The repository separates parsing, semantic analysis, MVIR optimization, runtime services, and backend emission.

> **Status: first implementation milestone — syntax frontend.** Source mapping, UTF-8 lexing, a functional-expression parser/AST, diagnostics, and `mvect --check-syntax` are implemented and tested. Name resolution, type checking (including System F), MVIR, rewriting, evaluation, and rendering remain scaffolds. Syntax success is not compilation success.

## Repository map
- `frontend/` — source input, tokens, lexer, parser, AST, symbol table, semantic checks.
- `mvir/` — intermediate representation and declarative rewrite specifications.
- `compiler/` — optimization orchestration and backend emission.
- `internals/` — runtime, values, memory, modules, diagnostics, serialization, and concurrency primitives.
- `stdbe/` — output backends and selector rules.
- `stdlib/` — standard-library module interfaces and native implementation entry points.
- `include/` — public backend, module, renderer, and shader APIs.
- `extern/` — editor syntax (Vim, Sublime) and TeX/LaTeX integration; see `extern/README.md`.

## Build
Use CMake 3.20+, a C/C++ compiler, and LDC, DMD, or GDC for the frontend:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The build discovers `ldc2`, `dmd`, or `gdc` and builds `build/mvect` and the frontend tests. Select a compiler with `-DMETAVECT_D_COMPILER=/path/to/compiler`. If none is installed, CMake reports that the frontend is skipped and builds the C API alone. `-DMETAVECT_BUILD_FRONTEND=OFF` explicitly selects a C-only build. No external libraries are needed for this milestone.

Lua rewrite specs can be syntax-checked with `luac -p mvir/rewrite-specs/*.lua`; they are not executed by the syntax frontend.

## First milestone usage

```sh
build/mvect --check-syntax examples/first.mvct
build/mvect --version
```

The syntax checker accepts module/import declarations, exported or local bindings, type annotations, literals, calls, lambdas, local `let`, conditionals, and arithmetic/comparison/Boolean expressions. It accepts the existing `stdlib/**/*.mvct` interface scaffolds without loading imports or executing calls. See [`frontend/README.md`](frontend/README.md) for the exact grammar, APIs, and limitations.

Successful syntax checks are silent. Exit codes: **0** success/help/version; **1** syntax or input-read failure in syntax-check mode; **2** invalid command-line usage; **3** compilation failed without `--check-syntax`, either because of a syntax or input error or because the remaining pipeline is not implemented. A normal compilation request does not produce output or pretend to render graphics.

Each D module is defined by exactly one file. A source-tree test fails the build if a module ever gains both a `.d` and a `.di`, because D ignores the `.di` once a `.d` exists and emits no code for interface-only modules.

## Compiler pipeline
1. Read source and preserve source locations — implemented.
2. Tokenize and parse into an AST — implemented for the first-milestone subset.
3. Run name resolution and type/semantic checks — planned next.
4. Lower the AST into MVIR — scaffold.
5. Apply validated, ordered rewrite passes — scaffold.
6. Select a backend and emit output — scaffold.
7. Report diagnostics with source spans and actionable messages.

## Design principles
- Deterministic builds and transformations.
- Explicit errors instead of silent recovery for invalid programs.
- Backend independence in the front end and MVIR.
- Conservative optimization by default.
- Stable public interfaces and documented extension points.

See `AGENTS.md` for contributor and agent instructions.
