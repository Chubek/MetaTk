// metalib/cpp/IO.c — implementation of IO.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// IO.h and must not be compiled on its own. Mirror: metalib/d/IO.d.
#pragma once

namespace metalib
{


inline void
install_IO (metaml::Session &s)
{
  using namespace support;
  prim (s, "IO.flush", "Unit -> Unit", 1, [] (C &, V &) {
    std::cout << std::flush;
    return unit ();
  });
  prim (s, "IO.read_all", "Unit -> String", 1, [] (C &, V &) {
    std::string out;
    char buffer[8192];
    while (std::cin.read (buffer, sizeof buffer) || std::cin.gcount ())
      out.append (buffer, static_cast<std::size_t> (std::cin.gcount ()));
    if (std::cin.bad ()) throw std::runtime_error ("cannot read standard input");
    return str (out);
  });
  prim (s, "IO.read_lines", "Unit -> [String]", 1, [] (C &, V &) {
    std::vector<Value> out;
    std::string line;
    while (std::getline (std::cin, line))
      {
        if (!line.empty () && line.back () == '\r')
          line.pop_back ();
        out.push_back (str (line));
      }
    return list (out);
  });
  prim (s, "IO.printf", "String -> [String] -> Unit", 2, [] (C &c, V &a) {
    auto fmt = c.session.find_primitive ("String.format");
    std::string text = S (a[0]);
    if (fmt)
      {
        auto f = c.interp.prim_value (fmt);
        text = S (c.apply (f, { a[0], a[1] }));
      }
    c.print (text);
    return unit ();
  });
  prim (s, "IO.print_lines", "[String] -> Unit", 1, [] (C &c, V &a) {
    for (auto &x : items (a[0]))
      c.print (S (x) + "\n");
    return unit ();
  });
  prim (s, "IO.is_tty", "Unit -> Bool", 1, [] (C &, V &) {
#if defined(__unix__) || defined(__APPLE__)
    return boolean (isatty (1) != 0);
#else
    return boolean (false);
#endif
  });
}

} // namespace metalib
