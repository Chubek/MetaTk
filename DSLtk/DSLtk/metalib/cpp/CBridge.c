// metalib/cpp/CBridge.c — implementation of CBridge.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// CBridge.h and must not be compiled on its own. Mirror: metalib/d/CBridge.d.
#pragma once

namespace metalib
{

inline void
install_CBridge (metaml::Session &s)
{
  using namespace support;
  prim (s, "CBridge.api_version", "Unit -> Int", 1, [] (C &, V &) { return integer (1); });
  prim (s, "CBridge.functions", "Unit -> [String]", 1, [] (C &, V &) {
    return strings ({ "metaml_new", "metaml_free", "metaml_load_stdlib", "metaml_eval",
                      "metaml_type_of", "metaml_free_string", "metaml_version" });
  });
}

// ---------------------------------------------------------------------
//  The C ABI. Define METALIB_CBRIDGE_IMPLEMENTATION in exactly one
//  translation unit before including CBridge.h to emit these functions.
// ---------------------------------------------------------------------
#ifdef METALIB_CBRIDGE_IMPLEMENTATION
namespace cbridge_detail
{
inline char *
dup (const std::string &s)
{
  char *p = static_cast<char *> (std::malloc (s.size () + 1));
  if (p)
    std::memcpy (p, s.c_str (), s.size () + 1);
  return p;
}
} // namespace cbridge_detail
} // namespace metalib

extern "C"
{
  struct metaml_session
  {
    metaml::Session session;
  };

  metaml_session *
  metaml_new (void)
  {
    try
      {
        return new metaml_session ();
      }
    catch (...)
      {
        return nullptr;
      }
  }
  void
  metaml_free (metaml_session *s)
  {
    delete s;
  }
  int
  metaml_load_stdlib (metaml_session *s, const char *dir)
  {
    auto r = s->session.load_stdlib (dir ? dir : "");
    return r.is_err () ? 1 : 0;
  }
  char *
  metaml_eval (metaml_session *s, const char *source, int *ok)
  {
    auto r = s->session.eval (source ? source : "", "<c>");
    if (ok)
      *ok = r.is_err () ? 0 : 1;
    if (r.is_err ())
      {
        std::string out;
        for (auto &d : r.unwrap_err ())
          out += s->session.render (d);
        return metalib::cbridge_detail::dup (out);
      }
    return metalib::cbridge_detail::dup (metaml::show_value (r.unwrap ()));
  }
  char *
  metaml_type_of (metaml_session *s, const char *expr, int *ok)
  {
    auto r = s->session.type_of (expr ? expr : "");
    if (ok)
      *ok = r.is_err () ? 0 : 1;
    if (r.is_err ())
      return metalib::cbridge_detail::dup (r.unwrap_err ().empty () ? std::string ("error")
                                                                     : r.unwrap_err ()[0].headline ());
    return metalib::cbridge_detail::dup (r.unwrap ());
  }
  void
  metaml_free_string (char *p)
  {
    std::free (p);
  }
  const char *
  metaml_version (void)
  {
    return metaml::version;
  }
}

namespace metalib
{
#endif // METALIB_CBRIDGE_IMPLEMENTATION

} // namespace metalib
