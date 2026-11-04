Test.section "container extensions"
b := Buffer.of_string "a\0bc"
Test.equal "sub binary" (Buffer.sub 1 2 b) (Some "\0b")
Test.equal "sub empty" (Buffer.sub 4 0 b) (Some "")
Test.equal "sub bounds" (Buffer.sub 0 9223372036854775807 b) None
Test.equal "sub negative" (Buffer.sub (~1) 1 b) None
Buffer.truncate 2 b
Test.equal "truncate" (Buffer.contents b) "a\0"
Buffer.add "x" b
Test.equal "append after truncate" (Buffer.hex b) "610078"
t := Table.of_list [("b", 2), ("a", 1)]
copy := Table.clone t
Test.check "insert existing" (not (Table.set_if_absent "a" 9 t))
Test.check "insert new" (Table.set_if_absent "c" 3 t)
Test.equal "values order" (Table.values t) [1, 2, 3]
Table.remove "a" t
Test.equal "clone independent" (Table.to_list copy) [("a", 1), ("b", 2)]
