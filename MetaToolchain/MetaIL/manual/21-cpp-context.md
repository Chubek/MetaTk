# 21. C++ Context operations

`metail::Context` is a move-only RAII owner of the native runtime. Construct it
with no arguments for discovery or with a runtime root string. Destruction releases
the Lua state. Operations on a moved-from context throw `metail::Error`.

Text overloads remain available alongside structured overloads:

| Operation | Text input | Structured input |
|---|---|---|
| `check` | Source string, optional diagnostic name | `const Module &` |
| `emit` | Source string | `const Module &` |
| `analyze` | Source string and pass filter | Module and pass filter |
| `optimize` | Returns `OptimizationResult` | Returns `ModuleOptimizationResult` |

Text optimization returns `.source` and `.stats`. Structured optimization returns
`.module` and `.stats`. Statistics are JSON strings in both cases. `parse(source)`
returns a value-owned `Module`, enabling programmatic edits to textual input.

```cpp
auto module = context.parse("fn f(x) { return x + 0; }");
context.check(module);
auto result = context.optimize(module, "AddZeroRight", 2);
auto report = context.analyze(result.module, "InstructionCount");
auto text = context.emit(result.module);
```

`load_kernels(directory)` adds custom declarations. `kernels_json()` returns the
inventory as JSON. `Context::read_file(path)` is a convenience for reading bytes;
the other string-taking methods interpret their input as source, not a pathname.

`native_handle()` exposes a borrowed C context pointer for interoperability.
Do not destroy it yourself or use it concurrently with wrapper operations.
Module/Expr values have no dependency on the lifetime of their originating Context.

[Contents](README.md) · [Next: DSLtk](22-dsltk-integration.md)
