{- metalib/core — fundamental functions shared by every module.

   The builtin types `Int`, `Float`, `String`, `Bool`, `Unit`, `[a]`,
   `Option 'a` (None | Some 'a) and `Result 'a 'e` (Ok 'a | Err 'e) are
   created by every session before metalib loads; this module adds
   identity/composition helpers, structural equality and ordering, error
   reporting, and the `Test` harness used by every `test.ml`.

   Host primitives used: Core.show, Core.error, Core.compare, Core.equal,
   Core.hash, Core.ignore. -}

module type CORE := sig
  val id : 'a -> 'a
  val const : 'a -> 'b -> 'a
  val flip : ('a -> 'b -> 'c) -> 'b -> 'a -> 'c
  val compose : ('b -> 'c) -> ('a -> 'b) -> 'a -> 'c
  val apply : ('a -> 'b) -> 'a -> 'b
  val not : Bool -> Bool
  val equal : 'a -> 'a -> Bool
  val compare : 'a -> 'a -> Int
  val min : 'a -> 'a -> 'a
  val max : 'a -> 'a -> 'a
  val show : 'a -> String
  val hash : 'a -> Int
  val error : String -> 'a
  val undefined : Unit -> 'a
  val ignore : 'a -> Unit
  val fst : ('a, 'b) -> 'a
  val snd : ('a, 'b) -> 'b
  val swap : ('a, 'b) -> ('b, 'a)
  val curry : (('a, 'b) -> 'c) -> 'a -> 'b -> 'c
  val uncurry : ('a -> 'b -> 'c) -> ('a, 'b) -> 'c
end

module Core :> CORE := struct
  val id x := x
  val const x _ := x
  val flip f a b := f b a
  val compose f g x := f (g x)
  val apply f x := f x
  val not b := if b then false else true
  val equal := Core.equal($1, $2)
  val compare := Core.compare($1, $2)
  val min a b := if Core.compare(a, b) <= 0 then a else b
  val max a b := if Core.compare(a, b) >= 0 then a else b
  val show := Core.show($1)
  val hash := Core.hash($1)
  val error msg := Core.error(msg)
  val undefined _ := Core.error("undefined")
  val ignore := Core.ignore($1)
  val fst p := match p with | (a, _) -> a
  val snd p := match p with | (_, b) -> b
  val swap p := match p with | (a, b) -> (b, a)
  val curry f a b := f (a, b)
  val uncurry f p := match p with | (a, b) -> f a b
end

{- Test — a tiny deterministic test harness for metalib `test.ml` files.
   Every check prints one line: `ok - <name>` or `FAIL - <name>: ...`. -}
module type TEST := sig
  val check : String -> Bool -> Unit
  val equal : String -> 'a -> 'a -> Unit
  val section : String -> Unit
end

module Test :> TEST := struct
  val check name ok :=
    if ok then IO.println("ok - " ^ name) else IO.println("FAIL - " ^ name)
  val equal name actual expected :=
    if Core.equal(actual, expected) then IO.println("ok - " ^ name)
    else IO.println("FAIL - " ^ name ^ ": got " ^ Core.show(actual)
                    ^ ", expected " ^ Core.show(expected))
  val section title := IO.println("# " ^ title)
end
