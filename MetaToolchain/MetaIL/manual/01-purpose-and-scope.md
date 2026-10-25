# 1. Purpose and scope

MetaIL is a small intermediate language for studying, embedding, and extending
scalar compiler analyses and transformations. Programs describe functions, local
integer values, expression trees, and control flow. Optikern `.optk` files register
executable analyses or transformations over that representation.

You can work in three ways:

| Interface | Typical input | Typical consumer |
|---|---|---|
| Textual DSL | `.mi` files or strings | CLI, fixtures, generated source |
| C structured API | `metail_module` descriptors | C compilers and tools |
| C++ native DSL | `Module`, `Function`, `Expr` values | C++ applications |

The structured APIs create actual IR tables for the runtime. They do not implement
construction by printing source and reparsing it. Text can be parsed into the same
inspectable structure when interoperability is useful.

All current program values are signed 64-bit integers. This is not a complete
backend: native code generation, JIT compilation, SSA construction, memory SSA,
alias analysis, and complete type systems are not implemented. A call expression
records an operation with possible effects; it does not bind a native function.

A useful first goal is to build `return (x + 0) * 8`, simplify it, inspect the
resulting expression, and emit equivalent MetaIL text. Chapters 5 and 6 show both
textual and C++ versions of that workflow.

[Contents](README.md) · [Next: Architecture](02-architecture.md)
