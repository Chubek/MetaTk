# 18. Algebraic data types

An algebraic data type declares a finite collection of constructors, each with its own argument types. Constructor names begin uppercase. A leading `|` makes an ADT declaration unambiguous when a single constructor might otherwise resemble a type alias.

```metaml
type Shape := | Circle Float | Rectangle Float Float
area shape := match shape with
  | Circle r -> 3.141592653589793 * r * r
  | Rectangle w h -> w * h
Test.equal "constructor arguments" (area (Rectangle 3.0 4.0)) 12.0
```

Constructors behave as curried functions until saturated. The resulting value stores its constructor tag and arguments. Patterns recover those arguments without exposing a mutable representation. Constructor order participates in structural ordering, so do not use incidental declaration order as a version-stable business ranking.

Type parameters express reusable containers. The builtin declarations are conceptually `Option 'a` with `None` and `Some 'a`, and `Result 'a 'e` with `Ok 'a` and `Err 'e`. These types are present in a fresh session even before loading the portable library; helper functions such as `Option.map` come from `metalib`.

```metaml
type Box 'a := | Box 'a
unbox (Box x) := x
Test.equal "parameterized ADT" (unbox (Box "payload")) "payload"
```

Prefer constructors over magic strings for application states. A signature can hide the representation of a type while exposing constructor functions or query operations. Chapter 20 explains this form of abstraction.

Custom constructors are not automatically JSON schemas or native ABI layouts. Canonical `Core.show` output is useful for inspection; choose an explicit serializer for persistence or interoperability. Host structured-data modules provide their own ADTs and conversion rules, discussed in chapters 35 and 36.

<!-- nav:begin -->
[Contents](README.md) · [Previous](17-pattern-matching.md) · [Next](19-modules.md)
<!-- nav:end -->
