// metalib/cpp/Loader.c — implementation of Loader.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// Loader.h and must not be compiled on its own. Mirror: metalib/d/Loader.d.
#pragma once

namespace metalib
{

inline void
install_Loader (metaml::Session &s)
{
  using namespace support;
  prim (s, "Loader.load", "String -> Result Unit String", 1, [] (C &c, V &a) {
    auto r = c.session.load_file (S (a[0]));
    if (r.is_err ())
      return err (frontend_detail::first_diag (r.unwrap_err ()));
    return ok (unit ());
  });
  prim (s, "Loader.resolve", "String -> Option String", 1, [] (C &c, V &a) {
    auto r = c.session.resolve_import (S (a[0]));
    if (!r)
      return none ();
    return some (str (*r));
  });
  prim (s, "Loader.search_path", "Unit -> [String]", 1,
        [] (C &c, V &) { return strings (c.session.search_paths ()); });
  prim (s, "Loader.add_path", "String -> Unit", 1, [] (C &c, V &a) {
    c.session.add_search_path (S (a[0]));
    return unit ();
  });
  prim (s, "Loader.stdlib", "Unit -> Option String", 1, [] (C &c, V &) {
    if (c.session.stdlib_dir ().empty ())
      return none ();
    return some (str (c.session.stdlib_dir ()));
  });
  prim (s, "Loader.loaded", "Unit -> [String]", 1,
        [] (C &c, V &) { return strings (c.session.imported_files ()); });
}

} // namespace metalib
