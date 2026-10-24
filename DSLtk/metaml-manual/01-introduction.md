# 1. Introduction and scope

MetaML is an embeddable ML-family language implemented in C++ and D. It combines named functions, numbered tacit parameters, typed composition, algebraic data, and ML-style modules. Both implementations use DSLtk for parsing infrastructure, source locations, diagnostics, and the reusable REPL shell. C++ applications include `MetaML.hpp`; D applications import `MetaML`. The C adapter described in chapter 43 uses the C++ interpreter through an opaque ABI.

This manual describes the implementation in this repository, rather than treating every exploratory design note as a shipped feature. In particular, ordinary evaluation is eager, function bodies are closures, and module bodies initialize lazily. C++20 is required by the current DSLtk dependency. Network servers, fibers, channels, and threading modules have reserved files but are not implemented libraries.

There are two library layers. The portable `metalib/*/mod.ml` files provide the prelude, numeric functions, strings, lists, Option, Result, and IO wrappers. The host modules in `metalib/cpp` and `metalib/d` add parsers, structured data, buffers, tables, graphs, and tooling. Installing a host module and permitting its primitives are separate operations.

```metaml
square x := x * x
answer := [1, 2, 3] |> List.map square |> List.sum
Test.equal "first program" answer 14
```

Examples marked `metaml` are complete snippets intended to run with the standard library and portable host bundle available. Their assertions check results without requiring REPL-specific output. Shell commands assume the repository's `DSLtk` directory is the current directory. Follow chapters 2–23 for the language, 24–40 for libraries, and 41–45 for embedding and reference material.

**Sources:** [`MetaML.hpp`](../MetaML.hpp), [`MetaML.d`](../MetaML.d), [`metalib`](../metalib).

<!-- nav:begin -->
[Contents](README.md) · [Next](02-building.md)
<!-- nav:end -->
