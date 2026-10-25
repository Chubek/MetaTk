# 24. Native C++ function builders

`Function` owns its name, parameter vector, and statement vector. Fluent methods
append statements and return the function by reference. You may build a function
separately, then move or copy it into a module:

```cpp
metail::Function function("clamp_low", {"x"});
auto x = metail::var("x");
function.label("entry")
        .branch(x < 0, "negative", "nonnegative")
        .label("negative").ret(0)
        .label("nonnegative").ret(x);
metail::Module module("example");
module.add(std::move(function));
```

Use `append(Statement)` when generating statement kinds dynamically. A `Statement`
contains its C statement-kind enum, an optional `Expr`, and fields for names and
targets. Fields unrelated to that kind are ignored by conversion; use empty values
for clarity.

Function construction validates the function name immediately, while full module
validation happens on a Context operation. This permits forward branch targets
to be added later. Do not mistake a successfully appended branch for a validated
complete CFG.

Builder method calls execute in host C++. A host loop can deliberately emit many
statements, but those iterations are not represented as a runtime loop unless
you append labels and back edges. Similarly, a host lambda is not a MetaIL function
body unless your application explicitly uses it to call these builders.

The `controlflow.cpp` and `gcd.cpp` examples demonstrate complete fluent builders
without an embedded program-source string.

[Contents](README.md) · [Next: Editing modules](25-module-editing.md)
