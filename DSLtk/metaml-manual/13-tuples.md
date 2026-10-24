# 13. Tuples and product data

A tuple combines a fixed number of values whose types may differ. Its arity and component types are statically known. Parentheses also group expressions, so `(x)` is just `x`, while `(x, y)` is a two-component tuple. Unit `()` is a separate primitive value.

```metaml
swap_pair (x, y) := (y, x)
point := (3, 4)
distance_squared (x, y) := x * x + y * y
Test.equal "tuple pattern" (swap_pair point) (4, 3)
Test.equal "tuple calculation" (distance_squared point) 25
```

Tuple patterns provide a straightforward way to unpack results from functions such as `List.partition`, `List.split_at`, `List.unzip`, and parser combinators. Prefer a local match or parameter pattern over repeatedly selecting components from a nested product.

Core exposes `fst`, `snd`, `swap`, `curry`, and `uncurry`, with common helpers opened by the prelude. `curry` converts a pair-taking function into a curried function; `uncurry` does the reverse. This distinction matters because the language supports both whitespace application and parenthesized multiargument call syntax.

A tuple is not an extensible record. Adding a component changes its type and every corresponding pattern. Use records for fields that benefit from names, and algebraic data types when the shape may be one of several alternatives.

```metaml
curried := Core.curry (fn (x, y) -> x + y)
Test.equal "curry" (curried 2 3) 5
Test.equal "uncurry" (Core.uncurry curried (4, 5)) 9
```

Tuple equality and ordering compare components structurally. If a component is a function, attempting structural comparison can fail at runtime. Opaque host handles retain identity semantics rather than automatically comparing the contents of the external resource.

<!-- nav:begin -->
[Contents](README.md) · [Previous](12-local-bindings-and-recursion.md) · [Next](14-list-syntax.md)
<!-- nav:end -->
