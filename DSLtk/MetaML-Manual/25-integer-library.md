# 25. Int: bounded and checked arithmetic

Int provides named arithmetic, predicates, bounds, bit operations, conversion, and number-theory helpers. Its `t` alias is the builtin `Int`, not a distinct opaque numeric representation. Ordinary `add`, `sub`, `mul`, and negation wrap modulo 2^64; use checked operations when overflow is an expected input condition.

```metaml
Test.equal "checked success" (Int.checked_mul 6 7) (Some 42)
Test.equal "checked overflow" (Int.checked_mul Int.min_value (~1)) None
Test.equal "checked boundary" (Int.checked_sub Int.min_value 0) (Some Int.min_value)
Test.equal "gcd" (Int.gcd 84 30) 6
Test.equal "parse bases" (Int.from_string "0xff") (Some 255)
```

`checked_add`, `checked_sub`, and `checked_mul` return `None` if the mathematical result lies outside the signed 64-bit range. The implementations check bounds without first performing overflowing signed arithmetic. Multiplication accounts for the asymmetric range: the magnitude of the most negative integer is representable only for a negative result.

`from_string` accepts an optional sign, decimal/hex/binary prefixes, and separators between digits. It rejects whitespace, partial parses, malformed separators, and out-of-range input. `to_string` renders decimal. Source literals follow related rules but report diagnostics rather than returning Option.

`abs min_value` remains `min_value` under wrapping arithmetic. The same bounded-representation caveat affects extreme `gcd`/`lcm` results; these are not arbitrary-precision integers. `pow` requires a non-negative exponent. Bit shifts mask the shift count to six bits, so they operate modulo 64 rather than accepting unbounded distances.

For sizes, indices, and allocation calculations, checked arithmetic is usually preferable to wrapping arithmetic. Bounds checks must still be performed against the target collection's length.

**Source:** [`int/mod.ml`](../metalib/int/mod.ml). The public signature below is refreshed from that source.

<!-- api:begin -->
```text
  type t = Int
  val zero : t
  val one : t
  val add : t -> t -> t
  val sub : t -> t -> t
  val mul : t -> t -> t
  val checked_add : t -> t -> Option t
  val checked_sub : t -> t -> Option t
  val checked_mul : t -> t -> Option t
  val div : t -> t -> t
  val rem : t -> t -> t
  val neg : t -> t
  val abs : t -> t
  val pow : t -> t -> t
  val min : t -> t -> t
  val max : t -> t -> t
  val clamp : t -> t -> t -> t
  val compare : t -> t -> Int
  val equal : t -> t -> Bool
  val sign : t -> t
  val is_even : t -> Bool
  val is_odd : t -> Bool
  val gcd : t -> t -> t
  val lcm : t -> t -> t
  val land : t -> t -> t
  val lor : t -> t -> t
  val lxor : t -> t -> t
  val shift_left : t -> t -> t
  val shift_right : t -> t -> t
  val max_value : t
  val min_value : t
  val from_string : String -> Option t
  val to_string : t -> String
  val to_float : t -> Float
  val ( + ) : t -> t -> t
```
<!-- api:end -->

<!-- nav:begin -->
[Contents](README.md) · [Previous](24-core-and-prelude.md) · [Next](26-floating-library.md)
<!-- nav:end -->
