-- Host modules: String, List, Hash, IO, Bridge, Buffer, Table, URI
Test.section "String (host)"
Test.equal "utf8_length" (String.utf8_length "h\u{e9}llo \u{1F600}") 7
Test.equal "utf8_chars" (String.utf8_chars "a\u{e9}b") ["a", "\u{e9}", "b"]
Test.check "utf8_valid" (String.utf8_valid "ok\u{e9}" && not (String.utf8_valid (String.of_codes [255])))
Test.equal "codepoints" (String.codepoints "A\u{3b1}") [65, 945]
Test.equal "of_codepoints" (String.of_codepoints [72, 105, ~1]) "Hi\u{FFFD}"
Test.equal "pad" (String.pad_left 5 "0" "42", String.pad_right 4 ".-" "x") ("00042", "x.-.")
Test.equal "words" (String.words "  a  bc\td \n") ["a", "bc", "d"]
Test.equal "last_index_of" (String.last_index_of "a" "banana") (Some 5)
Test.equal "count" (String.count "an" "banana") 2
Test.equal "strip" (String.strip_prefix "ba" "banana", String.strip_suffix "x" "banana") (Some "nana", None)
Test.equal "capitalize" (String.capitalize "hello") "Hello"
Test.check "classes" (String.is_digit "123" && String.is_alpha "abC" && String.is_space " \t" && String.is_alnum "a1" && not (String.is_digit ""))
Test.equal "format" (String.format "{} + {} = {{{}}}" ["1", "2", "3"]) "1 + 2 = {3}"
Test.equal "escape" (String.escape "a\"b\n") "a\\\"b\\n"
Test.equal "compare_ci" (String.compare_ci "ABC" "abd") (~1)

Test.section "List (host)"
Test.equal "take_while" (List.take_while (< 3) [1, 2, 3, 1]) [1, 2]
Test.equal "drop_while" (List.drop_while (< 3) [1, 2, 3, 1]) [3, 1]
Test.equal "chunks" (List.chunks 2 [1, 2, 3, 4, 5]) [[1, 2], [3, 4], [5]]
Test.equal "windows" (List.windows 2 [1, 2, 3]) [[1, 2], [2, 3]]
Test.equal "group" (List.group [1, 1, 2, 1]) [[1, 1], [2], [1]]
Test.equal "flatten" (List.flatten [[1], [], [2, 3]]) [1, 2, 3]
Test.equal "index_of" (List.index_of "b" ["a", "b"]) (Some 1)
Test.equal "transpose" (List.transpose [[1, 2, 3], [4, 5, 6]]) [[1, 4], [2, 5], [3, 6]]
Test.equal "scan" (List.scan (+) 0 [1, 2, 3]) [0, 1, 3, 6]
Test.equal "iterate" (List.iterate 4 (* 2) 1) [1, 2, 4, 8]
Test.equal "zip3" (List.zip3 [1, 2] ["a", "b"] [true, false]) [(1, "a", true), (2, "b", false)]
Test.equal "rotate" (List.rotate 1 [1, 2, 3], List.rotate (~1) [1, 2, 3]) ([2, 3, 1], [3, 1, 2])
Test.equal "shuffle" (List.shuffle 42 [1, 2, 3, 4, 5, 6, 7, 8]) (List.shuffle 42 [1, 2, 3, 4, 5, 6, 7, 8])
show_line (List.shuffle 7 [1, 2, 3, 4, 5, 6, 7, 8])
Test.equal "max_by/min_by" (List.max_by Core.compare [3, 9, 2], List.min_by Core.compare [3, 9, 2]) (Some 9, Some 2)

Test.section "Hash"
Test.equal "fnv1a" (Hash.hex (Hash.fnv1a "hello")) "a430d84680aabd0b"
Test.equal "djb2" (Hash.djb2 "a") 177670
Test.check "value" (Hash.value [1, 2] == Hash.value [1, 2])
show_line (Hash.combine 1 2)
Test.equal "crc32" (Hash.crc32 "123456789") 3421780262
Test.equal "sha256" (Hash.sha256 "abc") "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
Test.equal "sha256 empty" (Hash.sha256 "") "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
Test.equal "hex" (Hash.hex 255) "00000000000000ff"

Test.section "IO (host)"
IO.printf "{}-{}\n" ["a", "b"]
IO.print_lines ["l1", "l2"]
IO.flush ()

Test.section "Bridge"
Test.equal "kind" (map Bridge.kind [Bridge.encode 1]) ["String"]
Test.equal "kinds" (Bridge.kind 1, Bridge.kind 1.5, Bridge.kind (fn x -> x), Bridge.kind { a = 1 }) ("Int", "Float", "Function", "Record")
Test.equal "encode" (Bridge.encode (Some "x")) "Some \"x\""
Test.equal "fields" (Bridge.fields { b = 1, a = 2 }) ["a", "b"]
Test.equal "ctor_name" (Bridge.ctor_name (Ok 1), Bridge.ctor_name 5) (Some "Ok", None)
Test.equal "arity/size" (Bridge.arity (1, 2, 3), Bridge.size [1, 2]) (3, 3)
xs := [1, 2]
Test.check "same" (Bridge.same xs xs && not (Bridge.same [1] [1]))

Test.section "Buffer"
b := Buffer.create ()
Buffer.add "Hi" b
Buffer.add_byte 33 b
Buffer.add_u16le 258 b
Buffer.add_u32be 1 b
Test.equal "length" (Buffer.length b) 9
Test.equal "hex" (Buffer.hex b) "486921020100000001"
Test.equal "byte_at" (Buffer.byte_at 0 b, Buffer.byte_at 99 b) (Some 72, None)
Buffer.clear b
Buffer.add_line "x" b
Test.equal "contents" (Buffer.contents b) "x\n"

Test.section "Table"
t := Table.create ()
Table.set "b" 2 t
Table.set "a" 1 t
Table.set "b" 3 t
Test.equal "get" (Table.get "b" t, Table.get "z" t) (Some 3, None)
Test.check "has" (Table.has "a" t)
Test.equal "keys" (Table.keys t) ["a", "b"]
Test.equal "to_list" (Table.to_list t) [("a", 1), ("b", 3)]
Table.remove "a" t
Test.equal "size" (Table.size t) 1
Test.equal "of_list" (Table.to_list (Table.of_list [("y", "1"), ("x", "2")])) [("x", "2"), ("y", "1")]

Test.section "URI"
u := Option.get (URI.parse "HTTP://bob@Example.com:8080/a/b?x=1&y=a+b#frag")
Test.equal "fields" (u.scheme, u.user, u.host, u.port, u.path, u.query, u.fragment) ("http", "bob", "example.com", 8080, "/a/b", "x=1&y=a+b", "frag")
Test.equal "render" (URI.render u) "http://bob@example.com:8080/a/b?x=1&y=a+b#frag"
Test.equal "relative" (Option.map (fn r -> (r.scheme, r.path)) (URI.parse "docs/x.html")) (Some ("", "docs/x.html"))
Test.equal "bad port" (URI.parse "http://h:99999/") None
Test.equal "encode" (URI.encode "a b/\u{e9}") "a%20b%2F%C3%A9"
Test.equal "decode" (URI.decode "a%20b%2f", URI.decode "%zz") (Some "a b/", None)
Test.equal "query_params" (URI.query_params u.query) [("x", "1"), ("y", "a b")]
