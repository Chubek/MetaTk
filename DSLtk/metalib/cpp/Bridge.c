// metalib/cpp/Bridge.c — implementation of Bridge.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// Bridge.h and must not be compiled on its own. Mirror: metalib/d/Bridge.d.
#pragma once

namespace metalib
{


namespace bridge_detail
{
inline std::string
kind_name (const metaml::Value &v)
{
  using metaml::VK;
  switch (v.kind)
    {
    case VK::Unit:
      return "Unit";
    case VK::Bool:
      return "Bool";
    case VK::Int:
      return "Int";
    case VK::Float:
      return "Float";
    case VK::Str:
      return "String";
    case VK::List:
      return "List";
    case VK::Tuple:
      return "Tuple";
    case VK::Record:
      return "Record";
    case VK::Ctor:
      return "Ctor";
    case VK::Closure:
    case VK::Prim:
    case VK::CtorFn:
    case VK::Compose:
      return "Function";
    case VK::Module:
      return "Module";
    case VK::Foreign:
      return "Foreign";
    }
  return "Unit";
}
inline std::int64_t
tree_size (const metaml::Value &v)
{
  using metaml::VK;
  std::int64_t n = 1;
  if (v.kind == VK::List)
    for (auto &x : metaml::list_items (v))
      n += tree_size (x);
  else if (v.kind == VK::Tuple || v.kind == VK::Record || v.kind == VK::Ctor)
    for (auto &x : metaml::agg_of (v)->items)
      n += tree_size (x);
  return n;
}
} // namespace bridge_detail

inline void
install_Bridge (metaml::Session &s)
{
  using namespace support;
  using namespace bridge_detail;
  prim (s, "Bridge.kind", "'a -> String", 1, [] (C &, V &a) { return str (kind_name (a[0])); });
  prim (s, "Bridge.encode", "'a -> String", 1,
        [] (C &, V &a) { return str (metaml::show_value (a[0])); });
  prim (s, "Bridge.fields", "'a -> [String]", 1, [] (C &, V &a) {
    if (a[0].kind != VK::Record)
      return list ({});
    return strings (*metaml::agg_of (a[0])->labels);
  });
  prim (s, "Bridge.ctor_name", "'a -> Option String", 1, [] (C &, V &a) {
    if (a[0].kind != VK::Ctor)
      return none ();
    return some (str (metaml::agg_of (a[0])->tag));
  });
  prim (s, "Bridge.arity", "'a -> Int", 1, [] (C &, V &a) {
    const Value &v = a[0];
    if (v.kind == VK::List)
      return integer (static_cast<std::int64_t> (items (v).size ()));
    if (v.kind == VK::Tuple || v.kind == VK::Record || v.kind == VK::Ctor)
      return integer (static_cast<std::int64_t> (metaml::agg_of (v)->items.size ()));
    return integer (0);
  });
  prim (s, "Bridge.size", "'a -> Int", 1, [] (C &, V &a) { return integer (tree_size (a[0])); });
  prim (s, "Bridge.same", "'a -> 'a -> Bool", 2, [] (C &, V &a) {
    if (!a[0].obj || !a[1].obj)
      return boolean (false);
    return boolean (a[0].obj == a[1].obj);
  });
}

} // namespace metalib
