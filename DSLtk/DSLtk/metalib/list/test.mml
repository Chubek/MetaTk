{- Tests for metalib/list. -}
import "prelude"

Test.section "list"
Test.equal "empty/singleton/cons" (List.cons 1 (List.singleton 2)) [1, 2]
Test.equal "length" (List.length [1, 2, 3]) 3
Test.check "is_empty" (List.is_empty List.empty)
Test.equal "head/tail" (List.head [1, 2], List.tail [1, 2]) (Some 1, Some [2])
Test.equal "last" (List.last [1, 2, 3]) (Some 3)
Test.equal "nth" (List.nth 1 ["a", "b"]) (Some "b")
Test.equal "reverse" (List.reverse [1, 2, 3]) [3, 2, 1]
Test.equal "append/concat" (List.append [1] [2], List.concat [[1], [2, 3]]) ([1, 2], [1, 2, 3])
Test.equal "map" (List.map (* 10) [1, 2]) [10, 20]
Test.equal "mapi" (List.mapi (fn i x -> i + x) [10, 20]) [10, 21]
Test.equal "filter" (List.filter (fn x -> x % 2 == 0) [1, 2, 3, 4]) [2, 4]
Test.equal "filter_map" (List.filter_map Int.from_string ["1", "x", "3"]) [1, 3]
Test.equal "flat_map" (List.flat_map (fn x -> [x, x]) [1, 2]) [1, 1, 2, 2]
Test.equal "fold_left" (List.fold_left (fn a x -> a - x) 10 [1, 2]) 7
Test.equal "fold_right" (List.fold_right (fn x a -> x :: a) [1, 2] []) [1, 2]
Test.check "any/all/exists" (List.any (> 2) [1, 3] && List.all (> 0) [1, 3] && List.exists (== 3) [3])
Test.equal "find" (List.find (> 1) [1, 2, 3]) (Some 2)
Test.check "mem" (List.mem 2 [1, 2])
Test.equal "count" (List.count (> 1) [1, 2, 3]) 2
Test.equal "partition" (List.partition (> 1) [1, 2, 3]) ([2, 3], [1])
Test.equal "take/drop" (List.take 2 [1, 2, 3], List.drop 2 [1, 2, 3]) ([1, 2], [3])
Test.equal "zip/unzip" (List.unzip (List.zip [1, 2] ["a", "b"])) ([1, 2], ["a", "b"])
Test.equal "enumerate" (List.enumerate ["a", "b"]) [(0, "a"), (1, "b")]
Test.equal "range" (List.range 1 4) [1, 2, 3]
Test.equal "replicate" (List.replicate 3 "x") ["x", "x", "x"]
Test.equal "sum/product" (List.sum [1, 2, 3], List.product [2.0, 4.0]) (6, 8.0)
Test.equal "sort" (List.sort [3, 1, 2]) [1, 2, 3]
Test.equal "sort_by" (List.sort_by (fn a b -> Core.compare b a) [3, 1, 2]) [3, 2, 1]
Test.equal "uniq" (List.uniq [1, 2, 1, 3, 2]) [1, 2, 3]
Test.equal "intersperse" (List.intersperse 0 [1, 2, 3]) [1, 0, 2, 0, 3]
Test.equal "maximum/minimum" (List.maximum [3, 9, 2], List.minimum [3, 9, 2]) (Some 9, Some 2)
Test.equal "long list" (List.length (List.map (fn x -> x + 1) (List.range 0 100000))) 100000
Test.equal "comprehension" [x * y for x <- [1, 2], y <- [10, 20] if x * y != 20] [10, 40]
