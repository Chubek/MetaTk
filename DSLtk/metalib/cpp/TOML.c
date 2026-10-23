// metalib/cpp/TOML.c — implementation of TOML.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// TOML.h and must not be compiled on its own. Mirror: metalib/d/TOML.d.
#pragma once

namespace metalib
{

namespace toml_detail
{
using metaml::Value;
using namespace json_detail;

struct Node
{
  int kind = 0; // 0 table, 1 array, 2 value
  std::vector<std::pair<std::string, std::shared_ptr<Node>>> kv{};
  std::vector<std::shared_ptr<Node>> items{};
  Value value{};
  bool explicit_table = false;
  bool aot = false;    ///< array of tables
  bool frozen = false; ///< inline table / value array (cannot be extended)
  std::shared_ptr<Node>
  find (const std::string &k) const
  {
    for (auto &p : kv)
      if (p.first == k)
        return p.second;
    return nullptr;
  }
};
using NodeP = std::shared_ptr<Node>;

struct TomlFail
{
  std::string msg;
};

class Parser
{
public:
  explicit Parser (const std::string &s) : s_ (s) {}

  Value
  document ()
  {
    auto root = std::make_shared<Node> ();
    NodeP current = root;
    while (true)
      {
        skip_blank_lines ();
        if (pos_ >= s_.size ())
          break;
        if (s_[pos_] == '[')
          {
            bool aot = pos_ + 1 < s_.size () && s_[pos_ + 1] == '[';
            pos_ += aot ? 2 : 1;
            auto path = key_path ();
            spaces ();
            if (aot)
              {
                if (s_.compare (pos_, 2, "]]") != 0)
                  fail ("expected `]]`");
                pos_ += 2;
              }
            else
              {
                if (pos_ >= s_.size () || s_[pos_] != ']')
                  fail ("expected `]`");
                ++pos_;
              }
            current = aot ? open_aot (root, path) : open_table (root, path);
          }
        else
          {
            auto path = key_path ();
            spaces ();
            if (pos_ >= s_.size () || s_[pos_] != '=')
              fail ("expected `=` after the key");
            ++pos_;
            spaces ();
            auto v = value ();
            insert (current, path, v);
          }
        end_of_line ();
      }
    return convert (root);
  }

private:
  const std::string &s_;
  std::size_t pos_ = 0;

