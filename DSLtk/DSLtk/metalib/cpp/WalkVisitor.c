// metalib/cpp/WalkVisitor.c — implementation of WalkVisitor.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// WalkVisitor.h and must not be compiled on its own. Mirror: metalib/d/WalkVisitor.d.
#pragma once

namespace metalib
{

namespace walkv_detail
{
using metaml::Value;
using namespace ast_detail;

inline void
walk (metaml::PrimContext &c, const Value &f, const Value &v)
{
  c.apply (f, { v });
  for (auto &k : kids (v))
    walk (c, f, k);
}
inline void
walk_depth (metaml::PrimContext &c, const Value &f, const Value &v, std::int64_t d)
{
  c.apply (f, { metaml::Value::integer (d), v });
  for (auto &k : kids (v))
    walk_depth (c, f, k, d + 1);
}
inline void
enter_leave (metaml::PrimContext &c, const Value &enter, const Value &leave, const Value &v)
{
  c.apply (enter, { v });
  for (auto &k : kids (v))
    enter_leave (c, enter, leave, k);
  c.apply (leave, { v });
}
inline bool
path_to (metaml::PrimContext &c, const Value &p, const Value &v, std::vector<Value> &path)
{
  if (c.apply (p, { v }).i)
    return true;
  auto ks = kids (v);
  for (std::size_t i = 0; i < ks.size (); ++i)
    {
      path.push_back (metaml::Value::integer (static_cast<std::int64_t> (i)));
      if (path_to (c, p, ks[i], path))
        return true;
      path.pop_back ();
    }
  return false;
}
} // namespace walkv_detail

inline void
install_WalkVisitor (metaml::Session &s)
{
  using namespace support;
  using namespace walkv_detail;
  declare_ast_type (s);
  prim (s, "WalkVisitor.walk", "(Ast -> Unit) -> Ast -> Unit", 2, [] (C &c, V &a) {
    walk (c, a[0], a[1]);
    return unit ();
  });
  prim (s, "WalkVisitor.walk_depth", "(Int -> Ast -> Unit) -> Ast -> Unit", 2, [] (C &c, V &a) {
    walk_depth (c, a[0], a[1], 0);
    return unit ();
  });
  prim (s, "WalkVisitor.enter_leave", "(Ast -> Unit) -> (Ast -> Unit) -> Ast -> Unit", 3,
        [] (C &c, V &a) {
          enter_leave (c, a[0], a[1], a[2]);
          return unit ();
        });
  prim (s, "WalkVisitor.path_to", "(Ast -> Bool) -> Ast -> Option [Int]", 2, [] (C &c, V &a) {
    std::vector<Value> path;
    if (path_to (c, a[0], a[1], path))
      return some (list (path));
    return none ();
  });
  prim (s, "WalkVisitor.at_path", "[Int] -> Ast -> Option Ast", 2, [] (C &, V &a) {
    Value cur = a[1];
    for (auto &i : items (a[0]))
      {
        auto ks = kids (cur);
        if (i.i < 0 || static_cast<std::size_t> (i.i) >= ks.size ())
          return none ();
        cur = ks[static_cast<std::size_t> (i.i)];
      }
    return some (cur);
  });
}

} // namespace metalib
