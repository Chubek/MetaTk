{- metalib/option — the Option monad.

   `type Option 'a := | None | Some 'a` is builtin. `get_or` is
   data-first (as in `Option.get_or($1, 0)`); `with_default` is its
   data-last, pipeline-friendly twin. -}

module type OPTION := sig
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
end

module Option :> OPTION := struct
  val some x := Some x
  val none := None
  val is_some o := match o with | Some _ -> true | None -> false
  val is_none o := match o with | Some _ -> false | None -> true
  val get o := match o with | Some x -> x | None -> Core.error("Option.get: None")
  val get_or o d := match o with | Some x -> x | None -> d
  val with_default d o := match o with | Some x -> x | None -> d
  val map f o := match o with | Some x -> Some (f x) | None -> None
  val flat_map f o := match o with | Some x -> f x | None -> None
  val bind o f := match o with | Some x -> f x | None -> None
  val filter p o := match o with | Some x -> if p x then o else None | None -> None
  val or_else o alt := match o with | Some _ -> o | None -> alt
  val to_list o := match o with | Some x -> [x] | None -> []
  val iter f o := match o with | Some x -> f x | None -> ()
  val map2 f a b := match (a, b) with | (Some x, Some y) -> Some (f x y) | _ -> None
end
