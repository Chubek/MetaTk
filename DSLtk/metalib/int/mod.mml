{- metalib/int — the Int module (64-bit two's-complement integers).

   Arithmetic wraps on overflow; division truncates toward zero and
   division by zero raises M0402. Host primitives used: Int.parse,
   Int.render, Int.to_float, Int.abs, Int.pow, Int.land, Int.lor,
   Int.lxor, Int.shift_left, Int.shift_right, Int.min_value,
   Int.max_value. -}

module type INT := sig
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
end

module Int :> INT := struct
  type t = Int
  val zero := 0
  val one := 1
  val add := $1 + $2
  val sub := $1 - $2
  val mul := $1 * $2
  val checked_add := Int.checked_add($1, $2)
  val checked_sub := Int.checked_sub($1, $2)
  val checked_mul := Int.checked_mul($1, $2)
  val div := $1 / $2
  val rem := $1 % $2
  val neg := 0 - $1
  val abs := Int.abs($1)
  val pow := Int.pow($1, $2)
  val min a b := if a <= b then a else b
  val max a b := if a >= b then a else b
  val clamp lo hi x := if x < lo then lo else if x > hi then hi else x
  val compare a b := if a < b then ~1 else if a > b then 1 else 0
  val equal := $1 == $2
  val sign x := if x < 0 then ~1 else if x > 0 then 1 else 0
  val is_even := $1 % 2 == 0
  val is_odd := $1 % 2 != 0
  val gcd a b := if b == 0 then Int.abs(a) else gcd b (a % b)
  val lcm a b := if a == 0 || b == 0 then 0 else Int.abs(a / gcd a b * b)
  val land := Int.land($1, $2)
  val lor := Int.lor($1, $2)
  val lxor := Int.lxor($1, $2)
  val shift_left := Int.shift_left($1, $2)
  val shift_right := Int.shift_right($1, $2)
  val max_value := Int.max_value()
  val min_value := Int.min_value()
  val from_string s := Int.parse(s)
  val to_string i := Int.render(i)
  val to_float := Int.to_float($1)
  val ( + ) := $1 + $2
end
