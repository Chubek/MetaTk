{- metalib/result — the Result monad.

   `type Result 'a 'e := | Ok 'a | Err 'e` is builtin. Errors carry any
   type; host primitives conventionally use `Result 'a String`. -}

module type RESULT := sig
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
end

module Result :> RESULT := struct
  val ok x := Ok x
  val err e := Err e
  val is_ok r := match r with | Ok _ -> true | Err _ -> false
  val is_err r := match r with | Ok _ -> false | Err _ -> true
  val get_or r d := match r with | Ok x -> x | Err _ -> d
  val map f r := match r with | Ok x -> Ok (f x) | Err e -> Err e
  val map_err f r := match r with | Ok x -> Ok x | Err e -> Err (f e)
  val and_then f r := match r with | Ok x -> f x | Err e -> Err e
  val to_option r := match r with | Ok x -> Some x | Err _ -> None
  val from_option e o := match o with | Some x -> Ok x | None -> Err e
  val unwrap r := match r with
    | Ok x -> x
    | Err e -> Core.error("Result.unwrap: Err " ^ Core.show(e))
  val map2 f a b := match a with
    | Err e -> Err e
    | Ok x -> match b with | Err e -> Err e | Ok y -> Ok (f x y)
  val flatten r := and_then (fn x -> x) r
  val traverse f xs :=
    let rec go ys acc := match ys with
      | [] -> Ok (List.reverse(acc))
      | y :: rest -> match f y with | Err e -> Err e | Ok z -> go rest (z :: acc) in
    go xs []
  val sequence xs := traverse (fn x -> x) xs
end
