{- Tests for metalib/core. -}
import "prelude"

Test.section "core"
Test.equal "id" (Core.id 5) 5
Test.equal "const" (Core.const 1 2) 1
Test.equal "flip" (Core.flip (fn a b -> a - b) 1 10) 9
Test.equal "compose" (Core.compose (fn x -> x * 2) (fn x -> x + 1) 3) 8
Test.equal "apply" (Core.apply (fn x -> x + 1) 1) 2
Test.check "not" (Core.not false)
Test.check "equal" (Core.equal [1, 2] [1, 2])
Test.equal "compare" (Core.compare "a" "b") (~1)
Test.equal "min" (Core.min 3 2) 2
Test.equal "max" (Core.max 3 2) 3
Test.equal "show" (Core.show (Some [1, 2])) "Some [1, 2]"
Test.check "hash" (Core.hash "x" == Core.hash "x")
Test.equal "fst" (Core.fst (1, "a")) 1
Test.equal "snd" (Core.snd (1, "a")) "a"
Test.equal "swap" (Core.swap (1, "a")) ("a", 1)
Test.equal "curry" (Core.curry (fn p -> Core.fst p + Core.snd p) 1 2) 3
Test.equal "uncurry" (Core.uncurry (fn a b -> a * b) (3, 4)) 12
Core.ignore 42
