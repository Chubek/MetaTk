# 29. Option: absence without exceptions

`Option 'a` is either `None` or `Some value`. It models absence when there is no useful error payload: missing table entries, out-of-range indexing, or a numeric parse that simply did not match. The constructors are builtin; the helper functions belong to the portable Option module.

```metaml
Test.equal "mapping" (Some 20 |> Option.map (+ 1)) (Some 21)
Test.equal "default" (None |> Option.with_default 0) 0
Test.equal "sequence" (Option.sequence [Some 1, Some 2]) (Some [1, 2])
Test.equal "traverse" (Option.traverse Int.from_string ["1", "bad", "3"]) None
Test.equal "nested option" (Option.flatten (Some (Some 7))) (Some 7)
```

`map` transforms a present value. `flat_map` chains a function that already returns Option. `bind` is the data-first counterpart to `flat_map`. `get_or option default` is also data-first; `with_default default option` is the pipeline-friendly order. `get` raises `M0407` on None, so it is best reserved for checked invariants.

`sequence` turns a list of Options into an Option of a list. Empty input succeeds with `Some []`; any None yields None. `traverse` maps left to right and stops invoking its callback at the first None, preserving successful result order. This matters when later work is expensive or has effects. `flatten` removes one Option layer.

`to_result error option` promotes absence into a chosen error payload. `to_list` maps absence to an empty list and presence to a singleton. `filter` keeps a present value only if a predicate succeeds; `map2` requires both inputs to be present.

Arguments are eager. `get_or expensive_option (expensive_default ())` evaluates the default before calling the helper, even if the Option is Some. Use a `match` when fallback construction must be lazy. The same caution applies to `or_else` alternatives.

**Source:** [`option/mod.ml`](../metalib/option/mod.ml).

<!-- api:begin -->
```text
  val some : 'a -> Option 'a
  val none : Option 'a
  val is_some : Option 'a -> Bool
  val is_none : Option 'a -> Bool
  val get : Option 'a -> 'a
  val get_or : Option 'a -> 'a -> 'a
  val with_default : 'a -> Option 'a -> 'a
  val map : ('a -> 'b) -> Option 'a -> Option 'b
  val flat_map : ('a -> Option 'b) -> Option 'a -> Option 'b
  val bind : Option 'a -> ('a -> Option 'b) -> Option 'b
  val filter : ('a -> Bool) -> Option 'a -> Option 'a
  val or_else : Option 'a -> Option 'a -> Option 'a
  val to_list : Option 'a -> ['a]
  val iter : ('a -> Unit) -> Option 'a -> Unit
  val map2 : ('a -> 'b -> 'c) -> Option 'a -> Option 'b -> Option 'c
  val flatten : Option (Option 'a) -> Option 'a
  val to_result : 'e -> Option 'a -> Result 'a 'e
  val sequence : [Option 'a] -> Option ['a]
  val traverse : ('a -> Option 'b) -> ['a] -> Option ['b]
```
<!-- api:end -->

<!-- nav:begin -->
[Contents](README.md) · [Previous](28-list-library.md) · [Next](30-result-library.md)
<!-- nav:end -->
