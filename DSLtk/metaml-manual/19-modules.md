# 19. Structures, paths, open, and include

A structure groups values, types, and nested modules under a module name. A qualified path accesses a member without putting every name into the surrounding scope. This is the default organization style for reusable MetaML code.

```metaml
module Counter := struct
  type t = { n : Int }
  val zero := { n = 0 }
  val bump c := { n = c.n + 1 }
  val get c := c.n
end
Test.equal "module path" (Counter.get (Counter.bump Counter.zero)) 1
```

`open M` makes accessible members available by unqualified names. `include M` additionally re-exports members when used inside another structure. Use `open` sparingly: broad imports make it harder to see whether `map` means a local helper, a prelude function, or a module-specific operation.

Module bodies initialize lazily, on first access that forces their runtime value. A body normally runs once per module object. An initialization cycle can fail when a requested field is not yet available; the language does not promise arbitrary cyclic initializers will succeed.

Keep module initialization mostly pure. If a module body prints, opens files through an optional host API, or mutates a handle, the time of its first use determines when those effects occur. A failed initialization can leave earlier external effects behind even if a later access retries the body.

Modules are not ordinary extensible records: they also contain type definitions and signatures. A module alias or functor application is resolved through the module system rather than by ordinary function application on a record value.

```metaml
module Arithmetic := struct
  val twice x := x + x
end
open Arithmetic
Test.equal "opened member" (twice 21) 42
```

**Related:** chapter 20 covers visibility and abstract types; chapter 21 covers parameterized modules.

<!-- nav:begin -->
[Contents](README.md) · [Previous](18-algebraic-data.md) · [Next](20-signatures-and-abstraction.md)
<!-- nav:end -->
