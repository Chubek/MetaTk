{- Tests for metalib/int. -}
import "prelude"

Test.section "int"
Test.equal "zero/one" (Int.zero, Int.one) (0, 1)
Test.equal "add" (Int.add 2 3) 5
Test.equal "sub" (Int.sub 2 3) (~1)
Test.equal "mul" (Int.mul 6 7) 42
Test.equal "div truncates" (Int.div (~7) 2) (~3)
Test.equal "rem" (Int.rem (~7) 2) (~1)
Test.equal "neg" (Int.neg 5) (~5)
Test.equal "abs" (Int.abs (~5)) 5
Test.equal "pow" (Int.pow 2 10) 1024
Test.equal "min/max" (Int.min 1 2, Int.max 1 2) (1, 2)
Test.equal "clamp" (Int.clamp 0 10 42) 10
Test.equal "compare" [Int.compare 1 2, Int.compare 2 2, Int.compare 3 2] [~1, 0, 1]
Test.check "equal" (Int.equal 4 4)
Test.equal "sign" (Int.sign (~9)) (~1)
Test.check "even/odd" (Int.is_even 4 && Int.is_odd 5)
Test.equal "gcd" (Int.gcd 12 18) 6
Test.equal "lcm" (Int.lcm 4 6) 12
Test.equal "bits" (Int.land 12 10, Int.lor 12 10, Int.lxor 12 10) (8, 14, 6)
Test.equal "shifts" (Int.shift_left 1 4, Int.shift_right (~16) 2) (16, ~4)
Test.check "bounds" (Int.max_value > 0 && Int.min_value < 0)
Test.equal "wraps" (Int.max_value + 1) Int.min_value
Test.equal "from_string" (Int.from_string "0x2A") (Some 42)
Test.equal "from_string bad" (Int.from_string "4x") None
Test.equal "to_string" (Int.to_string (~12)) "-12"
Test.equal "to_float" (Int.to_float 3) 3.0
Test.equal "( + )" (Int.( + ) 1 2) 3
