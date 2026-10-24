# 11. Inference, signatures, and constraints

MetaML uses Hindley–Milner-style inference with rank-one polymorphism, numeric constraints, and row-polymorphic records. You can usually omit a signature, but signatures document intent and disambiguate composition. Type variables are written as apostrophe-prefixed names, for example `'a`.

```metaml
identity :: 'a -> 'a
identity x := x
pair :: 'a -> 'b -> ('a, 'b)
pair x y := (x, y)
Test.equal "polymorphism" (identity 1, identity "x") (1, "x")
Test.equal "pair" (pair true 3) (true, 3)
```

Function arrows associate to the right: `Int -> Int -> Int` describes a curried twoargument function. Lists use `['a]`, tuples use `('a, 'b)`, and type constructors accept arguments such as `Option Int` or `Result Int String`. Parenthesize a type constructor application when it is itself an argument: `Option (Option Int)`.

Numeric operations introduce `Num` constraints. A signature such as `Num 'a => ['a] -> 'a` is used by numeric reductions. In unconstrained situations, the checker may default numeric variables. Do not infer runtime representation solely from a polymorphic-looking printed type; pass explicit conversions at foreign-function boundaries.

Records can have row tails: `{ name : String | r } -> String` accepts records with at least a `name` field. The row represents additional fields, not a dictionary of arbitrary dynamically typed values.

`#type` and `Session.type_of` check an expression without evaluating it. They are appropriate for editors and interactive exploration, but not a runtime side-effect test. Type errors belong to `M03xx`; module signature errors usually belong to `M05xx`.

**Caution:** mutable opaque host values require disciplined homogeneous use. The current language does not impose a general ML value restriction; do not treat a polymorphic mutable handle as independent storage for unrelated types.

<!-- nav:begin -->
[Contents](README.md) · [Previous](10-pipelines.md) · [Next](12-local-bindings-and-recursion.md)
<!-- nav:end -->
