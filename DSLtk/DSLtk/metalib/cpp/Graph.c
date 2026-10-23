// metalib/cpp/Graph.c — implementation of Graph.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// Graph.h and must not be compiled on its own. Mirror: metalib/d/Graph.d.
#pragma once

namespace metalib
{

namespace graph_detail
{
using metaml::Value;
using G = dsl::DirectedGraph<std::string, std::string>;

/// Builds a DSLtk graph; nodes are the listed names followed by any edge
/// endpoints not listed, in order of first appearance.
struct Built
{
  G g{};
  std::map<std::string, dsl::GraphNodeId> ids{};
  std::vector<std::string> names{};
  dsl::GraphNodeId
  id (const std::string &n)
  {
    auto it = ids.find (n);
    if (it != ids.end ())
      return it->second;
    auto i = g.add_node (n);
    ids[n] = i;
    names.push_back (n);
    return i;
  }
};
inline Built
build (const std::vector<Value> &nodes, const std::vector<Value> &edges)
{
  Built b;
  for (auto &n : nodes)
    b.id (metaml::str_of (n));
  for (auto &e : edges)
    {
      auto x = b.id (metaml::str_of (support::fst (e)));
      auto y = b.id (metaml::str_of (support::snd (e)));
      b.g.add_edge (x, y, "");
    }
  return b;
}
inline Value
names_of (const Built &b, const std::vector<dsl::GraphNodeId> &ids)
{
  std::vector<Value> out;
  for (auto i : ids)
    out.push_back (metaml::make_str (b.g.value (i)));
  return metaml::make_list (out);
}
} // namespace graph_detail

inline void
install_Graph (metaml::Session &s)
{
  using namespace support;
  using namespace graph_detail;
  prim (s, "Graph.nodes", "[(String, String)] -> [String]", 1, [] (C &, V &a) {
    auto b = build ({}, items (a[0]));
    return strings (b.names);
  });
  prim (s, "Graph.successors", "String -> [(String, String)] -> [String]", 2, [] (C &, V &a) {
    auto b = build ({ a[0] }, items (a[1]));
    return names_of (b, b.g.successors (b.ids[S (a[0])]));
  });
  prim (s, "Graph.predecessors", "String -> [(String, String)] -> [String]", 2, [] (C &, V &a) {
    auto b = build ({ a[0] }, items (a[1]));
    return names_of (b, b.g.predecessors (b.ids[S (a[0])]));
  });
  prim (s, "Graph.topo_sort", "[String] -> [(String, String)] -> Result [String] String", 2,
        [] (C &, V &a) {
          auto b = build (items (a[0]), items (a[1]));
          try
            {
              return ok (names_of (b, b.g.topological_sort ()));
            }
          catch (const std::exception &)
            {
              return err ("Graph.topo_sort: the graph contains a cycle");
            }
        });
  prim (s, "Graph.reachable", "String -> [(String, String)] -> [String]", 2, [] (C &, V &a) {
    auto b = build ({ a[0] }, items (a[1]));
    return names_of (b, b.g.breadth_first (b.ids[S (a[0])]));
  });
  prim (s, "Graph.depth_first", "String -> [(String, String)] -> [String]", 2, [] (C &, V &a) {
    auto b = build ({ a[0] }, items (a[1]));
    return names_of (b, b.g.depth_first (b.ids[S (a[0])]));
  });
  prim (s, "Graph.shortest_path", "String -> String -> [(String, String)] -> Option [String]", 3,
        [] (C &, V &a) {
          auto b = build ({ a[0], a[1] }, items (a[2]));
          auto p = b.g.shortest_path (b.ids[S (a[0])], b.ids[S (a[1])]);
          if (!p)
            return none ();
          return some (names_of (b, *p));
        });
  prim (s, "Graph.sccs", "[String] -> [(String, String)] -> [[String]]", 2, [] (C &, V &a) {
    auto b = build (items (a[0]), items (a[1]));
    std::vector<Value> out;
    for (auto &comp : b.g.strongly_connected_components ())
      out.push_back (names_of (b, comp));
    return list (out);
  });
  prim (s, "Graph.has_cycle", "[(String, String)] -> Bool", 1, [] (C &, V &a) {
    auto b = build ({}, items (a[0]));
    for (auto &comp : b.g.strongly_connected_components ())
      if (comp.size () > 1)
        return boolean (true);
    for (auto &e : items (a[0]))
      if (S (fst (e)) == S (snd (e)))
        return boolean (true);
    return boolean (false);
  });
  prim (s, "Graph.to_dot", "String -> [String] -> [(String, String)] -> String", 3,
        [] (C &, V &a) {
          auto b = build (items (a[1]), items (a[2]));
          try
            {
              return str (b.g.to_dot ([] (const std::string &n) { return n; },
                                      [] (const std::string &e) { return e; }, S (a[0])));
            }
          catch (const std::exception &e)
            {
              return str (std::string ("Graph.to_dot: ") + e.what ());
            }
        });
}

} // namespace metalib
