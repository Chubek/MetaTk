# 25. Inspecting and editing modules

The C++ representation is a value model, not an opaque runtime handle. `Module`
exposes an optional name and a vector of functions. Each function exposes parameters
and statements. Expression trees are inspected through their const DSLtk AST view.

```cpp
auto module = context.parse("fn answer() { return 40; }");
auto &statement = module.functions.at(0).statements.at(0);
if (statement.kind == METAIL_RETURN && statement.expr) {
    statement.expr = metail::integer(42);
}
context.check(module);
```

You can insert, erase, reorder, or replace statements using standard container
operations. These edits can invalidate CFG references, so validate after a batch
of edits. Editing a label requires updating all corresponding branch/jump targets.
Changing a parameter vector does not rename variable nodes automatically.

`auto copy = module` makes an independent value copy. Mutating `copy` does not
change the original expression trees or strings. Likewise, `context.optimize`
returns a new module instead of editing its argument.

References into `functions` or `statements` follow normal `std::vector` invalidation
rules. A reference returned by `Module::function` can be invalidated by adding
another function. Keep indices, reserve storage, or complete one builder before
growing its owning vector.

The const `Expr::ast()` view prevents silently bypassing expression validation.
To transform a tree yourself, copy its AST, modify the copy using DSLtk, then
reconstruct it with `Expr::from_ast` before replacing the expression.

[Contents](README.md) · [Next: Interoperability](26-interoperability.md)
