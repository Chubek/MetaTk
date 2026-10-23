# 24. Core and the prelude

Core contains general functional helpers, comparison, rendering, and explicit error construction. The prelude opens Core and adds common conveniences: `print`, `println`, `show_line`, `sum`, `product`, `length`, `map`, `filter`, `fold`, `reverse`, `range`, and `concat`.

```metaml
Test.equal "identity" (Core.id "x") "x"
Test.equal "constant" (Core.const 3 "ignored") 3
Test.equal "flip" (Core.flip (fn x y -> x - y) 2 9) 7
Test.equal "prelude pipeline" ([1, 2, 3] |> map (* 2) |> sum) 12
Test.equal "string concat" (concat ["Meta", "ML"]) "MetaML"
```

`Core.show` returns canonical value notation. Strings are quoted and escaped, lists use brackets, constructors display their names, and functions display `<fn>`. Use `IO.print` for an unquoted string and `show_line` when inspecting an arbitrary value.

`Core.equal` and `Core.compare` are structural operations with runtime limitations. Functions are incomparable. NaN does not provide a useful mathematical ordering, and host handles are generally identity values rather than recursively serializable resources. `Core.hash` hashes the rendered representation; it is not a cryptographic digest or a universally valid application key function.

`Core.ignore` discards a result and returns unit. `Core.error` aborts with `M0407`; it is polymorphic so it can appear in any result position. Avoid using it for routine invalid input when `Result` would preserve a recoverable error.

The library includes a tiny `Test` module used by fixtures. `Test.equal name actual expected` checks structural equality, and `Test.check name condition` checks a boolean. Successful tests print deterministic `ok` lines; a failure aborts the fixture. This harness is also used to validate the manual's complete examples.

**Sources:** [`core/mod.ml`](../metalib/core/mod.ml), [`prelude/mod.ml`](../metalib/prelude/mod.ml).

<!-- nav:begin -->
[Contents](README.md) · [Previous](23-diagnostics.md) · [Next](25-integer-library.md)
<!-- nav:end -->
