# 6. Your first native C++ program

This complete application builds IR using C++ syntax and DSLtk-backed expressions:

```cpp
#include <MetaIL.hpp>
#include <iostream>

int main() {
    try {
        metail::Context context;
        auto x = metail::var("x");
        metail::Module module("demo");
        module.function("scale", {"x"}).ret((x + 0) * 8);
        auto result = context.optimize(module,
            "AddZeroRight,MultiplyByPowerOfTwoRight03");
        std::cout << context.emit(result.module);
        std::cout << result.stats;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```

Link it with `MetaIL::MetaIL` as described in chapter 4. The `Module`, its
`Function`, and the `Expr` values are ordinary owning C++ objects. Creating them
does not require a live runtime. A `Context` is needed for validation, analysis,
optimization, text parsing, and emission.

`optimize(module)` returns another structured module and JSON statistics. It
does not mutate `module`. The original expression remains a multiplication;
the output expression is a shift. Both can be inspected without emitting text.

Run `build/metail_example_native_dsl` for a compiled example that also imports
a DSLtk AST and edits a parsed textual program.

[Contents](README.md) · [Next: Tokens and names](07-tokens-and-names.md)
