# 26. Text and structured IR interoperability

Textual and native construction converge on the same IR. `Context::parse` and
`metail_parse` explicitly import text. `Context::emit(Module)` and
`metail_module_emit` explicitly export text. Neither step is required merely to
optimize or analyze a native-built module.

```cpp
auto imported = context.parse("module demo; fn f(x) { return x + 0; }");
auto result = context.optimize(imported, "AddZeroRight");
auto canonical_text = context.emit(result.module);
auto reparsed = context.parse(canonical_text);
```

Emission preserves the represented program, not original formatting or comments.
Parentheses, spacing, and operator spellings are canonicalized. Generated text
can be consumed by the CLI and reimported into either native API.

Use structured inspection to test transformations precisely: inspect statement
kinds and expression tags instead of searching formatted output. Use text snapshots
when readable diffs or external tool interchange are the goal. Both are useful,
but they verify different properties.

Validation rules are shared across parser-built and native-built modules. In
particular, native call names cannot masquerade as reserved operator names, and
boolean-literal names cannot be variable expressions. This avoids producing text
that would reparse into a different expression kind.

The `parser-free-ir` test replaces the textual program parser with an error and
still exercises structured construction, optimization, analysis, and emission.
Rewrite-pattern parsing remains available because `.optk` rules use their own
expression-pattern inputs.

[Contents](README.md) · [Next: Pipelines](27-pipelines.md)
