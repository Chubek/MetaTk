-- Host modules: ASTBuilder, VisitorBuilder, WalkAST, WalkVisitor, Graph,
-- FlexBridge, YACCBridge, Parsec, Frontend, Loader, Module, Type
Test.section "ASTBuilder"
t := Node "add" [Leaf "num" "1", Node "mul" [Leaf "num" "2", Leaf "var" "x"]]
Test.equal "dump" (ASTBuilder.dump t) "(add (num 1) (mul (num 2) (var x)))"
Test.equal "size/depth" (ASTBuilder.size t, ASTBuilder.depth t) (5, 3)
Test.equal "tag/text" (ASTBuilder.tag t, ASTBuilder.text (Leaf "a" "b")) ("add", Some "b")
Test.equal "find" (List.length (ASTBuilder.find "num" t)) 2
Test.equal "sexpr roundtrip" (ASTBuilder.of_sexpr (ASTBuilder.to_sexpr t)) (Ok t)
Test.equal "parse" (ASTBuilder.parse "(f (x \"1\") (g))") (Ok (Node "f" [Leaf "x" "1", Node "g" []]))
IO.print (ASTBuilder.pretty t)
IO.println (match ASTBuilder.parse "x" with | Ok _ -> "ok" | Err e -> e)

Test.section "VisitorBuilder"
eval_ast := VisitorBuilder.make (fn tag text -> if tag == "num" then Int.parse text |> Option.with_default 0 else 10) (fn tag xs -> if tag == "add" then List.sum xs else List.product xs)
Test.equal "make" (eval_ast t) 21
Test.equal "fold" (VisitorBuilder.fold (fn _ _ -> 1) (fn _ xs -> 1 + List.sum xs) t) 5
Test.equal "count_tags" (VisitorBuilder.count_tags t) [("add", 1), ("mul", 1), ("num", 2), ("var", 1)]
Test.equal "tags" (VisitorBuilder.tags t) ["add", "num", "mul", "var"]
Test.equal "dispatch" (VisitorBuilder.dispatch [("add", fn _ -> "plus")] (fn a -> ASTBuilder.tag a) t) "plus"

Test.section "WalkAST"
Test.equal "preorder" (List.map ASTBuilder.tag (WalkAST.preorder t)) ["add", "num", "mul", "num", "var"]
Test.equal "postorder" (List.map ASTBuilder.tag (WalkAST.postorder t)) ["num", "num", "var", "mul", "add"]
Test.equal "bfs" (List.map ASTBuilder.tag (WalkAST.breadth_first t)) ["add", "num", "mul", "num", "var"]
Test.equal "leaves" (List.length (WalkAST.leaves t)) 3
fold_consts n := match n with
  | Node "mul" [Leaf "num" a, Leaf "num" b] -> Some (Leaf "num" (Int.to_string ((Int.parse a |> Option.with_default 0) * (Int.parse b |> Option.with_default 0))))
  | Node "var" _ -> None
  | Leaf "var" _ -> Some (Leaf "num" "3")
  | _ -> None
Test.equal "rewrite" (ASTBuilder.dump (WalkAST.rewrite fold_consts t)) "(add (num 1) (num 6))"
Test.equal "map_bottom_up" (ASTBuilder.dump (WalkAST.map_bottom_up (fn n -> match n with | Leaf _ v -> Leaf "x" v | _ -> n) t)) "(add (x 1) (mul (x 2) (x x)))"
Test.equal "find_all" (List.length (WalkAST.find_all ASTBuilder.is_leaf t)) 3

Test.section "WalkVisitor"
WalkVisitor.walk_depth (fn d n -> IO.println (String.repeat d "  " ^ ASTBuilder.tag n)) t
Test.equal "path_to" (WalkVisitor.path_to (fn n -> ASTBuilder.text n == Some "x") t) (Some [1, 1])
Test.equal "at_path" (WalkVisitor.at_path [1, 0] t) (Some (Leaf "num" "2"))
WalkVisitor.enter_leave (fn n -> IO.print ("<" ^ ASTBuilder.tag n)) (fn _ -> IO.print ">") t
IO.println ""

