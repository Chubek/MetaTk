# 11. Modules and functions

A program contains an optional module name and at least one function. Function
names must be unique within the program; parameter names must be unique within
each function. Function order is preserved by parsing and emission.

```text
module arithmetic;
fn increment(x) { return x + 1; }
fn double(x) { return x * 2; }
```

Module names currently label a program and its analysis output. They are not an
import system, namespace loader, or linker unit. Calls are represented by name,
but call resolution and argument-type checking are outside the implemented
validation contract.

The C model uses `metail_module.functions` and `function_count`, with a nullable
module `name`. Each `metail_function` contains a name, parameter string array,
and statement array. The C++ equivalents are:

```cpp
metail::Module module("arithmetic");
module.function("increment", {"x"}).ret(metail::var("x") + 1);
module.function("double", {"x"}).ret(metail::var("x") * 2);
```

An empty function is allowed; an empty module cannot pass validation. There is
no requirement that every function end with `return`, and validation is not a
complete execution-safety checker. Explicit returns and explicit control-flow
edges make examples and transformation behavior easier to reason about.

When holding a C++ reference returned by `Module::function`, finish using it before
adding functions that could reallocate the module's `std::vector`.

[Contents](README.md) · [Next: Statements](12-statements.md)
