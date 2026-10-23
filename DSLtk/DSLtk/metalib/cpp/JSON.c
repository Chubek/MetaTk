// metalib/cpp/JSON.c — implementation of JSON.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// JSON.h and must not be compiled on its own. Mirror: metalib/d/JSON.d.
#pragma once

namespace metalib
{


namespace json_detail
{
using metaml::Value;

inline void
declare_type (metaml::Session &s)
{
  if (s.checker ().global->types.find ("Json"))
    return;
  s.declare ("type Json := | JNull | JBool Bool | JNum Float | JStr String | JArr [Json] "
             "| JObj [(String, Json)]\n",
             "<metalib JSON>");
}

inline Value
jnull ()
{
  return metaml::make_ctor ("JNull", 0);
}
inline Value
jbool (bool b)
{
  return metaml::make_ctor ("JBool", 1, { Value::boolean (b) });
}
inline Value
jnum (double d)
{
  return metaml::make_ctor ("JNum", 2, { Value::floating (d) });
}
inline Value
jstr (std::string s)
{
  return metaml::make_ctor ("JStr", 3, { metaml::make_str (std::move (s)) });
}
inline Value
jarr (const std::vector<Value> &xs)
{
  return metaml::make_ctor ("JArr", 4, { metaml::make_list (xs) });
}
inline Value
jobj (const std::vector<std::pair<std::string, Value>> &kv)
{
  std::vector<Value> items;
  for (auto &p : kv)
    items.push_back (metaml::make_tuple ({ metaml::make_str (p.first), p.second }));
  return metaml::make_ctor ("JObj", 5, { metaml::make_list (items) });
}
inline std::string
tag (const Value &v)
{
  return v.kind == metaml::VK::Ctor ? metaml::agg_of (v)->tag : std::string ();
}
inline const Value &
payload (const Value &v)
{
  return metaml::agg_of (v)->items[0];
}
inline std::vector<std::pair<std::string, Value>>
members (const Value &obj)
{
  std::vector<std::pair<std::string, Value>> out;
  for (auto &p : metaml::list_items (payload (obj)))
    out.push_back ({ metaml::str_of (metaml::agg_of (p)->items[0]), metaml::agg_of (p)->items[1] });
  return out;
}

/// Integral values print without a fraction; NaN/inf are not JSON numbers.
inline std::string
number_text (double d)
{
  if (std::isnan (d) || std::isinf (d))
    return "null";
  if (d == std::floor (d) && std::fabs (d) < 1e15)
    return std::to_string (static_cast<std::int64_t> (d));
  return metaml::detail::format_float (d);
}

inline std::string
quote (const std::string &s)
{
  static const char *hex = "0123456789abcdef";
  std::string out = "\"";
  for (char ch : s)
    {
      auto c = static_cast<unsigned char> (ch);
      switch (c)
        {
        case '"':
          out += "\\\"";
          break;
        case '\\':
          out += "\\\\";
          break;
        case '\b':
          out += "\\b";
          break;
        case '\f':
          out += "\\f";
          break;
        case '\n':
          out += "\\n";
          break;
        case '\r':
          out += "\\r";
          break;
        case '\t':
          out += "\\t";
          break;
        default:
          if (c < 0x20)
            {
              out += "\\u00";
              out += hex[c >> 4];
              out += hex[c & 15];
            }
          else
            out += ch;
        }
    }
  return out + "\"";
}

inline void
render (const Value &v, bool pretty, int indent, std::string &out)
{
  std::string t = tag (v);
  std::string pad = pretty ? std::string (static_cast<std::size_t> (indent + 2), ' ') : "";
  std::string close = pretty ? std::string (static_cast<std::size_t> (indent), ' ') : "";
  if (t == "JNull")
    out += "null";
  else if (t == "JBool")
    out += payload (v).i ? "true" : "false";
  else if (t == "JNum")
    out += number_text (payload (v).f);
  else if (t == "JStr")
    out += quote (metaml::str_of (payload (v)));
  else if (t == "JArr")
    {
      auto xs = metaml::list_items (payload (v));
      if (xs.empty ())
        {
          out += "[]";
          return;
        }
      out += pretty ? "[\n" : "[";
      for (std::size_t i = 0; i < xs.size (); ++i)
        {
          if (i)
            out += pretty ? ",\n" : ",";
          out += pad;
          render (xs[i], pretty, indent + 2, out);
        }
      out += pretty ? "\n" + close + "]" : "]";
    }
  else if (t == "JObj")
    {
      auto kv = members (v);
      if (kv.empty ())
        {
          out += "{}";
          return;
        }
      out += pretty ? "{\n" : "{";
      for (std::size_t i = 0; i < kv.size (); ++i)
        {
          if (i)
            out += pretty ? ",\n" : ",";
          out += pad + quote (kv[i].first) + (pretty ? ": " : ":");
          render (kv[i].second, pretty, indent + 2, out);
        }
      out += pretty ? "\n" + close + "}" : "}";
    }
  else
    out += "null";
}

struct JsonFail
{
  std::string msg;
};

class Reader
{
public:
  explicit Reader (const std::string &s) : s_ (s) {}
  Value
  document ()
  {
    auto v = value (0);
    ws ();
    if (pos_ < s_.size ())
      fail ("trailing characters");
    return v;
  }

private:
  const std::string &s_;
  std::size_t pos_ = 0;

