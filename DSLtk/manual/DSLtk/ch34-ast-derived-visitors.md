# Chapter 34 — Deriving Visitors from AST Schemas

A visitor combines the vocabulary discovered in an AST schema with
user-supplied semantic handlers. Template instantiation generates handler
dispatch; the schema determines which tags are accepted.

## Register operations with `on`

`on<"tag">(fn)` in C++ and `on!"tag"(fn)` in D produce a `VisitorCase`.
Handlers may take one node argument or a compile-time tag followed by a node.
The node carries runtime values; the tag identifies the handler specialization.

```cpp
auto number = dsl::on<"num">([](auto tag, const dsl::ASTNode& node) {
    static_assert(tag.value == "num");
    assert(!node.value().empty());
});
```

```d
struct NumberHandler
{
    void opCall(Tag)(Tag tag, ref ASTNode node)
    {
        static assert(Tag.value == "num");
        assert(node.value().length != 0);
    }
}
auto number = on!"num"(NumberHandler());
```

D also accepts alias-bound generic handlers through `on!(tag, fn)()`.
Concrete functions/delegates/functors retain their types in the visitor.

## Derive from a runtime AST

```cpp
auto schema = dsl::node<"add">(dsl::leaf<"num">(0));
int sum = 0;
auto visitor = dsl::derive_visitor(schema,
    dsl::on<"add">([](const dsl::ASTNode&) {}),
    dsl::on<"num">([&](const dsl::ASTNode& n) { sum += std::stoi(n.value()); }));
auto tree = dsl::node<"add">(dsl::leaf<"num">(2), dsl::leaf<"num">(3));
visitor.walk(tree);
assert(sum == 5);
```

```d
import std.conv : to;
auto schema = node!"add"(leaf!"num"(0));
int sum;
auto visitor = derive_visitor(schema,
    on!"add"((ref ASTNode n) {}),
    on!"num"((ref ASTNode n) { sum += to!int(n.value()); }));
auto tree = node!"add"(leaf!"num"(2), leaf!"num"(3));
visitor.walk(tree);
assert(sum == 5);
```

Schema collection discovers unique tags recursively. Its values and arities
do not constrain later trees: the schema above permits an `add` with two `num`
children although its example tree has only one. The validation contract is
tag coverage. The user handler can enforce additional structural rules.

Every schema tag must have a handler at construction. An unknown tag in a
visited tree raises an exception. Handlers execute in registration order, with
the first matching case used. `compile_visitor(runtimeSchema, handlers...)`
also offers this runtime-validated construction.

## Validate the schema at compilation

C++ supplies a structural tree of types:

```cpp
auto schema = dsl::ast_schema<"add">(dsl::ast_schema<"num">());
auto visitor = dsl::compile_visitor(schema,
    dsl::on<"add">([](const dsl::ASTNode&) {}),
    dsl::on<"num">([](const dsl::ASTNode&) {}));
```

`ASTSchema<Tag, Children...>` records the tag and child types. `ast_schema`
constructs its type-level vocabulary, and `compile_visitor` checks handler
coverage with a compile-time assertion before creating the runtime visitor.

D can use an `ASTNode` value computed by CTFE:

```d
enum schema = node!"add"(leaf!"num"(0));
auto visitor = compile_visitor!schema(
    on!"add"((ref ASTNode n) {}),
    on!"num"((ref ASTNode n) {}));
```

Each discovered schema tag is checked by `static foreach`/`static assert`.
The handler code is still supplied by the user. A runtime schema selects
accepted tags and validates them at runtime; a static schema validates them
during host-language compilation. Neither path performs runtime JIT compilation.

## Dispatch and traversal

`visit(node)` dispatches one node. `walk(node, order)` recursively traverses:

| Order | C++ | D | Handler timing |
|---|---|---|---|
| Preorder | `VisitOrder::Preorder` | `VisitOrder.Preorder` | Before children |
| Postorder | `VisitOrder::Postorder` | `VisitOrder.Postorder` | After children |

Preorder is the default. C++ traversal supports const or mutable ASTs when
the handlers accept the corresponding node reference. D's public visitor
operations take `ref ASTNode`, permitting mutation by callbacks.
Handlers that change child structure affect the traversal as it runs;
choose the order to suit the transformation.

## Equality-triggered visitor operations

After saturation, convert an extracted representative to `ASTNode` and
dispatch it through a visitor:

```d
auto graph = new EGraph;
auto root = graph.add("(add a 0)");
auto atom = graph.add("a");
assert(graph.saturate(sexpr_rules("(= (add ?x 0) ?x)")).saturated());
size_t calls;
graph.when_equal(root, atom, (EGraph g, EClassId id) {
    auto tree = g.extract_ast(id);
    enum schema = leaf!"atom"("");
    auto visitor = compile_visitor!schema(
        on!"atom"((ref ASTNode n) { ++calls; assert(n.value() == "a"); }));
    visitor.walk(tree);
});
assert(calls == 1);
```

The C++ equivalent is in
[`examples/26-equality-saturation.cpp`](../examples/26-equality-saturation.cpp).
Grammar-produced ASTs use nonterminal names and `token` leaves, so the same
visitor mechanism can implement grammar semantic passes (Chapter 36).
