// metalib/cpp/WalkAST.c — implementation of WalkAST.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// WalkAST.h and must not be compiled on its own. Mirror: metalib/d/WalkAST.d.
#pragma once

namespace metalib
{

namespace walk_detail
{
using metaml::Value;
using namespace ast_detail;

inline void
pre (const Value &v, std::vector<Value> &out)
{
  out.push_back (v);
  for (auto &k : kids (v))
    pre (k, out);
}
inline void
post (const Value &v, std::vector<Value> &out)
{
  for (auto &k : kids (v))
    post (k, out);
  out.push_back (v);
}
inline Value
map_up (metaml::PrimContext &c, const Value &f, const Value &v)
{
  if (is_leaf (v))
    return c.apply (f, { v });
  std::vector<Value> ks;
  for (auto &k : kids (v))
    ks.push_back (map_up (c, f, k));
  return c.apply (f, { node (tag_of (v), ks) });
}
inline Value
map_down (metaml::PrimContext &c, const Value &f, const Value &v)
{
  Value r = c.apply (f, { v });
  if (is_leaf (r))
    return r;
  std::vector<Value> ks;
  for (auto &k : kids (r))
    ks.push_back (map_down (c, f, k));
  return node (tag_of (r), ks);
}

constexpr std::int64_t rewrite_limit = 100000;

inline Value
rewrite (metaml::PrimContext &c, const Value &rule, const Value &v0, std::int64_t &steps)
{
  Value v = v0;
  if (!is_leaf (v))
    {
      std::vector<Value> ks;
      for (auto &k : kids (v))
        ks.push_back (rewrite (c, rule, k, steps));
      v = node (tag_of (v), ks);
    }
  while (true)
    {
      Value r = c.apply (rule, { v });
      auto a = metaml::agg_of (r);
      if (a->tag != "Some")
        return v;
      if (++steps > rewrite_limit)
        c.error ("M0404", "WalkAST.rewrite: no normal form after "
                              + std::to_string (rewrite_limit) + " rewrites");
      v = rewrite (c, rule, a->items[0], steps);
    }
}
inline void
find_all (metaml::PrimContext &c, const Value &p, const Value &v, std::vector<Value> &out)
{
  Value r = c.apply (p, { v });
  if (r.i)
    out.push_back (v);
  for (auto &k : kids (v))
    find_all (c, p, k, out);
}
} // namespace walk_detail

inline void
install_WalkAST (metaml::Session &s)
{
  using namespace support;
  using namespace walk_detail;
  declare_ast_type (s);
  prim (s, "WalkAST.preorder", "Ast -> [Ast]", 1, [] (C &, V &a) {
    std::vector<Value> out;
    pre (a[0], out);
    return list (out);
  });
  prim (s, "WalkAST.postorder", "Ast -> [Ast]", 1, [] (C &, V &a) {
    std::vector<Value> out;
    post (a[0], out);
    return list (out);
  });
  prim (s, "WalkAST.breadth_first", "Ast -> [Ast]", 1, [] (C &, V &a) {
    std::vector<Value> out{ a[0] };
    for (std::size_t i = 0; i < out.size (); ++i)
      for (auto &k : kids (out[i]))
        out.push_back (k);
    return list (out);
  });
  prim (s, "WalkAST.leaves", "Ast -> [Ast]", 1, [] (C &, V &a) {
    std::vector<Value> all, out;
    pre (a[0], all);
    for (auto &v : all)
      if (is_leaf (v))
        out.push_back (v);
    return list (out);
  });
  prim (s, "WalkAST.map_bottom_up", "(Ast -> Ast) -> Ast -> Ast", 2,
        [] (C &c, V &a) { return map_up (c, a[0], a[1]); });
  prim (s, "WalkAST.map_top_down", "(Ast -> Ast) -> Ast -> Ast", 2,
        [] (C &c, V &a) { return map_down (c, a[0], a[1]); });
  prim (s, "WalkAST.rewrite", "(Ast -> Option Ast) -> Ast -> Ast", 2, [] (C &c, V &a) {
    std::int64_t steps = 0;
    return rewrite (c, a[0], a[1], steps);
  });
  prim (s, "WalkAST.find_all", "(Ast -> Bool) -> Ast -> [Ast]", 2, [] (C &c, V &a) {
    std::vector<Value> out;
    find_all (c, a[0], a[1], out);
    return list (out);
  });
}

} // namespace metalib
