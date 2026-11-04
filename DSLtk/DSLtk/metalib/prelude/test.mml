{- Tests for metalib/prelude. -}
import "prelude"

Test.section "prelude"
print "hello from print"
show_line (1, "two")
Test.equal "sum" (sum [1.0, 2.0, 3.0]) 6.0
Test.equal "product" (product [2, 3]) 6
Test.equal "length" (length [1, 2]) 2
Test.equal "map/filter" (map (+ 1) (filter (> 1) [1, 2, 3])) [3, 4]
Test.equal "fold" (fold (+) 0 [1, 2, 3]) 6
Test.equal "reverse" (reverse [1, 2]) [2, 1]
Test.equal "range" (range 0 3) [0, 1, 2]
Test.equal "concat" (concat ["a", "b"]) "ab"
Test.equal "opened core" (id 3, not true, fst (1, 2)) (3, false, 1)
Test.equal "pipeline" ([1, 2, 3] |> map (* 2) |> sum) 12
Test.equal "compose" (((fn x -> x + 1) >> (fn x -> x * 10)) 1) 20
Test.equal "compose back" (((fn x -> x + 1) << (fn x -> x * 10)) 1) 11
