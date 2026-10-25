# Chapter 4 — Tour of the Repository

**Status:** reference. Status notes per directory are accurate as of this
writing; consult each subsystem's README for detail.

| Directory | Contents | Status |
| --- | --- | --- |
| `frontend/` | Source input, tokens, lexer, parser, AST, symbol table, semantic checks | Implemented (see `frontend/README.md`) |
| `driver/` | `mvect` CLI and `compile()` orchestration | Syntax/semantic check implemented; full compile stubbed |
| `mvir/` | Intermediate representation, AST lowering visitor, rewrite engine, Lua specs | Scaffold + declarative spec catalog |
| `mvir/rewrite-specs/` | `*.lua` declarative rewrite rules | Schema implemented; engine pending |
| `compiler/` | Pass orchestration, memoization, MVIR terms, `.csel` reading, emission | Scaffold contracts |
| `internals/` | Runtime, values, memory, GC, modules, diagnostics, concurrency | Scaffold contracts |
| `stdbe/` | Output backends (Cairo, Skia, TikZ, …) plus `.csel` selector rules | Scaffold contracts |
| `stdlib/` | 30 module interfaces (`.mvct`) with native (`.cpp`) entry points | Interface scaffolds |
| `include/` | Public backend/module/renderer/shader APIs (C, C++, D) | Scaffold contracts |
| `extern/` | Vim + Sublime grammars, LaTeX package, TeX snippets | Implemented (see `extern/README.md`) |
| `tests/` | D contract tests, CLI tests, editor and TeX checks | Implemented |
| `examples/` | `first.mvct` sample program | Implemented |
| `manual/` | This manual | You are here |
| `cmake/` | `build-d.cmake` helper, source-tree checks | Implemented |

Top-level files: `MetaVect.{h,hpp,d,c}` carry the single version/status
contract (`0.1.0`); `CMakeLists.txt` wires the C library, the D frontend,
and all tests.

## Pipeline boundaries

Source input → lexer → parser/AST → semantic checks → MVIR → verified
rewrites → backend emitter. Each boundary is a module with an explicit
contract. Frontend parsing stays independent of output backends; anything
backend-specific lives under `stdbe/` or `include/`.
