// metalib/cpp/SExpr.c — implementation of SExpr.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// SExpr.h and must not be compiled on its own. Mirror: metalib/d/SExpr.d.
#pragma once

namespace metalib
{


namespace sexpr_detail
{
inline metaml::Value
to_value (const dsl::SExpr &e)
{
  if (e.is_atom)
    return metaml::make_ctor ("Atom", 0, { metaml::make_str (e.text) });
  std::vector<metaml::Value> xs;
  for (auto &i : e.items)
    xs.push_back (to_value (i));
  return metaml::make_ctor ("SList", 1, { metaml::make_list (xs) });
}
inline dsl::SExpr
from_value (const metaml::Value &v)
{
  auto a = metaml::agg_of (v);
  if (a->tag == "Atom")
    return dsl::SExpr::atom (metaml::str_of (a->items[0]));
  std::vector<dsl::SExpr> xs;
  for (auto &x : metaml::list_items (a->items[0]))
    xs.push_back (from_value (x));
  return dsl::SExpr::list (std::move (xs));
}
} // namespace sexpr_detail

inline void
install_SExpr (metaml::Session &s)
{
  using namespace support;
  using namespace sexpr_detail;
  if (!s.checker ().global->types.find ("SExpr"))
    s.declare ("type SExpr := | Atom String | SList [SExpr]\n", "<metalib SExpr>");
  prim (s, "SExpr.parse", "String -> Result [SExpr] String", 1, [] (C &, V &a) {
    try
      {
        std::vector<Value> out;
        for (auto &e : dsl::parse_sexpressions (S (a[0])))
          out.push_back (to_value (e));
        return ok (list (out));
      }
    catch (const std::exception &e)
      {
        return err (e.what ());
      }
  });
  prim (s, "SExpr.parse_one", "String -> Result SExpr String", 1, [] (C &, V &a) {
    try
      {
        return ok (to_value (dsl::parse_sexpr (S (a[0]))));
      }
    catch (const std::exception &e)
      {
        return err (e.what ());
      }
  });
  prim (s, "SExpr.render", "SExpr -> String", 1,
        [] (C &, V &a) { return str (from_value (a[0]).dump ()); });
  prim (s, "SExpr.atom_text", "SExpr -> Option String", 1, [] (C &, V &a) {
    auto t = metaml::agg_of (a[0]);
    if (t->tag == "Atom")
      return some (t->items[0]);
    return none ();
  });
}

} // namespace metalib