  [[noreturn]] void
  fail (const std::string &why)
  {
    std::size_t line = 1;
    for (std::size_t i = 0; i < pos_ && i < s_.size (); ++i)
      line += s_[i] == '\n';
    throw TomlFail{ "TOML: " + why + " at line " + std::to_string (line) };
  }
  void
  spaces ()
  {
    while (pos_ < s_.size () && (s_[pos_] == ' ' || s_[pos_] == '\t'))
      ++pos_;
  }
  void
  comment ()
  {
    if (pos_ < s_.size () && s_[pos_] == '#')
      while (pos_ < s_.size () && s_[pos_] != '\n')
        ++pos_;
  }
  void
  skip_blank_lines ()
  {
    while (pos_ < s_.size ())
      {
        spaces ();
        comment ();
        if (pos_ < s_.size () && (s_[pos_] == '\n' || s_[pos_] == '\r'))
          ++pos_;
        else
          break;
      }
  }
  void
  skip_ws_comments ()
  {
    while (pos_ < s_.size ())
      {
        char c = s_[pos_];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
          ++pos_;
        else if (c == '#')
          comment ();
        else
          break;
      }
  }
  void
  end_of_line ()
  {
    spaces ();
    comment ();
    if (pos_ < s_.size () && s_[pos_] == '\r')
      ++pos_;
    if (pos_ < s_.size () && s_[pos_] != '\n')
      fail ("expected a newline after the value");
    if (pos_ < s_.size ())
      ++pos_;
  }
  static bool
  bare (char c)
  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'
           || c == '-';
  }
  std::string
  key ()
  {
    spaces ();
    if (pos_ < s_.size () && s_[pos_] == '"')
      return basic_string ();
    if (pos_ < s_.size () && s_[pos_] == '\'')
      return literal_string ();
    std::size_t b = pos_;
    while (pos_ < s_.size () && bare (s_[pos_]))
      ++pos_;
    if (b == pos_)
      fail ("expected a key");
    return s_.substr (b, pos_ - b);
  }
  std::vector<std::string>
  key_path ()
  {
    std::vector<std::string> path{ key () };
    while (true)
      {
        spaces ();
        if (pos_ < s_.size () && s_[pos_] == '.')
          {
            ++pos_;
            path.push_back (key ());
          }
        else
          return path;
      }
  }
  NodeP
  descend (const NodeP &t, const std::string &k)
  {
    auto n = t->find (k);
    if (!n)
      {
        n = std::make_shared<Node> ();
        t->kv.push_back ({ k, n });
        return n;
      }
    if (n->kind == 1 && n->aot && !n->items.empty ())
      return n->items.back ();
    if (n->kind != 0 || n->frozen)
      fail ("key `" + k + "` is not a table");
    return n;
  }
  NodeP
  open_table (const NodeP &root, const std::vector<std::string> &path)
  {
    NodeP t = root;
    for (std::size_t i = 0; i + 1 < path.size (); ++i)
      t = descend (t, path[i]);
    auto n = t->find (path.back ());
    if (n)
      {
        if (n->kind != 0 || n->explicit_table || n->frozen)
          fail ("duplicate table `" + path.back () + "`");
        n->explicit_table = true;
        return n;
      }
    n = std::make_shared<Node> ();
    n->explicit_table = true;
    t->kv.push_back ({ path.back (), n });
    return n;
  }
  NodeP
  open_aot (const NodeP &root, const std::vector<std::string> &path)
  {
    NodeP t = root;
    for (std::size_t i = 0; i + 1 < path.size (); ++i)
      t = descend (t, path[i]);
    auto n = t->find (path.back ());
    if (!n)
      {
        n = std::make_shared<Node> ();
        n->kind = 1;
        n->aot = true;
        t->kv.push_back ({ path.back (), n });
      }
    else if (n->kind != 1 || !n->aot)
      fail ("key `" + path.back () + "` is not an array of tables");
    auto e = std::make_shared<Node> ();
    e->explicit_table = true;
    n->items.push_back (e);
    return e;
  }
  void
  insert (const NodeP &table, const std::vector<std::string> &path, const NodeP &v)
  {
    NodeP t = table;
    for (std::size_t i = 0; i + 1 < path.size (); ++i)
      t = descend (t, path[i]);
    if (t->find (path.back ()))
      fail ("duplicate key `" + path.back () + "`");
    t->kv.push_back ({ path.back (), v });
  }
  static NodeP
  leaf (Value v)
  {
    auto n = std::make_shared<Node> ();
    n->kind = 2;
    n->value = std::move (v);
    return n;
  }
  int
  hexn (int n)
  {
    if (pos_ + static_cast<std::size_t> (n) > s_.size ())
      fail ("invalid unicode escape");
    int v = 0;
    for (int k = 0; k < n; ++k)
      {
        char h = s_[pos_++];
        int d = (h >= '0' && h <= '9')   ? h - '0'
                : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                : (h >= 'A' && h <= 'F') ? h - 'A' + 10
                                         : -1;
        if (d < 0)
          fail ("invalid unicode escape");
        v = v * 16 + d;
      }
    return v;
  }
  void
  escape (std::string &out)
  {
    if (pos_ >= s_.size ())
      fail ("unterminated string");
    char e = s_[pos_++];
    switch (e)
      {
      case 'b':
        out += '\b';
        break;
      case 't':
        out += '\t';
        break;
      case 'n':
        out += '\n';
        break;
      case 'f':
        out += '\f';
        break;
      case 'r':
        out += '\r';
        break;
      case '"':
        out += '"';
        break;
      case '\\':
        out += '\\';
        break;
      case 'u':
      case 'U':
        {
          int cp = hexn (e == 'u' ? 4 : 8);
          if (!metaml::detail::append_utf8 (out, static_cast<std::uint32_t> (cp)))
            fail ("invalid unicode escape");
          break;
        }
      default:
        fail (std::string ("invalid escape `\\") + e + "`");
      }
  }
  std::string
  basic_string ()
  {
    if (s_.compare (pos_, 3, "\"\"\"") == 0)
      {
        pos_ += 3;
        if (pos_ < s_.size () && s_[pos_] == '\n')
          ++pos_;
        else if (s_.compare (pos_, 2, "\r\n") == 0)
          pos_ += 2;
        std::string out;
        while (true)
          {
            if (pos_ >= s_.size ())
              fail ("unterminated string");
            if (s_.compare (pos_, 3, "\"\"\"") == 0)
              {
                pos_ += 3;
                while (pos_ < s_.size () && s_[pos_] == '"')
                  {
                    out += '"';
                    ++pos_;
                  }
                return out;
              }
            char c = s_[pos_++];
            if (c == '\\')
              {
                std::size_t q = pos_;
                while (q < s_.size () && (s_[q] == ' ' || s_[q] == '\t'))
                  ++q;
                if (q < s_.size () && (s_[q] == '\n' || s_[q] == '\r'))
                  {
                    pos_ = q;
                    while (pos_ < s_.size ()
                           && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n'
                               || s_[pos_] == '\r'))
                      ++pos_;
                    continue;
                  }
                escape (out);
              }
            else
              out += c;
          }
      }
    ++pos_;
    std::string out;
    while (true)
      {
        if (pos_ >= s_.size () || s_[pos_] == '\n')
          fail ("unterminated string");
        char c = s_[pos_++];
        if (c == '"')
          return out;
        if (c == '\\')
          escape (out);
        else
          out += c;
      }
  }
  std::string
  literal_string ()
  {
    if (s_.compare (pos_, 3, "'''") == 0)
      {
        pos_ += 3;
        if (pos_ < s_.size () && s_[pos_] == '\n')
          ++pos_;
        else if (s_.compare (pos_, 2, "\r\n") == 0)
          pos_ += 2;
        auto e = s_.find ("'''", pos_);
        if (e == std::string::npos)
          fail ("unterminated string");
        std::string out = s_.substr (pos_, e - pos_);
        pos_ = e + 3;
        while (pos_ < s_.size () && s_[pos_] == '\'')
          {
            out += '\'';
            ++pos_;
          }
        return out;
      }
    ++pos_;
    auto e = s_.find ('\'', pos_);
    auto nl = s_.find ('\n', pos_);
    if (e == std::string::npos || (nl != std::string::npos && nl < e))
      fail ("unterminated string");
    std::string out = s_.substr (pos_, e - pos_);
    pos_ = e + 1;
    return out;
  }
  NodeP
  value ()
  {
    if (pos_ >= s_.size ())
      fail ("expected a value");
    char c = s_[pos_];
    if (c == '"')
      return leaf (jstr (basic_string ()));
    if (c == '\'')
      return leaf (jstr (literal_string ()));
    if (c == '[')
      {
        ++pos_;
        auto n = std::make_shared<Node> ();
        n->kind = 1;
        n->frozen = true;
        while (true)
          {
            skip_ws_comments ();
            if (pos_ < s_.size () && s_[pos_] == ']')
              {
                ++pos_;
                return n;
              }
            n->items.push_back (value ());
            skip_ws_comments ();
            if (pos_ < s_.size () && s_[pos_] == ',')
              {
                ++pos_;
                continue;
              }
            if (pos_ < s_.size () && s_[pos_] == ']')
              {
                ++pos_;
                return n;
              }
            fail ("expected `,` or `]` in array");
          }
      }
    if (c == '{')
      {
        ++pos_;
        auto n = std::make_shared<Node> ();
        spaces ();
        if (pos_ < s_.size () && s_[pos_] == '}')
          {
            ++pos_;
            n->frozen = true;
            return n;
          }
        while (true)
          {
            auto path = key_path ();
            spaces ();
            if (pos_ >= s_.size () || s_[pos_] != '=')
              fail ("expected `=` in inline table");
            ++pos_;
            spaces ();
            auto v = value ();
            insert (n, path, v);
            spaces ();
            if (pos_ < s_.size () && s_[pos_] == ',')
              {
                ++pos_;
                spaces ();
                continue;
              }
            if (pos_ < s_.size () && s_[pos_] == '}')
              {
                ++pos_;
                n->frozen = true;
                return n;
              }
            fail ("expected `,` or `}` in inline table");
          }
      }
    std::size_t b = pos_;
    while (pos_ < s_.size ())
      {
        char d = s_[pos_];
        if (d == ' ' || d == '\t' || d == '\n' || d == '\r' || d == ',' || d == ']' || d == '}'
            || d == '#')
          break;
        ++pos_;
      }
    std::string tok = s_.substr (b, pos_ - b);
    if (tok.empty ())
      fail ("expected a value");
    return leaf (scalar (tok));
  }
  Value
  scalar (const std::string &tok)
  {
    if (tok == "true")
      return jbool (true);
    if (tok == "false")
      return jbool (false);
    if (tok == "inf" || tok == "+inf")
      return jnum (HUGE_VAL);
    if (tok == "-inf")
      return jnum (-HUGE_VAL);
    if (tok == "nan" || tok == "+nan" || tok == "-nan")
      return jnum (NAN);
    std::string body = tok;
    bool neg = false;
    if (!body.empty () && (body[0] == '+' || body[0] == '-'))
      {
        neg = body[0] == '-';
        body = body.substr (1);
      }
    if (body.size () > 2 && body[0] == '0' && body[1] == 'o')
      {
        std::int64_t v = 0;
        for (std::size_t k = 2; k < body.size (); ++k)
          {
            if (body[k] == '_')
              continue;
            if (body[k] < '0' || body[k] > '7')
              fail ("invalid integer `" + tok + "`");
            v = v * 8 + (body[k] - '0');
          }
        return jnum (static_cast<double> (neg ? -v : v));
      }
    if (body.size () > 2 && body[0] == '0' && (body[1] == 'x' || body[1] == 'b'))
      {
        auto v = metaml::detail::parse_int (body);
        if (!v)
          fail ("invalid integer `" + tok + "`");
        return jnum (static_cast<double> (neg ? -*v : *v));
      }
    bool digits_only = !body.empty ();
    bool has_dot = false, has_exp = false, other = false;
    for (char d : body)
      {
        if (d == '.')
          has_dot = true;
        else if (d == 'e' || d == 'E')
          has_exp = true;
        else if (!((d >= '0' && d <= '9') || d == '_'))
          other = true;
      }
    (void)digits_only;
    if (body.size () >= 5 && body[0] >= '0' && body[0] <= '9'
        && (body.find (':') != std::string::npos || body.find ('-') != std::string::npos
            || body.find ('T') != std::string::npos))
      return jstr (tok);
    if (other && !(has_exp && body.find_first_of ("+-") != std::string::npos))
      fail ("invalid value `" + tok + "`");
    if (has_dot || has_exp)
      {
        auto f = metaml::detail::parse_float (tok);
        if (!f)
          fail ("invalid float `" + tok + "`");
        return jnum (*f);
      }
    auto v = metaml::detail::parse_int (tok);
    if (!v)
      fail ("invalid integer `" + tok + "`");
    return jnum (static_cast<double> (*v));
  }
  Value
  convert (const NodeP &n)
  {
    if (n->kind == 2)
      return n->value;
    if (n->kind == 1)
      {
        std::vector<Value> xs;
        for (auto &i : n->items)
          xs.push_back (convert (i));
        return jarr (xs);
      }
    std::vector<std::pair<std::string, Value>> kv;
    for (auto &p : n->kv)
      kv.push_back ({ p.first, convert (p.second) });
    return jobj (kv);
  }
};

