# 10. Pipelines and composition

`x |> f` passes a value to a function. It is particularly natural with data-last APIs: first configure an operation, then pipe the collection or string through it. Use explicit pipelines for long chains instead of relying on concatenative juxtaposition.

```metaml
inc x := x + 1
double x := x * 2
forward := inc >> double
backward := double << inc
Test.equal "composition" (forward 3, backward 3) (8, 8)
Test.equal "pipeline" ([1, 2, 3] |> List.map inc |> List.sum) 9
```

`f >> g` means first `f`, then `g`; `g << f` means the same computation. The Unicode composition operator `∘` is a backward-composition spelling. A composition remains a first-class function, so it can be stored, passed to `List.map`, and applied later.

Bare juxtaposition has a type-directed interpretation: ordinary function application where possible, composition in supported function-typed situations. The checker records its decision in the AST, and the evaluator follows that annotation. This is not untyped stack concatenation, and it should not be used to hide an otherwise incompatible application.

Function types are the best guide: composing `A -> B` with `B -> C` produces `A -> C`. Partial application lets a multiargument function fit a unary pipeline. For example, `List.map inc` accepts a list; `String.split ","` accepts the input string.

The interpreter uses stack bookkeeping when evaluating composition, but `$n` still addresses formal parameters. Do not design an API that assumes unrelated top-level evaluations share hidden operands. `#stack` is primarily an interactive observation tool, not a replacement for explicit function arguments.

**Style:** named steps plus `|>` usually give clearer diagnostics than a dense mixture of tacit references, juxtaposition, and infix composition.

<!-- nav:begin -->
[Contents](README.md) · [Previous](09-sections.md) · [Next](11-type-inference.md)
<!-- nav:end -->
