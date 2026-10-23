# 20. Signatures and abstraction boundaries

A module signature lists the public types and values required from a structure. Define a named signature with `module type Name := sig ... end`. Value specifications use `val name : type`. An abstract type declaration omits its representation.

```metaml
module type COUNTER := sig
  type t
  val zero : t
  val bump : t -> t
  val get : t -> Int
end
module Hidden_counter :> COUNTER := struct
  type t = Int
  val zero := 0
  val bump n := n + 1
  val get n := n
  val implementation_detail := 99
end
Test.equal "abstract API" (Hidden_counter.get (Hidden_counter.bump Hidden_counter.zero)) 1
```

Transparent ascription uses `:`; opaque sealing uses `:>`. Both check that the implementation satisfies the declared interface. Opaque sealing hides unspecified members and gives abstract types a public identity independent of the implementation representation. In the current checker, transparent ascription checks compatibility but retains the implementation scope; do not depend on it to hide extra fields.

The portable library modules use sealed interfaces so their helper definitions do not become accidental public APIs. This separation allows an implementation to change an accumulator, choose a host primitive, or reorganize private functions without changing the public type surface.

A type alias in a signature intentionally exposes an equality, for example `type t = Int`. An abstract `type t` intentionally does not. Choose based on whether callers must interoperate directly with that representation.

Signature mismatches produce module diagnostics, commonly `M0502`, with the missing or incompatible member identified. Explicitly test both successful clients and rejected representation leaks when adding a new sealed API.

**Pitfall:** a matching value name is insufficient; its polymorphism and type constructor arguments must also satisfy the specification.

<!-- nav:begin -->
[Contents](README.md) · [Previous](19-modules.md) · [Next](21-functors.md)
<!-- nav:end -->
