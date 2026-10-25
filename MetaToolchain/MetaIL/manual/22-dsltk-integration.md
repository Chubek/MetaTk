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
It rejects embedded NULs in tags and children attached to leaf nodes, including
callee metadata. The limits remain 256 expression levels and 200,000 expression
nodes; callee metadata is not an IR expression node.
`ast()` exposes a const reference for inspection; `dump()` prints DSLtk's
S-expression debug representation. That dump is not MetaIL source and should not
be passed to the textual program parser.

DSLtk stores leaf data as strings. The bridge decodes only the integer leaf payload
with `std::from_chars`; it never parses generated MetaIL source. Call callee leaves
are converted into the canonical call's name field.

## Const visitors

Use DSLtk's `derive_visitor` and `on` cases for typed tag dispatch:

```cpp
auto expression = metail::var("x") + 1;
std::vector<std::string> variables;
auto visitor = ::dsl::derive_visitor(expression.ast(),
    ::dsl::on<"add">([](const ::dsl::ASTNode &) {}),
    ::dsl::on<"int">([](const ::dsl::ASTNode &) {}),
    ::dsl::on<"var">([&](const ::dsl::ASTNode &n) {
        variables.push_back(n.value());
    }));
expression.visit(visitor); // Or dsl::VisitOrder::Postorder.
```

Callbacks receive borrowed const nodes. Calls include a `callee` metadata leaf,
so visitors for calls need a handler for that tag too. DSLtk checks that handlers
cover the visitor schema and rejects a node whose tag is outside that schema.
Visitor exceptions propagate; the expression is unchanged.

## Checked DSLtk rewrites

```cpp
auto rules = ::dsl::rewrite_set(::dsl::rule<"add-zero">(
    [](const ::dsl::ASTNode &n) {
        return n.tag() == "add" && n.child(1) == metail::integer(0).ast();
    },
    [](const ::dsl::ASTNode &n) { return n.child(0); }));
auto result = metail::rewrite((metail::var("x") + 0) + 0, rules);
// result.expression is x; applications == 2; passes == 2.
auto batch = metail::rewrite_expressions(module, rules, {100, 10000});
context.check(batch.module);
```

The adapter uses DSLtk's bottom-up rewrite traversal. At each expression node,
rules run in declaration order and later rules see earlier replacements. New
replacement children are visited on the next pass. Callee metadata is excluded
from rewriting; rename a call by replacing its complete `call` node. Every rule
replacement is validated immediately, before another rule sees it. The complete
tree is validated after each pass to catch aggregate depth and node limits.

`RewriteLimits` contains `max_passes` (100 by default, per expression) and
`max_applications` (10,000 by default, shared across a module). These explicit
limits supersede the input `RewriteSet::max_iterations`. A pass with no structural
changes proves convergence; it is included in `passes`. A matching identity rule
counts as neither a change nor an application. A changing pass needs a subsequent
unchanged pass, even when the result looks final. Zero application budget permits
only unchanged results; zero pass budget is invalid. Cycles and expanding rules
fail with `metail::Error` when a budget or IR limit is reached. No silently
truncated result is returned.

`RewriteResult` owns `expression` and records `passes` and `applications`.
`ModuleRewriteResult` owns `module` and records `expressions`, total `passes`, and
total `applications`. Inputs remain unchanged on success or failure. Callback
exceptions propagate, and callback side effects outside the tree cannot be rolled
back. Budgets bound traversal and accepted replacements, not allocation or time
spent inside user callbacks. Treat callbacks as trusted C++ code.

Structural validation does **not** prove a rule preserves behavior. Custom rules
must respect signed-64 wraparound, division and shift traps, operand evaluation
order, and effects. The example removes only a constant zero and retains the other
operand exactly once. Do not replace `tick() * 0` or `(1 / 0) * 0` with zero.
Dropping, duplicating, or reordering arbitrary subtrees requires proving them pure
(including trap freedom). Optikern remains the verified optimization pipeline;
these native callbacks are not automatically registered as kernels.

## Simultaneous substitution

```cpp
auto x = metail::var("x"), y = metail::var("y");
auto changed = (x + y).substitute({{"x", y + 1}, {"y", metail::integer(2)}});
// (y + 1) + 2: inserted trees are not substituted again.
```

Substitution uses one DSLtk bottom-up pass, matches only variable leaves, and
validates the final result. It leaves call names unchanged. Self-referential and
cyclic bindings terminate because inserted trees are not revisited. This is a
syntactic editing operation: duplicating a binding containing a call can duplicate
its effects. It does not rename function parameters, resolve bindings, or guarantee
program equivalence. Use `Context::check` after inserting the result in a module.

All results own their AST storage and need no Lua context. Concurrent reads of an
expression are safe; callers must synchronize any shared mutable callback state.

[Contents](README.md) · [Next: C++ expressions](23-cpp-expressions.md)