Test.section "Graph"
es := [("a", "b"), ("b", "c"), ("a", "c"), ("d", "a")]
Test.equal "topo" (Graph.topo_sort [] es) (Ok ["d", "a", "b", "c"])
Test.equal "cycle" (Graph.topo_sort [] [("x", "y"), ("y", "x")]) (Err "Graph.topo_sort: the graph contains a cycle")
Test.equal "reachable" (Graph.reachable "a" es) ["a", "b", "c"]
Test.equal "shortest" (Graph.shortest_path "d" "c" es, Graph.shortest_path "c" "d" es) (Some ["d", "a", "c"], None)
IO.println (Core.show (Graph.sccs ["z"] [("x", "y"), ("y", "x"), ("y", "w")]))
Test.check "has_cycle" (Graph.has_cycle [("x", "x")] && not (Graph.has_cycle es))
IO.print (Graph.to_dot "deps" [] [("a", "b")])

Test.section "FlexBridge"
rules := [("num", "[0-9]+"), ("id", "[a-z_][a-z0-9_]*"), ("op", "[-+*/=]"), ("lp", "\\("), ("rp", "\\)"), ("_ws", "[ \\t\\n]+"), ("let", "let")]
Test.equal "tokenize" (FlexBridge.tokenize rules "let x1 = (42+y)") (Ok [("id", "let"), ("id", "x1"), ("op", "="), ("lp", "("), ("num", "42"), ("op", "+"), ("id", "y"), ("rp", ")")])
IO.println (match FlexBridge.tokenize rules "a\n  $" with | Ok _ -> "ok" | Err e -> e)
Test.equal "spans" (FlexBridge.tokenize_spans rules "a 12") (Ok [("id", "a", 0), ("num", "12", 2)])
Test.equal "valid" (FlexBridge.valid_pattern "[a-", FlexBridge.valid_pattern "+x", FlexBridge.valid_pattern "\\d+") (Err "unterminated character class", Err "quantifier `+` has nothing to repeat", Ok ())
Test.check "matches" (FlexBridge.matches "\\d+\\.\\d+" "3.14" && not (FlexBridge.matches "\\d+" "3a"))
Test.equal "prefix" (FlexBridge.match_prefix "[a-c]*" "abcd") (Some "abc")

Test.section "YACCBridge"
g := "expr : expr \"+\" term | term ;\nterm : term \"*\" atom | atom ;\natom : NUM | ID | \"(\" expr \")\" ;\n"
Test.equal "check" (YACCBridge.check g) (Ok ["expr", "term", "atom"])
IO.println (match YACCBridge.parse g "1 + 2 * (x + 3)" with | Ok a -> ASTBuilder.dump a | Err e -> e)
IO.println (match YACCBridge.parse g "1 + * 2" with | Ok a -> ASTBuilder.dump a | Err e -> e)
IO.println (match YACCBridge.parse g "1 +" with | Ok a -> ASTBuilder.dump a | Err e -> e)
IO.println (match YACCBridge.parse_tokens g [("NUM", "7"), ("OP", "*"), ("ID", "k")] with | Ok a -> ASTBuilder.dump a | Err e -> e)
IO.println (match YACCBridge.check "a : b" with | Ok _ -> "ok" | Err e -> e)
IO.println (match YACCBridge.parse "list : | list ID ;" "a b c" with | Ok a -> ASTBuilder.dump a | Err e -> e)

