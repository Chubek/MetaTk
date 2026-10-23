{- metalib/float — the Float module (IEEE-754 binary64).

   `to_string` renders the shortest text that reads back as the same
   value (identical in every dialect). Host primitives used: Float.of_int,
   Float.to_string, Float.parse, Float.of_string, Float.truncate,
   Float.floor, Float.ceil, Float.round, Float.sqrt, Float.exp, Float.log,
   Float.sin, Float.cos, Float.tan, Float.abs, Float.pow, Float.atan2,
   Float.is_nan, Float.infinity, Float.nan. -}

module type FLOAT := sig
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
end

module Float :> FLOAT := struct
  type t = Float
  val zero := 0.0
  val one := 1.0
  val pi := 3.141592653589793
  val e := 2.718281828459045
  val infinity := Float.infinity()
  val nan := Float.nan()
  val add := $1 + $2
  val sub := $1 - $2
  val mul := $1 * $2
  val div := $1 / $2
  val neg := 0.0 - $1
  val abs := Float.abs($1)
  val min a b := if a <= b then a else b
  val max a b := if a >= b then a else b
  val compare a b := if a < b then ~1 else if a > b then 1 else 0
  val floor := Float.floor($1)
  val ceil := Float.ceil($1)
  val round := Float.round($1)
  val truncate := Float.truncate($1)
  val to_int := Float.truncate($1)
  val sqrt := Float.sqrt($1)
  val exp := Float.exp($1)
  val log := Float.log($1)
  val sin := Float.sin($1)
  val cos := Float.cos($1)
  val tan := Float.tan($1)
  val pow := Float.pow($1, $2)
  val atan2 := Float.atan2($1, $2)
  val is_nan := Float.is_nan($1)
  val is_finite := Float.is_finite($1)
  val is_infinite := Float.is_infinite($1)
  val of_int := Float.of_int($1)
  val of_string := Float.of_string($1)
  val from_string := Float.parse($1)
  val to_string := Float.to_string($1)
end
