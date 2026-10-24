# 30. Result: explicit error pipelines

`Result 'a 'e` distinguishes success `Ok value` from failure `Err error`. Error payloads can be any type; host libraries commonly use String. Unlike a diagnostic, an Err value is ordinary data and does not abort evaluation unless application code deliberately unwraps it.

```metaml
positive s := Int.from_string s |> Result.from_option "not an integer" |> Result.and_then (fn n -> if n > 0 then Ok n else Err "not positive")
Test.equal "success" (positive "12") (Ok 12)
Test.equal "failure" (positive "0") (Err "not positive")
Test.equal "sequence first error" (Result.sequence [Ok 1, Err "first", Err "second"]) (Err "first")
Test.equal "combine" (Result.map2 (+) (Ok 20) (Ok 22)) (Ok 42)
```

`map` transforms successes; `map_err` changes error payloads; `and_then` chains a fallible computation. `get_or` supplies a default, `to_option` discards an error, and `from_option` assigns one to an absent value. `unwrap` aborts with `M0407` on Err and includes a rendered error in the message.

`sequence` and `traverse` collect successful values in input order. Empty input returns `Ok []`. Traversal stops invoking its callback at the first Err and returns that error unchanged. They are validation pipelines with short-circuiting, not error accumulators; use a separate collector if every error must be reported.

`map2` combines two successful Results and prefers the first argument's error if both are Err. Because its arguments are evaluated eagerly, it cannot prevent work needed to produce the second Result. By contrast, a second computation inside an `and_then` callback runs only after a first success.

Keep error types consistent across a chain. An application-specific ADT can preserve categories more reliably than concatenating strings. Render user-facing text at the boundary, where source context or recovery instructions are available.

**Source:** [`result/mod.ml`](../metalib/result/mod.ml).

<!-- api:begin -->
```text
  val ok : 'a -> Result 'a 'e
  val err : 'e -> Result 'a 'e
  val is_ok : Result 'a 'e -> Bool
  val is_err : Result 'a 'e -> Bool
  val get_or : Result 'a 'e -> 'a -> 'a
  val map : ('a -> 'b) -> Result 'a 'e -> Result 'b 'e
  val map_err : ('e -> 'f) -> Result 'a 'e -> Result 'a 'f
  val and_then : ('a -> Result 'b 'e) -> Result 'a 'e -> Result 'b 'e
  val to_option : Result 'a 'e -> Option 'a
  val from_option : 'e -> Option 'a -> Result 'a 'e
  val unwrap : Result 'a 'e -> 'a
  val map2 : ('a -> 'b -> 'c) -> Result 'a 'e -> Result 'b 'e -> Result 'c 'e
  val flatten : Result (Result 'a 'e) 'e -> Result 'a 'e
  val sequence : [Result 'a 'e] -> Result ['a] 'e
  val traverse : ('a -> Result 'b 'e) -> ['a] -> Result ['b] 'e
```
<!-- api:end -->

<!-- nav:begin -->
[Contents](README.md) · [Previous](29-option-library.md) · [Next](31-input-output.md)
<!-- nav:end -->
