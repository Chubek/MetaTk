# 22. Editing and traversing owned modules

Juniper exposes editable values, not a hidden mutable compiler graph. Copy a
module when you need an independent version, edit that copy, then validate it.
There is no Juniper-specific visitor registration or rewrite-plugin API to call;
ordinary C++ traversal is sufficient for application-owned edits.

## A safe, narrow rewrite

This complete program replaces `add(value, 0)` with `value` without duplicating or
dropping evaluation of `value`. It does not generalize the transformation to
`mul(value, 0)` or other effect-sensitive identities.

<!-- check: cpp -->
```cpp
#include "Support.hpp"
#include <iostream>
#include <utility>

std::size_t remove_add_zero(juniper::Expr& expr) {
    std::size_t changes = 0;
    for (auto& arg : expr.args)
        changes += remove_add_zero(arg);
    if (expr.kind == METAIL_OP && expr.name == "add" &&
        expr.args.size() == 2 && expr.args[1].kind == METAIL_INT &&
        expr.args[1].value == 0) {
        // Copy the child before replacing its owner; do not assign directly
        // from an element that the assignment itself may destroy.
        juniper::Expr replacement = expr.args[0];
        expr = std::move(replacement);
        ++changes;
    }
    return changes;
}

bool has_expression(metail_stmt_kind kind) {
    return kind == METAIL_LET || kind == METAIL_RETURN ||
           kind == METAIL_EVAL || kind == METAIL_ASSUME ||
           kind == METAIL_BRANCH;
}

int main(int argc, char** argv) {
    try {
        juniper::Compiler compiler(argc > 1 ? argv[1] : nullptr);
        auto original = example::take(compiler.parse(
            "fn observed(x) { return add(add(tick(),0),add(x,0)); }"));
        example::take(compiler.check(original));
        auto edited = original;
        std::size_t changes = 0;
        for (auto& function : edited.functions)
            for (auto& statement : function.statements)
                if (has_expression(statement.kind))
                    changes += remove_add_zero(statement.expr);
        example::take(compiler.check(edited));
        example::expect(changes == 2, "unexpected rewrite count");
        const auto before = example::take(compiler.emit_metail(original));
        const auto after = example::take(compiler.emit_metail(edited));
        example::expect(before != after, "original was not independently owned");
        example::expect(after.find("tick()") != std::string::npos,
                        "effectful call was lost");
        std::cout << after;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```

The transformation is valid under signed-64 wraparound because adding literal
zero cannot change the value, and removing that literal evaluation loses no
effect or trap. The retained subtree is still evaluated once.

## Traversal is not execution

A visitor can inspect both children of a lazy `select` without executing either
one. A transformation must nevertheless keep those arms lazy in the resulting IR.
Do not rewrite it into a sequence that evaluates both arms first.

Validate before recursively traversing externally assembled native IR; otherwise
an application could walk over-limit structures before the library checks them.
For deliberate new transformations, add an application budget and tests rather
than looping until an unproven fixed point.

## Respect invalidation and scope

Vector edits can invalidate references and iterators. Do not keep a pointer to an
`Expr` while growing the vector that owns it. Local-variable substitution must
respect reassignment: blindly replacing every occurrence of a name can change
reads that occur after a later `let`.

For broad optimization, prefer `Compiler::optimize` and MetaIL's guarded kernels.
Keep native edits focused on transformations whose semantics and ownership you
can state and test precisely.

[Previous](21-native-ir.md) · [Index](README.md) · [Next](23-preprocessing-api.md)
