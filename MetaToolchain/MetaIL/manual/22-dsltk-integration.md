# 22. DSLtk AST integration

`MetaIL.hpp` includes the actual `DSLtk.hpp` selected by `DSLTK_ROOT`. `Expr`
inherits DSLtk's `DSL<Expr, AST>` mixin and owns a `dsl::ASTNode`. Arithmetic
operators use DSLtk node builders. This is the native expression representation,
not an unused dependency or a source-generation facade.

AST conventions are:

| MetaIL expression | DSLtk representation |
|---|---|
| Integer | Leaf tagged `int`, decimal signed-64 value |
| Variable | Leaf tagged `var`, identifier value |
| Operator | Inner node tagged by canonical operator, argument children |
| Call | Inner `call` node, first child `callee` leaf, then argument children |

```cpp
auto tree = ::dsl::node<"add">(
    ::dsl::leaf<"var">("x"), ::dsl::leaf<"int">("0"));
auto expression = metail::Expr::from_ast(tree);
```

`from_ast` validates the schema, integer range, identifiers, arity, and limits.
`ast()` exposes a const reference for inspection; `dump()` prints DSLtk's
S-expression debug representation. That dump is not MetaIL source and should not
be passed to the textual program parser.

DSLtk stores leaf data as strings. The bridge decodes only the integer leaf payload
with `std::from_chars`; it never parses generated MetaIL source. Call callee leaves
are converted into the canonical call's name field.

General DSLtk rewrites are not automatically safe compiler optimizations. Keep
MetaIL wraparound, traps, and effects in mind; semantic pass integration still
belongs in Optikern and the verified optimizer.

[Contents](README.md) · [Next: C++ expressions](23-cpp-expressions.md)
