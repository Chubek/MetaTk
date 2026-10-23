-- Host modules: JSON, SExpr, TOML, YAML
Test.section "JSON"
j := JSON.parse "{\"a\": [1, 2.5, true, null], \"b\": \"x\\ny\", \"c\": {}}"
Test.check "parse ok" (match j with | Ok _ -> true | Err _ -> false)
jv := match j with | Ok v -> v | Err _ -> JNull
Test.equal "render" (JSON.render jv) "{\"a\":[1,2.5,true,null],\"b\":\"x\\ny\",\"c\":{}}"
Test.equal "get" (JSON.get "b" jv) (Some (JStr "x\ny"))
Test.equal "at" (JSON.get "a" jv |> Option.flat_map (JSON.at 1)) (Some (JNum 2.5))
Test.equal "bad" (JSON.parse "[1,") (Err "JSON: unexpected end of input at offset 3")
Test.equal "of_value" (JSON.render (JSON.of_value {x = 1, y = ["a"]})) "{\"x\":1,\"y\":[\"a\"]}"
IO.println (JSON.pretty jv)
IO.println (JSON.render (JSON.of_value (Some (1, 2.0))))
IO.println (JSON.render (match JSON.parse "\"\\u00e9\\ud83d\\ude00\"" with | Ok v -> v | Err e -> JStr e))
IO.println (JSON.render (JNum 1e300))
IO.println (match JSON.parse "{\"a\":1 \"b\":2}" with | Ok _ -> "ok" | Err e -> e)

Test.section "SExpr"
Test.equal "parse_one" (SExpr.parse_one "(a (b c) \"d e\")") (Ok (SList [Atom "a", SList [Atom "b", Atom "c"], Atom "d e"]))
IO.println (match SExpr.parse "(x) y (z (w))" with | Ok xs -> List.map SExpr.render xs |> String.join " | " | Err e -> e)
IO.println (match SExpr.parse_one "(a" with | Ok _ -> "ok" | Err e -> e)

Test.section "TOML"
toml_src := "title = \"T\"\n[owner]\nname = \"Tom\" # c\nage = 42\n[[items]]\nid = 1\n[[items]]\nid = 2\ntags = [\"a\", \"b\"]\npi = 3.14\nok = true\n"
IO.println (match TOML.parse toml_src with | Ok v -> JSON.render v | Err e -> e)
IO.println (match TOML.parse toml_src with | Ok v -> (match TOML.render v with | Ok t -> t | Err e -> e) | Err e -> e)
IO.println (match TOML.parse "a = 1\na = 2\n" with | Ok _ -> "ok" | Err e -> e)
IO.println (match TOML.parse "x = \n" with | Ok _ -> "ok" | Err e -> e)
IO.println (match TOML.parse "s = '''\nhi\nthere'''\nt = \"\\u00e9\"\nk.l = 1_000\n" with | Ok v -> JSON.render v | Err e -> e)

Test.section "YAML"
ysrc := "name: demo\nlist:\n  - 1\n  - two\n  - {a: 1, b: [x, y]}\nnested:\n  k: v # comment\n  e: \"q\\tx\"\n  n: ~\ntext: |\n  line1\n  line2\nfold: >-\n  a\n  b\n"
IO.println (match YAML.parse ysrc with | Ok v -> JSON.render v | Err e -> e)
IO.println (match YAML.parse ysrc with | Ok v -> YAML.render v | Err e -> e)
IO.println (match YAML.parse "- a\n- - b\n  - c\n- k: 1\n  j: 2\n" with | Ok v -> JSON.render v | Err e -> e)
IO.println (match YAML.parse "a: [1, 2\n" with | Ok _ -> "ok" | Err e -> e)
IO.println (YAML.render (JSON.of_value {a = [1, 2], b = "has: colon", c = ""}))
