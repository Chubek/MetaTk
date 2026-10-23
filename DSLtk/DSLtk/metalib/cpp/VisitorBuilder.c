// metalib/cpp/VisitorBuilder.c — implementation of VisitorBuilder.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// VisitorBuilder.h and must not be compiled on its own. Mirror: metalib/d/VisitorBuilder.d.
#pragma once

namespace metalib
{

namespace visitor_detail
{
using metaml::Value;
using namespace ast_detail;

inline Value
fold (metaml::PrimContext &c, const Value &on_leaf, const Value &on_node, const Value &v)
{
  if (is_leaf (v))
    return c.apply (on_leaf, { metaml::make_str (tag_of (v)), metaml::make_str (text_of (v)) });
  std::vector<Value> rs;
  for (auto &k : kids (v))
    rs.push_back (fold (c, on_leaf, on_node, k));
  return c.apply (on_node, { metaml::make_str (tag_of (v)), metaml::make_list (rs) });
}

inline void
count (const Value &v, std::map<std::string, std::int64_t> &m)
{
  ++m[tag_of (v)];
  for (auto &k : kids (v))
    count (k, m);
}
inline void
tags (const Value &v, std::vector<std::string> &out)
{
  if (std::find (out.begin (), out.end (), tag_of (v)) == out.end ())
    out.push_back (tag_of (v));
  for (auto &k : kids (v))
    tags (k, out);
}
} // namespace visitor_detail

inline void
install_VisitorBuilder (metaml::Session &s)
{
  using namespace support;
  using namespace visitor_detail;
  declare_ast_type (s);
  prim (s, "VisitorBuilder.make", "(String -> String -> 'r) -> (String -> ['r] -> 'r) -> (Ast -> 'r)",
        2, [] (C &c, V &a) {
          Value on_leaf = a[0], on_node = a[1];
          return c.session.make_function ("visitor", 1, [on_leaf, on_node] (C &cc, V &b) {
            return fold (cc, on_leaf, on_node, b[0]);
          });
        });
  prim (s, "VisitorBuilder.fold", "(String -> String -> 'r) -> (String -> ['r] -> 'r) -> Ast -> 'r",
        3, [] (C &c, V &a) { return fold (c, a[0], a[1], a[2]); });
  prim (s, "VisitorBuilder.dispatch", "[(String, Ast -> 'r)] -> (Ast -> 'r) -> Ast -> 'r", 3,
        [] (C &c, V &a) {
          const std::string &t = tag_of (a[2]);
          for (auto &h : items (a[0]))
            if (S (fst (h)) == t)
              return c.apply (snd (h), { a[2] });
          return c.apply (a[1], { a[2] });
        });
  prim (s, "VisitorBuilder.count_tags", "Ast -> [(String, Int)]", 1, [] (C &, V &a) {
    std::map<std::string, std::int64_t> m;
    count (a[0], m);
    std::vector<Value> out;
    for (auto &kv : m)
      out.push_back (pair (str (kv.first), integer (kv.second)));
    return list (out);
  });
  prim (s, "VisitorBuilder.tags", "Ast -> [String]", 1, [] (C &, V &a) {
    std::vector<std::string> out;
    tags (a[0], out);
    return strings (out);
  });
}

} // namespace metalib
