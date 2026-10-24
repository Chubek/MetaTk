# 21. Functors and parameterized modules

A functor accepts a module matching a signature and returns a structure. This is useful when a reusable algorithm depends on a collection of related types and operations, rather than just one callback.

```metaml
module type SHOWABLE := sig
  type t
  val show : t -> String
end
module Show_int : SHOWABLE := struct
  type t = Int
  val show := Int.to_string
end
module Label (X : SHOWABLE) := struct
  val render x := "value=" ^ X.show x
end
module Int_label := Label(Show_int)
Test.equal "functor" (Int_label.render 42) "value=42"
```

The parameter signature defines which operations the functor may use. This makes dependencies explicit and prevents the body from relying on a concrete implementation's private details. A result signature can further restrict the returned module.

Functor application belongs to module syntax. It is not an ordinary value-level call that can be passed to `List.map`. Bind the application to a module, then use its exported values. For a single operation with no associated types, a higher-order function is often simpler.

Opaque abstract types and fresh module applications interact through type identities. Avoid assuming two abstract types are interchangeable merely because their implementations both use `Int`. Expose a shared alias in a signature when equality is part of the contract, or provide an explicit conversion function.

Runtime initialization still follows module laziness. An applied functor can capture its argument module and initialize its result when forced. Be careful about effects in either structure: parameterization does not automatically make initialization pure or repeatable.

**Design exercise:** compare an ordering functor argument containing `type t` and `compare` with passing a comparator directly to `List.sort_by`. Use the functor when several returned operations must agree on the same abstract element type.

<!-- nav:begin -->
[Contents](README.md) · [Previous](20-signatures-and-abstraction.md) · [Next](22-imports.md)
<!-- nav:end -->