inline bool
bare_key (const std::string &k)
{
  if (k.empty ())
    return false;
  for (char c : k)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'
          || c == '-'))
      return false;
  return true;
}
inline std::string
key_text (const std::string &k)
{
  return bare_key (k) ? k : json_detail::quote (k);
}
inline bool
is_table_array (const Value &v)
{
  if (tag (v) != "JArr")
    return false;
  auto xs = metaml::list_items (payload (v));
  if (xs.empty ())
    return false;
  for (auto &x : xs)
    if (tag (x) != "JObj")
      return false;
  return true;
}
inline std::string
value_text (const Value &v)
{
  std::string t = tag (v);
  if (t == "JStr")
    return json_detail::quote (metaml::str_of (payload (v)));
  if (t == "JBool")
    return payload (v).i ? "true" : "false";
  if (t == "JNum")
    {
      double d = payload (v).f;
      if (std::isnan (d))
        return "nan";
      if (std::isinf (d))
        return d < 0 ? "-inf" : "inf";
      if (d == std::floor (d) && std::fabs (d) < 1e15)
        return std::to_string (static_cast<std::int64_t> (d));
      return metaml::detail::format_float (d);
    }
  if (t == "JArr")
    {
      std::string out = "[";
      auto xs = metaml::list_items (payload (v));
      for (std::size_t i = 0; i < xs.size (); ++i)
        out += (i ? ", " : "") + value_text (xs[i]);
      return out + "]";
    }
  if (t == "JObj")
    {
      auto kv = members (v);
      if (kv.empty ())
        return "{}";
      std::string out = "{ ";
      for (std::size_t i = 0; i < kv.size (); ++i)
        out += (i ? ", " : "") + key_text (kv[i].first) + " = " + value_text (kv[i].second);
      return out + " }";
    }
  throw TomlFail{ "TOML: cannot represent null" };
}
inline void
emit (const Value &obj, const std::string &path, std::string &out)
{
  auto kv = members (obj);
  for (auto &p : kv)
    if (tag (p.second) != "JObj" && !is_table_array (p.second))
      out += key_text (p.first) + " = " + value_text (p.second) + "\n";
  for (auto &p : kv)
    {
      std::string sub = path.empty () ? key_text (p.first) : path + "." + key_text (p.first);
      if (tag (p.second) == "JObj")
        {
          if (!out.empty ())
            out += "\n";
          out += "[" + sub + "]\n";
          emit (p.second, sub, out);
        }
      else if (is_table_array (p.second))
        for (auto &x : metaml::list_items (payload (p.second)))
          {
            if (!out.empty ())
              out += "\n";
            out += "[[" + sub + "]]\n";
            emit (x, sub, out);
          }
    }
}
} // namespace toml_detail

inline void
install_TOML (metaml::Session &s)
{
  using namespace support;
  using namespace toml_detail;
  json_detail::declare_type (s);
  prim (s, "TOML.parse", "String -> Result Json String", 1, [] (C &, V &a) {
    try
      {
        Parser p (S (a[0]));
        return ok (p.document ());
      }
    catch (const TomlFail &f)
      {
        return err (f.msg);
      }
  });
  prim (s, "TOML.render", "Json -> Result String String", 1, [] (C &, V &a) {
    if (json_detail::tag (a[0]) != "JObj")
      return err ("TOML: the top level must be an object");
    try
      {
        std::string out;
        emit (a[0], "", out);
        return ok (str (out));
      }
    catch (const TomlFail &f)
      {
        return err (f.msg);
      }
  });
}

} // namespace metalib
