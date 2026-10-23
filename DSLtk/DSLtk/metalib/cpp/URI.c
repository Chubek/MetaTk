// metalib/cpp/URI.c — implementation of URI.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// URI.h and must not be compiled on its own. Mirror: metalib/d/URI.d.
#pragma once

namespace metalib
{


namespace uri_detail
{
inline bool
unreserved (char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-'
         || c == '.' || c == '_' || c == '~';
}
inline std::string
encode (const std::string &s)
{
  static const char *d = "0123456789ABCDEF";
  std::string out;
  for (char ch : s)
    {
      auto c = static_cast<unsigned char> (ch);
      if (unreserved (ch))
        out += ch;
      else
        {
          out += '%';
          out += d[c >> 4];
          out += d[c & 15];
        }
    }
  return out;
}
inline std::optional<std::string>
decode (const std::string &s, bool plus_space)
{
  std::string out;
  auto hexv = [] (char c) -> int {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    return -1;
  };
  for (std::size_t i = 0; i < s.size (); ++i)
    {
      if (s[i] == '%')
        {
          if (i + 2 >= s.size ())
            return std::nullopt;
          int h = hexv (s[i + 1]), l = hexv (s[i + 2]);
          if (h < 0 || l < 0)
            return std::nullopt;
          out += static_cast<char> (h * 16 + l);
          i += 2;
        }
      else if (plus_space && s[i] == '+')
        out += ' ';
      else
        out += s[i];
    }
  return out;
}
} // namespace uri_detail

inline void
install_URI (metaml::Session &s)
{
  using namespace support;
  using namespace uri_detail;
  s.declare ("type Uri = { scheme : String, user : String, host : String, port : Int, "
             "path : String, query : String, fragment : String }\n",
             "<metalib URI>");
  prim (s, "URI.parse", "String -> Option Uri", 1, [] (C &, V &a) {
    std::string u = S (a[0]);
    std::string scheme, user, host, path, query, fragment;
    std::int64_t port = -1;
    auto hash = u.find ('#');
    if (hash != std::string::npos)
      {
        fragment = u.substr (hash + 1);
        u = u.substr (0, hash);
      }
    auto q = u.find ('?');
    if (q != std::string::npos)
      {
        query = u.substr (q + 1);
        u = u.substr (0, q);
      }
    auto colon = u.find (':');
    bool has_scheme = colon != std::string::npos && colon > 0;
    for (std::size_t i = 0; has_scheme && i < colon; ++i)
      {
        char c = u[i];
        bool okc = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                   || (i > 0 && ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.'));
        if (!okc)
          has_scheme = false;
      }
    if (has_scheme)
      {
        scheme = metaml::detail::ascii_map (u.substr (0, colon), false);
        u = u.substr (colon + 1);
      }
    if (u.size () >= 2 && u[0] == '/' && u[1] == '/')
      {
        u = u.substr (2);
        auto slash = u.find ('/');
        std::string auth = slash == std::string::npos ? u : u.substr (0, slash);
        path = slash == std::string::npos ? "" : u.substr (slash);
        auto at = auth.rfind ('@');
        if (at != std::string::npos)
          {
            user = auth.substr (0, at);
            auth = auth.substr (at + 1);
          }
        std::size_t pc = std::string::npos;
        if (!auth.empty () && auth[0] == '[')
          {
            auto rb = auth.find (']');
            if (rb == std::string::npos)
              return none ();
            if (rb + 1 < auth.size () && auth[rb + 1] == ':')
              pc = rb + 1;
          }
        else
          pc = auth.rfind (':');
        if (pc != std::string::npos)
          {
            auto ps = auth.substr (pc + 1);
            if (!ps.empty ())
              {
                for (char c : ps)
                  if (c < '0' || c > '9')
                    return none ();
                auto pv = metaml::detail::parse_int (ps);
                if (!pv || *pv > 65535)
                  return none ();
                port = *pv;
              }
            auth = auth.substr (0, pc);
          }
        host = metaml::detail::ascii_map (auth, false);
      }
    else
      path = u;
    if (!has_scheme && host.empty () && path.empty () && query.empty () && fragment.empty ())
      return none ();
    return some (record ({ { "scheme", str (scheme) }, { "user", str (user) },
                           { "host", str (host) }, { "port", integer (port) },
                           { "path", str (path) }, { "query", str (query) },
                           { "fragment", str (fragment) } }));
  });
  prim (s, "URI.render", "Uri -> String", 1, [] (C &, V &a) {
    const Value &r = a[0];
    std::string out;
    if (!S (field (r, "scheme")).empty ())
      out += S (field (r, "scheme")) + ":";
    if (!S (field (r, "host")).empty () || !S (field (r, "user")).empty ())
      {
        out += "//";
        if (!S (field (r, "user")).empty ())
          out += S (field (r, "user")) + "@";
        out += S (field (r, "host"));
        if (field (r, "port").i >= 0)
          out += ":" + std::to_string (field (r, "port").i);
      }
    out += S (field (r, "path"));
    if (!S (field (r, "query")).empty ())
      out += "?" + S (field (r, "query"));
    if (!S (field (r, "fragment")).empty ())
      out += "#" + S (field (r, "fragment"));
    return str (out);
  });
  prim (s, "URI.encode", "String -> String", 1, [] (C &, V &a) { return str (encode (S (a[0]))); });
  prim (s, "URI.decode", "String -> Option String", 1, [] (C &, V &a) {
    auto r = decode (S (a[0]), false);
    return r ? some (str (*r)) : none ();
  });
  prim (s, "URI.query_params", "String -> [(String, String)]", 1, [] (C &, V &a) {
    std::vector<Value> out;
    for (auto &part : metaml::detail::split_string ("&", S (a[0])))
      {
        if (part.empty ())
          continue;
        auto eq = part.find ('=');
        std::string k = eq == std::string::npos ? part : part.substr (0, eq);
        std::string v = eq == std::string::npos ? "" : part.substr (eq + 1);
        auto dk = decode (k, true), dv = decode (v, true);
        out.push_back (pair (str (dk ? *dk : k), str (dv ? *dv : v)));
      }
    return list (out);
  });
}

} // namespace metalib
