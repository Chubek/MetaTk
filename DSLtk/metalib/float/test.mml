{- Tests for metalib/float. -}
import "prelude"

Test.section "float"
Test.equal "add" (Float.add 0.1 0.2) 0.30000000000000004
Test.equal "div" (Float.div 1.0 4.0) 0.25
Test.equal "neg/abs" (Float.abs (Float.neg 2.5)) 2.5
Test.equal "floor/ceil/round" (Float.floor 2.7, Float.ceil 2.1, Float.round 2.5) (2.0, 3.0, 3.0)
Test.equal "truncate" (Float.truncate (~2.9)) (~2)
Test.equal "sqrt" (Float.sqrt 16.0) 4.0
Test.equal "pow" (Float.pow 2.0 0.5) (Float.sqrt 2.0)
Test.check "pi" (Float.abs (Float.sin Float.pi) < 1e-12)
Test.check "nan" (Float.is_nan Float.nan && Float.nan != Float.nan)
Test.check "infinity" (Float.infinity > 1e308)
Test.equal "compare" (Float.compare 1.5 0.5) 1
Test.equal "of_int" (Float.of_int 7) 7.0
Test.equal "of_string" (Float.of_string "42") 42.0
Test.equal "from_string" (Float.from_string "x") None
Test.equal "to_string" [Float.to_string 2.0, Float.to_string 0.1, Float.to_string 1e21, Float.to_string 1.5e-7] ["2.0", "0.1", "1e21", "1.5e-7"]
Test.equal "min/max" (Float.min 1.0 2.0, Float.max 1.0 2.0) (1.0, 2.0)
