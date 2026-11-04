{- Tests for metalib/result. -}
import "prelude"

Test.section "result"
Test.equal "ok/err" [Result.ok 1, Result.err "e"] [Ok 1, Err "e"]
Test.check "is_ok/is_err" (Result.is_ok (Ok 1) && Result.is_err (Err 2))
Test.equal "get_or" (Result.get_or (Err "x") 0) 0
Test.equal "map" (Result.map (+ 1) (Ok 1)) (Ok 2)
Test.equal "map_err" (Result.map_err String.length (Err "abc")) (Err 3)
Test.equal "and_then" (Ok "5" |> Result.and_then (fn s -> Result.from_option "bad" (Int.from_string s))) (Ok 5)
Test.equal "to_option" (Result.to_option (Err 1)) None
Test.equal "from_option" (Result.from_option "none" None) (Err "none")
Test.equal "unwrap" (Result.unwrap (Ok 3)) 3
