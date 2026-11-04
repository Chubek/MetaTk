{- metalib/string — the String module.

   Strings are immutable byte sequences (UTF-8 by convention); indices
   and lengths count bytes. Data-last argument order makes the functions
   pipeline friendly: `s |> String.split ","`. Host primitives used:
   String.length, String.concat, String.sub, String.char_at,
   String.index_of, String.chars, String.join, String.split, String.trim,
   String.upper, String.lower, String.starts_with, String.ends_with,
   String.contains, String.replace, String.repeat, String.compare,
   String.to_codes, String.of_codes, Int.render, Float.to_string. -}

module type STRING := sig
  type t = String
  val empty : t
  val length : t -> Int
  val is_empty : t -> Bool
  val concat : t -> t -> t
  val sub : Int -> Int -> t -> t
  val get : Int -> t -> Option t
  val index_of : t -> t -> Option Int
  val chars : t -> [t]
  val join : t -> [t] -> t
  val split : t -> t -> [t]
  val lines : t -> [t]
  val trim : t -> t
  val upper : t -> t
  val lower : t -> t
  val starts_with : t -> t -> Bool
  val ends_with : t -> t -> Bool
  val contains : t -> t -> Bool
  val replace : t -> t -> t -> t
  val repeat : Int -> t -> t
  val reverse : t -> t
  val compare : t -> t -> Int
  val equal : t -> t -> Bool
  val to_codes : t -> [Int]
  val of_codes : [Int] -> t
  val of_int : Int -> t
  val of_float : Float -> t
  val to_int : t -> Option Int
end

module String :> STRING := struct
  type t = String
  val empty := ""
  val length := String.length($1)
  val is_empty := String.length($1) == 0
  val concat := $1 ^ $2
  val sub := String.sub($1, $2, $3)
  val get := String.char_at($1, $2)
  val index_of := String.index_of($1, $2)
  val chars := String.chars($1)
  val join := String.join($1, $2)
  val split := String.split($1, $2)
  val lines := String.split("\n", $1)
  val trim := String.trim($1)
  val upper := String.upper($1)
  val lower := String.lower($1)
  val starts_with := String.starts_with($1, $2)
  val ends_with := String.ends_with($1, $2)
  val contains := String.contains($1, $2)
  val replace := String.replace($1, $2, $3)
  val repeat := String.repeat($1, $2)
  val reverse s := String.join("", List.reverse(String.chars(s)))
  val compare := String.compare($1, $2)
  val equal := $1 == $2
  val to_codes := String.to_codes($1)
  val of_codes := String.of_codes($1)
  val of_int := Int.render($1)
  val of_float := Float.to_string($1)
  val to_int := Int.parse($1)
end