  [[noreturn]] void
  fail (const std::string &why)
  {
    throw JsonFail{ "JSON: " + why + " at offset " + std::to_string (pos_) };
  }
  void
  ws ()
  {
    while (pos_ < s_.size ()
           && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r'))
      ++pos_;
  }
  void
  literal (const char *word)
  {
    std::size_t n = std::strlen (word);
    if (s_.compare (pos_, n, word) != 0)
      fail ("invalid literal");
    pos_ += n;
  }
  Value
  value (int depth)
  {
    if (depth > 512)
      fail ("nesting too deep");
    ws ();
    if (pos_ >= s_.size ())
      fail ("unexpected end of input");
    char c = s_[pos_];
    if (c == 'n')
      {
        literal ("null");
        return jnull ();
      }
    if (c == 't')
      {
        literal ("true");
        return jbool (true);
      }
    if (c == 'f')
      {
        literal ("false");
        return jbool (false);
      }
    if (c == '"')
      return jstr (string ());
    if (c == '[')
      {
        ++pos_;
        std::vector<Value> xs;
        ws ();
        if (pos_ < s_.size () && s_[pos_] == ']')
          {
            ++pos_;
            return jarr (xs);
          }
        while (true)
          {
            xs.push_back (value (depth + 1));
            ws ();
            if (pos_ >= s_.size ())
              fail ("unexpected end of input");
            if (s_[pos_] == ',')
              {
                ++pos_;
                continue;
              }
            if (s_[pos_] == ']')
              {
                ++pos_;
                return jarr (xs);
              }
            fail ("expected `,` or `]`");
          }
      }
    if (c == '{')
      {
        ++pos_;
        std::vector<std::pair<std::string, Value>> kv;
        ws ();
        if (pos_ < s_.size () && s_[pos_] == '}')
          {
            ++pos_;
            return jobj (kv);
          }
        while (true)
          {
            ws ();
            if (pos_ >= s_.size () || s_[pos_] != '"')
              fail ("expected a string key");
            std::string k = string ();
            ws ();
            if (pos_ >= s_.size () || s_[pos_] != ':')
              fail ("expected `:`");
            ++pos_;
            Value v = value (depth + 1);
            kv.push_back ({ k, v });
            ws ();
            if (pos_ >= s_.size ())
              fail ("unexpected end of input");
            if (s_[pos_] == ',')
              {
                ++pos_;
                continue;
              }
            if (s_[pos_] == '}')
              {
                ++pos_;
                return jobj (kv);
              }
            fail ("expected `,` or `}`");
          }
      }
    if (c == '-' || (c >= '0' && c <= '9'))
      return number ();
    fail (std::string ("unexpected character `") + c + "`");
  }
  Value
  number ()
  {
    std::size_t b = pos_;
    if (s_[pos_] == '-')
      ++pos_;
    auto digit = [&] () { return pos_ < s_.size () && s_[pos_] >= '0' && s_[pos_] <= '9'; };
    if (!digit ())
      fail ("invalid number");
    if (s_[pos_] == '0')
      ++pos_;
    else
      while (digit ())
        ++pos_;
    if (pos_ < s_.size () && s_[pos_] == '.')
      {
        ++pos_;
        if (!digit ())
          fail ("invalid number");
        while (digit ())
          ++pos_;
      }
    if (pos_ < s_.size () && (s_[pos_] == 'e' || s_[pos_] == 'E'))
      {
        ++pos_;
        if (pos_ < s_.size () && (s_[pos_] == '+' || s_[pos_] == '-'))
          ++pos_;
        if (!digit ())
          fail ("invalid number");
        while (digit ())
          ++pos_;
      }
    std::string text = s_.substr (b, pos_ - b);
    return jnum (std::strtod (text.c_str (), nullptr));
  }
  int
  hex4 ()
  {
    if (pos_ + 4 > s_.size ())
      fail ("invalid escape");
    int v = 0;
    for (int k = 0; k < 4; ++k)
      {
        char h = s_[pos_++];
        int d = (h >= '0' && h <= '9')   ? h - '0'
                : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                : (h >= 'A' && h <= 'F') ? h - 'A' + 10
                                         : -1;
        if (d < 0)
          fail ("invalid escape");
        v = v * 16 + d;
      }
    return v;
  }
  std::string
  string ()
  {
    ++pos_;
    std::string out;
    while (true)
      {
        if (pos_ >= s_.size ())
          fail ("unterminated string");
        char c = s_[pos_++];
        if (c == '"')
          return out;
        if (static_cast<unsigned char> (c) < 0x20)
          fail ("control character in string");
        if (c != '\\')
          {
            out += c;
            continue;
          }
        if (pos_ >= s_.size ())
          fail ("unterminated string");
        char e = s_[pos_++];
        switch (e)
          {
          case '"':
            out += '"';
            break;
          case '\\':
            out += '\\';
            break;
          case '/':
            out += '/';
            break;
          case 'b':
            out += '\b';
            break;
          case 'f':
            out += '\f';
            break;
          case 'n':
            out += '\n';
            break;
          case 'r':
            out += '\r';
            break;
          case 't':
            out += '\t';
            break;
          case 'u':
            {
              int cp = hex4 ();
              if (cp >= 0xD800 && cp <= 0xDBFF && pos_ + 1 < s_.size () && s_[pos_] == '\\'
                  && s_[pos_ + 1] == 'u')
                {
                  pos_ += 2;
                  int lo = hex4 ();
                  if (lo >= 0xDC00 && lo <= 0xDFFF)
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                  else
                    cp = 0xFFFD;
                }
              if (!metaml::detail::append_utf8 (out, static_cast<std::uint32_t> (cp)))
                metaml::detail::append_utf8 (out, 0xFFFD);
              break;
            }
          default:
            fail ("invalid escape");
          }
      }
  }
};

inline Value
of_value (const Value &v)
{
  using metaml::VK;
  switch (v.kind)
    {
    case VK::Unit:
      return jnull ();
    case VK::Bool:
      return jbool (v.i != 0);
    case VK::Int:
      return jnum (static_cast<double> (v.i));
    case VK::Float:
      return jnum (v.f);
    case VK::Str:
      return jstr (metaml::str_of (v));
    case VK::List:
      {
        std::vector<Value> xs;
        for (auto &x : metaml::list_items (v))
          xs.push_back (of_value (x));
        return jarr (xs);
      }
    case VK::Tuple:
      {
        std::vector<Value> xs;
        for (auto &x : metaml::agg_of (v)->items)
          xs.push_back (of_value (x));
        return jarr (xs);
      }
    case VK::Record:
      {
        std::vector<std::pair<std::string, Value>> kv;
        auto a = metaml::agg_of (v);
        for (std::size_t i = 0; i < a->items.size (); ++i)
          kv.push_back ({ (*a->labels)[i], of_value (a->items[i]) });
        return jobj (kv);
      }
    case VK::Ctor:
      {
        std::string t = tag (v);
        if (t == "JNull" || t == "JBool" || t == "JNum" || t == "JStr" || t == "JArr" || t == "JObj")
          return v;
        auto a = metaml::agg_of (v);
        if (t == "None")
          return jnull ();
        if (t == "Some" && a->items.size () == 1)
          return of_value (a->items[0]);
        std::vector<Value> args;
        for (auto &x : a->items)
          args.push_back (of_value (x));
        return jobj ({ { "tag", jstr (t) }, { "args", jarr (args) } });
      }
    default:
      return jstr (metaml::show_value (v));
    }
}
} // namespace json_detail

inline void
install_JSON (metaml::Session &s)
{
  using namespace support;
  using namespace json_detail;
  declare_type (s);
  prim (s, "JSON.parse", "String -> Result Json String", 1, [] (C &, V &a) {
    try
      {
        Reader r (S (a[0]));
        return ok (r.document ());
      }
    catch (const JsonFail &f)
      {
        return err (f.msg);
      }
  });
  prim (s, "JSON.render", "Json -> String", 1, [] (C &, V &a) {
    std::string out;
    render (a[0], false, 0, out);
    return str (out);
  });
  prim (s, "JSON.pretty", "Json -> String", 1, [] (C &, V &a) {
    std::string out;
    render (a[0], true, 0, out);
    return str (out);
  });
  prim (s, "JSON.get", "String -> Json -> Option Json", 2, [] (C &, V &a) {
    if (tag (a[1]) != "JObj")
      return none ();
    for (auto &kv : members (a[1]))
      if (kv.first == S (a[0]))
        return some (kv.second);
    return none ();
  });
  prim (s, "JSON.at", "Int -> Json -> Option Json", 2, [] (C &, V &a) {
    if (tag (a[1]) != "JArr")
      return none ();
    auto xs = items (payload (a[1]));
    if (a[0].i < 0 || a[0].i >= static_cast<std::int64_t> (xs.size ()))
      return none ();
    return some (xs[static_cast<std::size_t> (a[0].i)]);
  });
  prim (s, "JSON.of_value", "'a -> Json", 1, [] (C &, V &a) { return of_value (a[0]); });
}

} // namespace metalib
