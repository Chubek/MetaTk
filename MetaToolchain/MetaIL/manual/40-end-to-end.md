# 40. End-to-end workflow and boundaries

A practical native workflow is: construct or import IR, validate, inspect it,
select kernels, transform, analyze the result, and emit when interchange is needed.

```cpp
metail::Context context;
metail::Module module("application");
auto x = metail::var("x");
module.function("scale", {"x"}).ret(x * 8);
context.check(module);

metail::KernelRegistry registry(context);
metail::KernelQuery query;
query.keywords = "MultiplyByPowerOfTwoRight03";
query.tags = {"rewrite", "strength-reduction"};
auto selected = registry.search(query);
auto result = context.optimize(module, registry.passes(selected));
auto report = context.analyze(result.module, "ControlFlowGraph,ExpressionNodeCount");
auto output = context.emit(result.module);
```

The selected result is a structured module; `output` is optional presentation.
You can inspect its expression children directly, edit a copy, or transfer it to
another Context. You can also obtain the selected `.optk` files with
`registry.request(query)` for display or packaging.

Choose explicit pass lists when reproducible order is important, and keep effect/
trap examples in your own regression suite. Validate after structural edits and
test the installed package independently of the source tree.

MetaIL's current boundary remains scalar signed-64 IR. It offers working parsing,
native construction, analyses, rewrites, procedural cleanup, and discovery. It
does not yet provide SSA, memory operations/alias analysis, JIT execution, native
code generation, or a complete type system. Treat those as future extensions,
each requiring explicit semantics and tests across all three interfaces.

[Contents](README.md) · [Back to the first chapter](01-purpose-and-scope.md)
