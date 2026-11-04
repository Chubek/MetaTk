{- Tests for metalib/option. -}
import "prelude"

Test.section "option"
Test.equal "some/none" (Option.some 1, Option.none) (Some 1, None)
Test.check "is_some/is_none" (Option.is_some (Some 1) && Option.is_none None)
Test.equal "get" (Option.get (Some "x")) "x"
Test.equal "get_or" (Option.get_or None 0, Option.get_or (Some 5) 0) (0, 5)
Test.equal "with_default" (None |> Option.with_default 7) 7
Test.equal "map" (Option.map (* 2) (Some 21)) (Some 42)
Test.equal "flat_map" (Option.flat_map Int.from_string (Some "12")) (Some 12)
Test.equal "bind" (Option.bind (Some 2) (fn x -> if x > 1 then Some x else None)) (Some 2)
Test.equal "filter" (Option.filter (> 5) (Some 3)) None
Test.equal "or_else" (Option.or_else None (Some 1)) (Some 1)
Test.equal "to_list" (Option.to_list (Some 1)) [1]
Option.iter (fn x -> Test.equal "iter" x 9) (Some 9)
Test.equal "map2" (Option.map2 (+) (Some 1) (Some 2)) (Some 3)
