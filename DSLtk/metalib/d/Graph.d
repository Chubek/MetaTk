/**
 * metalib/d/Graph.d — MetaML host module `Graph` (D dialect): graph algorithms over edge lists (DSLtk `DirectedGraph`).
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/Graph.h):
 *   Graph.nodes : [(String, String)] -> [String]
 *   Graph.successors : String -> [(String, String)] -> [String]
 *   Graph.predecessors : String -> [(String, String)] -> [String]
 *   Graph.topo_sort : [String] -> [(String, String)] -> Result [String] String
 *   Graph.reachable : String -> [(String, String)] -> [String]
 *   Graph.depth_first : String -> [(String, String)] -> [String]
 *   Graph.shortest_path : String -> String -> [(String, String)] -> Option [String]
 *   Graph.sccs : [String] -> [(String, String)] -> [[String]]
 *   Graph.has_cycle : [(String, String)] -> Bool
 *   Graph.to_dot : String -> [String] -> [(String, String)] -> String
 */
module metalib.d.Graph;

import MetaML;
import metalib.d.Support;

import DSLtk : DirectedGraph, GraphNodeId;

private alias G = DirectedGraph!(string, string);

/// Builds a DSLtk graph; nodes are the listed names followed by any edge
/// endpoints not listed, in order of first appearance.
private final class Built
{
    G g;
    GraphNodeId[string] ids;
    string[] names;
    this() { g = new G; }
    GraphNodeId id(string n)
    {
        if (auto p = n in ids) return *p;
        auto i = g.add_node(n);
        ids[n] = i;
        names ~= n;
        return i;
    }
}
private Built build(Value[] nodes, Value[] edges)
{
    auto b = new Built;
    foreach (n; nodes) b.id(str_of(n));
    foreach (e; edges)
    {
        auto x = b.id(str_of(fst(e)));
        auto y = b.id(str_of(snd(e)));
        b.g.add_edge(x, y, "");
    }
    return b;
}
private Value names_of(Built b, const(GraphNodeId)[] ids)
{
    Value[] result;
    foreach (i; ids) result ~= make_str(b.g.value(i));
    return make_list(result);
}

void installGraph(Session s)
{
    prim(s, "Graph.nodes", "[(String, String)] -> [String]", 1, delegate Value(C c, V a) {
        auto b = build(null, items(a[0]));
        return strings(b.names);
    });
    prim(s, "Graph.successors", "String -> [(String, String)] -> [String]", 2, delegate Value(C c, V a) {
        auto b = build([a[0]], items(a[1]));
        return names_of(b, b.g.successors(b.ids[S(a[0])]));
    });
    prim(s, "Graph.predecessors", "String -> [(String, String)] -> [String]", 2, delegate Value(C c, V a) {
        auto b = build([a[0]], items(a[1]));
        return names_of(b, b.g.predecessors(b.ids[S(a[0])]));
    });
    prim(s, "Graph.topo_sort", "[String] -> [(String, String)] -> Result [String] String", 2, delegate Value(C c, V a) {
        auto b = build(items(a[0]), items(a[1]));
        try return ok(names_of(b, b.g.topological_sort()));
        catch (Exception) return err("Graph.topo_sort: the graph contains a cycle");
    });
    prim(s, "Graph.reachable", "String -> [(String, String)] -> [String]", 2, delegate Value(C c, V a) {
        auto b = build([a[0]], items(a[1]));
        return names_of(b, b.g.breadth_first(b.ids[S(a[0])]));
    });
    prim(s, "Graph.depth_first", "String -> [(String, String)] -> [String]", 2, delegate Value(C c, V a) {
        auto b = build([a[0]], items(a[1]));
        return names_of(b, b.g.depth_first(b.ids[S(a[0])]));
    });
    prim(s, "Graph.shortest_path", "String -> String -> [(String, String)] -> Option [String]", 3,
        delegate Value(C c, V a) {
            auto b = build([a[0], a[1]], items(a[2]));
            auto p = b.g.shortest_path(b.ids[S(a[0])], b.ids[S(a[1])]);
            if (p.isNull) return none();
            return some(names_of(b, p.get));
        });
    prim(s, "Graph.sccs", "[String] -> [(String, String)] -> [[String]]", 2, delegate Value(C c, V a) {
        auto b = build(items(a[0]), items(a[1]));
        Value[] result;
        foreach (comp; b.g.strongly_connected_components()) result ~= names_of(b, comp);
        return list(result);
    });
    prim(s, "Graph.has_cycle", "[(String, String)] -> Bool", 1, delegate Value(C c, V a) {
        auto b = build(null, items(a[0]));
        foreach (comp; b.g.strongly_connected_components()) if (comp.length > 1) return boolean(true);
        foreach (e; items(a[0])) if (S(fst(e)) == S(snd(e))) return boolean(true);
        return boolean(false);
    });
    prim(s, "Graph.to_dot", "String -> [String] -> [(String, String)] -> String", 3, delegate Value(C c, V a) {
        auto b = build(items(a[1]), items(a[2]));
        try return str(b.g.to_dot((string n) => n, (string e) => e, S(a[0])));
        catch (Exception e) return str("Graph.to_dot: " ~ e.msg);
    });
}

