# 38. Generic ASTs and visitors

ASTBuilder exposes `Ast`, a generic tree with `Leaf tag text` and `Node tag children`. It bridges to DSLtk's AST representation and S-expression facilities. These generic trees are useful for your own DSLs; they are not the mutable internal MetaML expression objects used by the type checker.

```metaml
t := Node "add" [Leaf "num" "1", Leaf "num" "2"]
Test.equal "tree size" (ASTBuilder.size t) 3
Test.equal "dump" (ASTBuilder.dump t) "(add (num 1) (num 2))"
Test.equal "postorder" (List.map ASTBuilder.tag (WalkAST.postorder t)) ["num", "num", "add"]
evaluate := VisitorBuilder.make (fn _ text -> Int.parse text |> Option.with_default 0) (fn _ children -> List.sum children)
Test.equal "folding visitor" (evaluate t) 3
```

ASTBuilder includes queries for tags, text, children, depth, size, and matching tags. It can dump or pretty-print trees, parse its supported textual notation, and convert to/from SExpr. Handle conversion Results rather than assuming every S-expression has a valid tree shape.

WalkAST provides preorder, postorder, breadth-first traversal, leaf collection, searches, bottom-up mapping, and rewrites. Bottom-up mapping is appropriate for constant folding because transformed children are available before transforming the parent. A rewrite callback returning None means no replacement for that node, not deletion from the tree.

WalkVisitor adds callback-oriented walks, enter/leave hooks, and paths. Paths are zero-based child-index lists. `at_path` returns Option for a checked lookup, and `path_to` can locate the first predicate match. Paths become stale after a shape-changing rewrite; do not retain them as permanent identifiers.

VisitorBuilder supports folds, tag counts, tag discovery, and dispatch. Keep visitors total over unexpected tags, or return an explicit Result when malformed trees are expected. Generic AST tags carry no automatic language-specific type guarantees.

**Sources:** ASTBuilder, WalkAST, WalkVisitor, and VisitorBuilder host headers and their D mirrors.

<!-- nav:begin -->
[Contents](README.md) · [Previous](37-graphs.md) · [Next](39-parser-libraries.md)
<!-- nav:end -->