Test.section "Parsec"
digit := Parsec.satisfy String.is_digit
number := Parsec.token (Parsec.map (fn ds -> String.join "" ds |> Int.parse |> Option.with_default 0) (Parsec.many1 digit))
sym s := Parsec.token (Parsec.string s)
expr := Parsec.lazy (fn _ -> Parsec.map (fn p -> List.sum (fst p :: snd p)) (Parsec.seq term (Parsec.many (Parsec.right (sym "+") term))))
term := Parsec.alt number (Parsec.between (sym "(") (sym ")") expr)
Test.equal "run" (Parsec.run (Parsec.right (Parsec.spaces ()) expr) " 1 + (2 + 3) + 4") (Ok 10)
IO.println (match Parsec.run expr "1 + (2 +" with | Ok n -> Int.to_string n | Err e -> e)
IO.println (match Parsec.run expr "1 2" with | Ok n -> Int.to_string n | Err e -> e)
IO.println (match Parsec.run (Parsec.label "a number" number) "x" with | Ok n -> Int.to_string n | Err e -> e)
Test.equal "sep_by" (Parsec.run (Parsec.sep_by (Parsec.regex "[a-z]+") (Parsec.char ",")) "ab,c,def") (Ok ["ab", "c", "def"])
Test.equal "optional" (Parsec.run (Parsec.seq (Parsec.optional (Parsec.char "-")) (Parsec.regex "\\d+")) "-12") (Ok (Some "-", "12"))
Test.equal "prefix" (Parsec.run_prefix (Parsec.string "ab") "abc") (Ok ("ab", "c"))
Test.equal "bind" (Parsec.run (Parsec.bind (Parsec.any ()) (fn c -> Parsec.string c)) "xx") (Ok "x")

Test.section "Frontend"
IO.println (Core.show (Frontend.tokens "f x := x + 1 -- hi"))
IO.println (match Frontend.parse "x := 1 + 2" with | Ok s -> s | Err e -> e)
IO.println (match Frontend.parse "x := (1 +" with | Ok s -> s | Err e -> e)
Test.equal "type_of" (Frontend.type_of "fn x -> x + 1") (Ok "Int -> Int")
IO.println (match Frontend.type_of "1 + \"a\"" with | Ok s -> s | Err e -> e)
Test.equal "format_type" (Frontend.format_type "('x -> 'y) -> ['x] -> ['y]") (Ok "('a -> 'b) -> ['a] -> ['b]")
Test.equal "eval" (Frontend.eval "f x := x * 2\nf 21") (Ok "42")
IO.println (match Frontend.eval "nope 1" with | Ok s -> s | Err e -> e)
Test.equal "parse_expr" (Frontend.parse_expr "1 + x") (Ok "(binop (op +) (int 1) (var x))")

Test.section "Loader"
Test.check "stdlib" (Option.is_some (Loader.stdlib ()))
Test.check "resolve" (Option.is_some (Loader.resolve "list") && Option.is_none (Loader.resolve "no_such_module_xyz"))
IO.println (match Loader.load "no/such/file.ml" with | Ok _ -> "ok" | Err e -> e)

Test.section "Module"
Test.check "exists" (Module.exists "List" && not (Module.exists "Nope"))
Test.check "members" (match Module.members "Option" with | Some ms -> List.mem "map" ms | None -> false)
Test.equal "member_type" (Module.member_type "Option" "map") (Some "('a -> 'b) -> Option 'a -> Option 'b")
module Demo := struct
  type Shape := | Circle Float | Square Float
  val area s := match s with | Circle r -> 3.0 * r * r | Square a -> a * a
  module Inner := struct
    val k := 1
  end
end
IO.print (Option.with_default "" (Module.signature "Demo"))
Test.equal "submodules" (Module.submodules "Demo") (Some ["Inner"])

Test.section "Type"
Test.equal "normalize" (Type.normalize "'z -> ('q, 'z)") (Ok "'a -> ('b, 'a)")
Test.equal "unify" (Type.unify "'a -> Int" "String -> 'b") (Ok "String -> Int")
IO.println (match Type.unify "Int" "String" with | Ok s -> s | Err e -> e)
Test.check "instance_of" (Type.instance_of "Int -> Int" "'a -> 'a" && not (Type.instance_of "'a -> 'a" "Int -> Int") && not (Type.instance_of "Int -> String" "'a -> 'a"))
Test.check "equal" (Type.equal "'x -> 'y" "'a -> 'b" && not (Type.equal "'a -> 'a" "'a -> 'b"))
Test.equal "arity" (Type.arity "Int -> String -> Bool") (Ok 2)
IO.println (match Type.normalize "Foo Int" with | Ok s -> s | Err e -> e)
IO.println (match Loader.load "tests/integration/host/loadme.ml" with | Ok _ -> "loaded ok" | Err e -> e)
