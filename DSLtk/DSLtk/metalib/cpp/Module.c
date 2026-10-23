// metalib/cpp/Module.c — implementation of Module.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// Module.h and must not be compiled on its own. Mirror: metalib/d/Module.d.
#pragma once

namespace metalib
{

namespace module_detail
{
using metaml::Value;

inline metaml::ScopeP
find (metaml::Session &s, const std::string &path)
{
  std::vector<std::string> segs;
  std::size_t b = 0;
  while (true)
    {
      auto d = path.find ('.', b);
      segs.push_back (path.substr (b, d == std::string::npos ? d : d - b));
      if (d == std::string::npos)
        break;
      b = d + 1;
    }
  const metaml::ScopeP *m = s.checker ().global->module (segs[0]);
  metaml::ScopeP cur = m ? *m : nullptr;
  for (std::size_t i = 1; cur && i < segs.size (); ++i)
    {
      auto n = cur->modules.find (segs[i]);
      cur = n ? *n : nullptr;
    }
  return cur;
}

inline std::string
type_head (const metaml::TypeDefP &d)
{
  std::string out = "type " + d->name;
  for (std::size_t i = 0; i < d->params.size (); ++i)
    out += std::string (" '") + static_cast<char> ('a' + static_cast<int> (i % 26));
  return out;
}

inline std::string
signature (const metaml::ScopeP &m, const std::string &indent)
{
  std::string out = "sig\n";
  for (auto &k : m->types.keys ())
    {
      auto &d = *m->types.find (k);
      out += indent + "  " + type_head (d);
      if (d->alias && d->body)
        out += " = " + metaml::show_type (d->body);
      out += "\n";
      for (auto &c : d->ctors)
        out += indent + "    | " + c->name + " : " + metaml::show_type (c->scheme) + "\n";
    }
  for (auto &k : m->vars.keys ())
    out += indent + "  val " + k + " : " + metaml::show_type (m->vars.find (k)->scheme) + "\n";
  for (auto &k : m->modules.keys ())
    out += indent + "  module " + k + " : " + signature (*m->modules.find (k), indent + "  ");
  for (auto &k : m->functors.keys ())
    out += indent + "  functor " + k + "\n";
  return out + indent + "end\n";
}
} // namespace module_detail

inline void
install_Module (metaml::Session &s)
{
  using namespace support;
  using namespace module_detail;
  prim (s, "Module.names", "Unit -> [String]", 1,
        [] (C &c, V &) { return strings (c.session.checker ().global->modules.keys ()); });
  prim (s, "Module.functors", "Unit -> [String]", 1,
        [] (C &c, V &) { return strings (c.session.checker ().global->functors.keys ()); });
  prim (s, "Module.signatures", "Unit -> [String]", 1,
        [] (C &c, V &) { return strings (c.session.checker ().global->sigs.keys ()); });
  prim (s, "Module.exists", "String -> Bool", 1,
        [] (C &c, V &a) { return boolean (find (c.session, S (a[0])) != nullptr); });
  prim (s, "Module.members", "String -> Option [String]", 1, [] (C &c, V &a) {
    auto m = find (c.session, S (a[0]));
    return m ? some (strings (m->vars.keys ())) : none ();
  });
  prim (s, "Module.types", "String -> Option [String]", 1, [] (C &c, V &a) {
    auto m = find (c.session, S (a[0]));
    return m ? some (strings (m->types.keys ())) : none ();
  });
  prim (s, "Module.submodules", "String -> Option [String]", 1, [] (C &c, V &a) {
    auto m = find (c.session, S (a[0]));
    return m ? some (strings (m->modules.keys ())) : none ();
  });
  prim (s, "Module.member_type", "String -> String -> Option String", 2, [] (C &c, V &a) {
    auto m = find (c.session, S (a[0]));
    if (!m)
      return none ();
    auto v = m->vars.find (S (a[1]));
    return v ? some (str (metaml::show_type (v->scheme))) : none ();
  });
  prim (s, "Module.signature", "String -> Option String", 1, [] (C &c, V &a) {
    auto m = find (c.session, S (a[0]));
    return m ? some (str (signature (m, ""))) : none ();
  });
}

} // namespace metalib
