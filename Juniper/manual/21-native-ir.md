# 21. Constructing native IR values

Native construction is useful when your application already has an AST or wants
to avoid manufacturing source strings. The value model mirrors MetaIL's scalar
structured API; there are no borrowed source buffers or implicit parser objects
inside a `Module`.

## Build a complete module without parsing

The following standalone example uses the helper introduced in chapter 20:

<!-- check: cpp -->
```cpp
#include "Support.hpp"
#include <iostream>
#include <utility>

juniper::Expr literal(std::int64_t value) {
    juniper::Expr expr;
    expr.kind = METAIL_INT;
    expr.value = value;
    return expr;
}

juniper::Expr variable(std::string name) {
    juniper::Expr expr;
    expr.kind = METAIL_VAR;
    expr.name = std::move(name);
    return expr;
}

juniper::Expr add(juniper::Expr left, juniper::Expr right) {
    juniper::Expr expr;
    expr.kind = METAIL_OP;
    expr.name = "add";
    expr.args = {std::move(left), std::move(right)};
    return expr;
}

int main(int argc, char** argv) {
    try {
        juniper::Compiler compiler(argc > 1 ? argv[1] : nullptr);
        juniper::Statement ret;
        ret.kind = METAIL_RETURN;
        ret.expr = add(variable("x"), literal(7));

        juniper::Function function;
        function.name = "add_seven";
        function.params = {"x"};
        function.statements.push_back(std::move(ret));

        juniper::Module module;
        module.name = "native_example";
        module.functions.push_back(std::move(function));
        example::take(compiler.check(module));
        std::cout << example::take(compiler.emit(module));
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```

`literal`, `variable`, and `add` above are local builder functions. They are not
convenience functions promised by the Juniper API. Keeping small builders in
your application's namespace is a good way to make construction readable without
inventing a second source language.

## Fill fields according to the node kind

| Expression kind | Meaningful fields |
|---|---|
| `METAIL_INT` | `value` |
| `METAIL_VAR` | `name` |
| `METAIL_OP` | canonical operator `name`, ordered `args` |
| `METAIL_CALL` | callee `name`, ordered `args` |

For statements, `METAIL_LET` uses `name` and `expr`; return/eval/assume use
`expr`; labels use `name`; goto uses `target`; branches use `expr`, `yes`, and
`no`. Leave unused fields at their default values rather than using them as
private metadata that another operation is expected to preserve.

For example, this is a branch **fragment** to insert into a valid function:

```cpp
juniper::Statement branch;
branch.kind = METAIL_BRANCH;
branch.expr = variable("condition");
branch.yes = "taken";
branch.no = "not_taken";
```

Both labels must actually appear in that function. Native construction does not
waive identifier, operator-arity, CFG-target, or budget checks.

## Backend defaults are deliberate

A default `Module` starts with `Backend::portable()`. A separately default-built
`Backend{}` has the two required resources but **no rules**. Assigning an empty
backend to the module therefore removes arithmetic/literal coverage even if its
name happens to be `portable`.

Call `compiler.check(module)` at your construction boundary. It validates
structure through the real MetaIL API, not merely the C++ types. It does not prove
locals initialized, calls resolved, or your intended algorithm correct.

[Previous](20-cpp-contexts.md) · [Index](README.md) · [Next](22-editing-ir.md)
