# 26. Float: IEEE arithmetic and conversion

Float provides constants, arithmetic, elementary functions, rounding, classification, and conversion. Its values are binary64, so decimal fractions such as 0.1 are not generally exact. Mathematical library results can depend on the platform's libm at the last bit; dialect parity is checked on the same host toolchain environment.

```metaml
Test.equal "square root" (Float.sqrt 9.0) 3.0
Test.equal "rounding" (Float.floor 2.9, Float.ceil 2.1, Float.round 2.5) (2.0, 3.0, 3.0)
Test.check "classification" (Float.is_finite 1.0 && Float.is_infinite Float.infinity)
Test.check "NaN" (Float.is_nan Float.nan && not (Float.is_finite Float.nan))
Test.equal "empty parse" (Float.from_string "") None
```

`from_string` returns `Option Float`; `of_string` raises a runtime diagnostic on failure. Whole-string parsing rejects leading whitespace and malformed underscore placement. The conversion delegates numeric interpretation to the host C conversion routine, so do not use it as a strict JSON-number validator. The JSON module has its own parser.

`floor`, `ceil`, and `round` return Float values. `truncate` and `to_int` return Int, saturating at the signed limits and mapping NaN to zero. This is intentionally different from arbitrary implementation-defined native casts. `of_int` can lose precision beyond the exact-integer range of binary64.

`to_string` seeks a short round-tripping representation, preserves negative zero, and renders special values as `nan`, `inf`, or `-inf`. Such text is MetaML-oriented, not necessarily acceptable to JSON or another external protocol.

Use tolerances when testing approximate numerical algorithms. Direct equality is appropriate only when exact results or exact round trips are part of the contract. In particular, avoid assuming `Float.compare` establishes a meaningful total order for NaNs.

**Source:** [`float/mod.ml`](../metalib/float/mod.ml).

<!-- api:begin -->
```text
  type t = Float
  val zero : t
  val one : t
  val pi : t
  val e : t
  val infinity : t
  val nan : t
  val add : t -> t -> t
  val sub : t -> t -> t
  val mul : t -> t -> t
  val div : t -> t -> t
  val neg : t -> t
  val abs : t -> t
  val min : t -> t -> t
  val max : t -> t -> t
  val compare : t -> t -> Int
  val floor : t -> t
  val ceil : t -> t
  val round : t -> t
  val truncate : t -> Int
  val to_int : t -> Int
  val sqrt : t -> t
  val exp : t -> t
  val log : t -> t
  val sin : t -> t
  val cos : t -> t
  val tan : t -> t
  val pow : t -> t -> t
  val atan2 : t -> t -> t
  val is_nan : t -> Bool
  val is_finite : t -> Bool
  val is_infinite : t -> Bool
  val of_int : Int -> t
  val of_string : String -> t
  val from_string : String -> Option t
  val to_string : t -> String
```
<!-- api:end -->

<!-- nav:begin -->
[Contents](README.md) · [Previous](25-integer-library.md) · [Next](27-string-library.md)
<!-- nav:end -->
