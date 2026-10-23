// metalib/cpp/String.c — implementation of String.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// String.h and must not be compiled on its own. Mirror: metalib/d/String.d.
#pragma once

namespace metalib
{


namespace string_detail
{
/// Decodes one UTF-8 sequence at i; returns (code point, length) or (-1, 1).
inline std::pair<std::int64_t, std::size_t>
decode (const std::string &s, std::size_t i)
{
  unsigned char c = static_cast<unsigned char> (s[i]);
  std::size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
  if (n == 0 || i + n > s.size ())
    return { -1, 1 };
  if (n == 1)
    return { c, 1 };
  std::int64_t cp = c & (0x7F >> n);
  for (std::size_t k = 1; k < n; ++k)
    {
      unsigned char d = static_cast<unsigned char> (s[i + k]);
      if ((d & 0xC0) != 0x80)
        return { -1, 1 };
      cp = (cp << 6) | (d & 0x3F);
    }
  static const std::int64_t mins[] = { 0, 0, 0x80, 0x800, 0x10000 };
  if (cp < mins[n] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
    return { -1, 1 };
  return { cp, n };
}
inline bool
all_of (const std::string &s, bool (*pred) (char))
{
  if (s.empty ())
    return false;
  for (char c : s)
    if (!pred (c))
      return false;
  return true;
}
inline std::string
padding (std::size_t n, const std::string &fill)
{
  std::string out;
  if (fill.empty ())
    return out;
  while (out.size () < n)
    out += fill[out.size () % fill.size ()];
  return out;
}
} // namespace string_detail

inline void
install_String (metaml::Session &s)
{
  using namespace support;
  using namespace string_detail;
  prim (s, "String.utf8_length", "String -> Int", 1, [] (C &, V &a) {
    const auto &x = S (a[0]);
    std::int64_t n = 0;
    for (std::size_t i = 0; i < x.size (); i += decode (x, i).second)
      ++n;
    return integer (n);
  });
  prim (s, "String.utf8_chars", "String -> [String]", 1, [] (C &, V &a) {
    const auto &x = S (a[0]);
    std::vector<Value> out;
    for (std::size_t i = 0; i < x.size ();)
      {
        auto n = decode (x, i).second;
        out.push_back (str (x.substr (i, n)));
        i += n;
      }
    return list (out);
  });
  prim (s, "String.utf8_valid", "String -> Bool", 1, [] (C &, V &a) {
    const auto &x = S (a[0]);
    for (std::size_t i = 0; i < x.size ();)
      {
        auto d = decode (x, i);
        if (d.first < 0)
          return boolean (false);
        i += d.second;
      }
    return boolean (true);
  });
  prim (s, "String.codepoints", "String -> [Int]", 1, [] (C &, V &a) {
    const auto &x = S (a[0]);
    std::vector<Value> out;
    for (std::size_t i = 0; i < x.size ();)
      {
        auto d = decode (x, i);
        out.push_back (integer (d.first < 0 ? 0xFFFD : d.first));
        i += d.second;
      }
    return list (out);
  });
  prim (s, "String.of_codepoints", "[Int] -> String", 1, [] (C &, V &a) {
    std::string out;
    for (auto &v : items (a[0]))
      {
        std::int64_t cp = v.i;
        if (cp < 0 || cp > 0x10FFFF || !metaml::detail::append_utf8 (out, static_cast<std::uint32_t> (cp)))
          metaml::detail::append_utf8 (out, 0xFFFD);
      }
    return str (out);
  });
  prim (s, "String.pad_left", "Int -> String -> String -> String", 3, [] (C &, V &a) {
    const auto &x = S (a[2]);
    std::size_t w = a[0].i < 0 ? 0 : static_cast<std::size_t> (a[0].i);
    if (x.size () >= w)
      return str (x);
    return str (padding (w - x.size (), S (a[1])) + x);
  });
  prim (s, "String.pad_right", "Int -> String -> String -> String", 3, [] (C &, V &a) {
    const auto &x = S (a[2]);
    std::size_t w = a[0].i < 0 ? 0 : static_cast<std::size_t> (a[0].i);
    if (x.size () >= w)
      return str (x);
    return str (x + padding (w - x.size (), S (a[1])));
  });
  prim (s, "String.words", "String -> [String]", 1, [] (C &, V &a) {
    std::vector<Value> out;
    std::string cur;
    for (char c : S (a[0]))
      {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
          {
            if (!cur.empty ())
              out.push_back (str (cur));
            cur.clear ();
          }
        else
          cur += c;
      }
    if (!cur.empty ())
      out.push_back (str (cur));
    return list (out);
  });
  prim (s, "String.last_index_of", "String -> String -> Option Int", 2, [] (C &, V &a) {
    auto p = S (a[1]).rfind (S (a[0]));
    if (p == std::string::npos)
      return none ();
    return some (integer (static_cast<std::int64_t> (p)));
  });
  prim (s, "String.count", "String -> String -> Int", 2, [] (C &, V &a) {
    const auto &n = S (a[0]), &x = S (a[1]);
    if (n.empty ())
      return integer (0);
    std::int64_t c = 0;
    for (std::size_t p = x.find (n); p != std::string::npos; p = x.find (n, p + n.size ()))
      ++c;
    return integer (c);
  });
  prim (s, "String.strip_prefix", "String -> String -> Option String", 2, [] (C &, V &a) {
    const auto &p = S (a[0]), &x = S (a[1]);
    if (x.size () >= p.size () && x.compare (0, p.size (), p) == 0)
      return some (str (x.substr (p.size ())));
    return none ();
  });
  prim (s, "String.strip_suffix", "String -> String -> Option String", 2, [] (C &, V &a) {
    const auto &p = S (a[0]), &x = S (a[1]);
    if (x.size () >= p.size () && x.compare (x.size () - p.size (), p.size (), p) == 0)
      return some (str (x.substr (0, x.size () - p.size ())));
    return none ();
  });
  prim (s, "String.capitalize", "String -> String", 1, [] (C &, V &a) {
    std::string x = S (a[0]);
    if (!x.empty () && x[0] >= 'a' && x[0] <= 'z')
      x[0] = static_cast<char> (x[0] - 32);
    return str (x);
  });
  struct Cls
  {
    const char *name;
    bool (*pred) (char);
  };
  static const Cls classes[] = {
    { "String.is_digit", [] (char c) { return c >= '0' && c <= '9'; } },
    { "String.is_alpha", [] (char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); } },
    { "String.is_space", [] (char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; } },
    { "String.is_alnum", [] (char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
      } },
  };
  for (auto &cl : classes)
    {
      auto pred = cl.pred;
      prim (s, cl.name, "String -> Bool", 1,
            [pred] (C &, V &a) { return boolean (all_of (S (a[0]), pred)); });
    }
  prim (s, "String.format", "String -> [String] -> String", 2, [] (C &, V &a) {
    const auto &f = S (a[0]);
    auto args = items (a[1]);
    std::string out;
    std::size_t next = 0;
    for (std::size_t i = 0; i < f.size (); ++i)
      {
        if (f[i] == '{' && i + 1 < f.size () && f[i + 1] == '{')
          {
            out += '{';
            ++i;
          }
        else if (f[i] == '}' && i + 1 < f.size () && f[i + 1] == '}')
          {
            out += '}';
            ++i;
          }
        else if (f[i] == '{' && i + 1 < f.size () && f[i + 1] == '}')
          {
            if (next < args.size ())
              out += S (args[next]);
            ++next;
            ++i;
          }
        else
          out += f[i];
      }
    return str (out);
  });
  prim (s, "String.escape", "String -> String", 1, [] (C &, V &a) {
    auto q = metaml::detail::quote_string (S (a[0]));
    return str (q.substr (1, q.size () - 2));
  });
  prim (s, "String.compare_ci", "String -> String -> Int", 2, [] (C &, V &a) {
    auto x = metaml::detail::ascii_map (S (a[0]), false);
    auto y = metaml::detail::ascii_map (S (a[1]), false);
    return integer (x < y ? -1 : x > y ? 1 : 0);
  });
}

} // namespace metalib
