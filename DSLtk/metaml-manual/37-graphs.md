# 37. Graph algorithms

Graph provides directed, string-named graph operations backed by DSLtk's graph substrate. Edges are pairs `(source, destination)`. Functions that also accept an explicit node list can retain isolated vertices; other operations discover nodes from edge endpoints in deterministic first-seen order.

```metaml
edges := [("a", "b"), ("b", "c"), ("a", "c")]
Test.equal "topological order" (Graph.topo_sort [] edges) (Ok ["a", "b", "c"])
Test.equal "shortest unweighted path" (Graph.shortest_path "a" "c" edges) (Some ["a", "c"])
Test.equal "unreachable" (Graph.shortest_path "c" "a" edges) None
Test.check "self-cycle" (Graph.has_cycle [("x", "x")])
```

`successors` and `predecessors` inspect adjacency. `reachable` performs breadth-first traversal and includes the starting node; `depth_first` provides depth-first traversal. A shortest path is unweighted, not a minimum-cost path with numeric edge weights.

`topo_sort nodes edges` returns Err when a cycle prevents a topological ordering. `sccs` computes strongly connected components, including explicitly listed isolated nodes. A one-node component is cyclic only if it has a self-edge; `has_cycle` accounts for that distinction.

Graph order is deterministic for a fixed input, but multiple valid topological orders may exist. Do not treat one ordering as a mathematical uniqueness claim. If external files or hash maps supply the edges in unstable order, normalize those inputs before depending on reproducible output.

`to_dot` emits Graphviz DOT text. Use it for inspection rather than assuming arbitrary untrusted names are a safe fragment to splice into another document or shell command. The library does not launch Graphviz or manage an external process.

Applications include module dependency analysis, build planning, reachability, and grouping mutually recursive definitions. For very large graphs, account for both the input edge list and the native graph constructed during each operation; this API rebuilds a graph rather than retaining a mutable graph handle between calls.

**Sources:** [`cpp/Graph.h`](../metalib/cpp/Graph.h), [`d/Graph.d`](../metalib/d/Graph.d).

<!-- nav:begin -->
[Contents](README.md) · [Previous](36-configuration-and-sexpressions.md) · [Next](38-ast-and-visitors.md)
<!-- nav:end -->
