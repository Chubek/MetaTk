{- Tests for metalib/string. -}
import "prelude"

Test.section "string"
Test.equal "length" (String.length "hello") 5
Test.check "is_empty" (String.is_empty String.empty)
Test.equal "concat" (String.concat "ab" "cd") "abcd"
Test.equal "sub" (String.sub 1 3 "abcdef") "bcd"
Test.equal "sub clamps" (String.sub 4 10 "abcdef") "ef"
Test.equal "get" (String.get 1 "abc", String.get 9 "abc") (Some "b", None)
Test.equal "index_of" (String.index_of "lo" "hello") (Some 3)
Test.equal "chars" (String.chars "abc") ["a", "b", "c"]
Test.equal "join" (String.join ", " ["a", "b"]) "a, b"
Test.equal "split" ("a,b,,c" |> String.split ",") ["a", "b", "", "c"]
Test.equal "lines" (String.lines "x\ny") ["x", "y"]
Test.equal "trim" (String.trim "  hi \n") "hi"
Test.equal "upper/lower" (String.upper "aB", String.lower "aB") ("AB", "ab")
Test.check "starts/ends" (String.starts_with "he" "hello" && String.ends_with "lo" "hello")
Test.check "contains" (String.contains "ell" "hello")
Test.equal "replace" (String.replace "l" "L" "hello") "heLLo"
Test.equal "repeat" (String.repeat 3 "ab") "ababab"
Test.equal "reverse" (String.reverse "abc") "cba"
Test.equal "compare" (String.compare "b" "a") 1
Test.check "equal" (String.equal "x" "x")
Test.equal "codes" (String.of_codes (String.to_codes "Hi")) "Hi"
Test.equal "of_int/of_float" (String.of_int 42 ^ "/" ^ String.of_float 2.5) "42/2.5"
Test.equal "to_int" (String.to_int "17") (Some 17)
Test.equal "escapes" (String.length "\u{1F600}\t") 5
