// metalib/cpp/YAML.c — implementation of YAML.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// YAML.h and must not be compiled on its own. Mirror: metalib/d/YAML.d.
#pragma once

namespace metalib
{

namespace yaml_detail
{
using metaml::Value;
using namespace json_detail;

struct YamlFail
{
  std::string msg;
};

struct Line
{
  int indent = 0;
  std::string text; ///< content without indentation and comments
  std::size_t no = 0;
  bool content = false;
};

inline std::string
rtrim (std::string s)
{
  while (!s.empty () && (s.back () == ' ' || s.back () == '\t' || s.back () == '\r'))
    s.pop_back ();
  return s;
}
inline std::string
trim (const std::string &s)
{
  std::size_t b = 0;
  while (b < s.size () && (s[b] == ' ' || s[b] == '\t'))
    ++b;
  return rtrim (s.substr (b));
}

/// Removes a trailing ` # comment` outside quotes.
inline std::string
strip_comment (const std::string &s)
{
  char q = 0;
  for (std::size_t i = 0; i < s.size (); ++i)
    {
      char c = s[i];
      if (q)
        {
          if (q == '"' && c == '\\')
            ++i;
          else if (c == q)
            q = 0;
          continue;
        }
      if (c == '"' || c == '\'')
        {
          if (i == 0 || s[i - 1] == ' ' || s[i - 1] == '\t' || s[i - 1] == '[' || s[i - 1] == '{'
              || s[i - 1] == ',' || s[i - 1] == ':' || s[i - 1] == '-')
            q = c;
          continue;
        }
      if (c == '#' && (i == 0 || s[i - 1] == ' ' || s[i - 1] == '\t'))
        return rtrim (s.substr (0, i));
    }
  return rtrim (s);
}

class Parser
{
public:
  explicit Parser (const std::string &src)
  {
    std::size_t start = 0, no = 1;
    while (start <= src.size ())
      {
        auto e = src.find ('\n', start);
        std::string raw = src.substr (start, e == std::string::npos ? std::string::npos : e - start);
        if (!raw.empty () && raw.back () == '\r')
          raw.pop_back ();
        raw_.push_back (raw);
        Line l;
        l.no = no;
        std::size_t k = 0;
        while (k < raw.size () && raw[k] == ' ')
          ++k;
        if (k < raw.size () && raw[k] == '\t')
          {
            cur_ = lines_.size ();
            lines_.push_back (l);
            fail_line (no, "tabs are not allowed for indentation");
          }
        l.indent = static_cast<int> (k);
        l.text = strip_comment (raw.substr (k));
        l.content = !l.text.empty () && l.text != "---" && l.text != "...";
        if (l.text.compare (0, 4, "--- ") == 0)
          {
            l.text = trim (l.text.substr (4));
            l.content = !l.text.empty ();
          }
        if (!l.text.empty () && l.text[0] == '%')
          l.content = false;
        lines_.push_back (l);
        if (e == std::string::npos)
          break;
        start = e + 1;
        ++no;
      }
  }

  Value
  document ()
  {
    std::size_t i = next_content (0);
    if (i >= lines_.size ())
      return jnull ();
    Value v = block (i, lines_[i].indent);
    i = next_content (i);
    if (i < lines_.size ())
      fail_line (lines_[i].no, "unexpected content (bad indentation?)");
    return v;
  }

private:
  std::vector<Line> lines_{};
  std::vector<std::string> raw_{};
  std::size_t cur_ = 0;

  [[noreturn]] void
  fail_line (std::size_t no, const std::string &why)
  {
    throw YamlFail{ "YAML: " + why + " at line " + std::to_string (no) };
  }
  [[noreturn]] void
  fail (std::size_t i, const std::string &why)
  {
    fail_line (i < lines_.size () ? lines_[i].no : lines_.size (), why);
  }
  std::size_t
  next_content (std::size_t i) const
  {
    while (i < lines_.size () && !lines_[i].content)
      ++i;
    return i;
  }
  static bool
  is_seq (const std::string &t)
  {
    return t == "-" || t.compare (0, 2, "- ") == 0;
  }
  /// Finds the `:` separating a mapping key; npos if not an entry.
  static std::size_t
  entry_colon (const std::string &t)
  {
    if (t.empty () || t[0] == '[' || t[0] == '{' || is_seq (t))
      return std::string::npos;
    std::size_t i = 0;
    if (t[0] == '"' || t[0] == '\'')
      {
        char q = t[0];
        for (i = 1; i < t.size (); ++i)
          {
            if (q == '"' && t[i] == '\\')
              {
                ++i;
                continue;
              }
            if (t[i] == q)
              {
                if (q == '\'' && i + 1 < t.size () && t[i + 1] == '\'')
                  {
                    ++i;
                    continue;
                  }
                break;
              }
          }
        ++i;
        while (i < t.size () && t[i] == ' ')
          ++i;
        if (i < t.size () && t[i] == ':' && (i + 1 == t.size () || t[i + 1] == ' '))
          return i;
        return std::string::npos;
      }
    for (; i < t.size (); ++i)
      if (t[i] == ':' && (i + 1 == t.size () || t[i + 1] == ' '))
        return i;
    return std::string::npos;
  }
  Value
  block (std::size_t &i, int indent)
  {
    const auto &t = lines_[i].text;
    if (is_seq (t))
      return sequence (i, indent);
    if (entry_colon (t) != std::string::npos)
      return mapping (i, indent);
    return plain_lines (i, indent);
  }
  Value
  sequence (std::size_t &i, int indent)
  {
    std::vector<Value> xs;
    while (i < lines_.size () && lines_[i].content && lines_[i].indent == indent
           && is_seq (lines_[i].text))
      {
        std::string rest = lines_[i].text == "-" ? "" : lines_[i].text.substr (2);
        std::size_t lead = 0;
        while (lead < rest.size () && rest[lead] == ' ')
          ++lead;
        rest = rest.substr (lead);
        if (rest.empty ())
          {
            std::size_t j = next_content (i + 1);
            if (j < lines_.size () && lines_[j].indent > indent)
              {
                i = j;
                xs.push_back (block (i, lines_[j].indent));
              }
            else
              {
                xs.push_back (jnull ());
                i = j;
              }
          }
        else
          {
            lines_[i].indent = indent + 2 + static_cast<int> (lead);
            lines_[i].text = rest;
            xs.push_back (block (i, lines_[i].indent));
          }
        i = next_content (i);
      }
    return jarr (xs);
  }
  Value
  mapping (std::size_t &i, int indent)
  {
    std::vector<std::pair<std::string, Value>> kv;
    while (i < lines_.size () && lines_[i].content && lines_[i].indent == indent)
      {
        const std::string t = lines_[i].text;
        auto c = entry_colon (t);
        if (c == std::string::npos)
          fail (i, "expected a mapping entry `key: value`");
        std::string key = scalar_key (trim (t.substr (0, c)), i);
        std::string rest = trim (t.substr (c + 1));
        std::size_t here = i;
        Value v;
        if (rest.empty ())
          {
            std::size_t j = next_content (i + 1);
            if (j < lines_.size () && lines_[j].indent > indent)
              {
                i = j;
                v = block (i, lines_[j].indent);
              }
            else if (j < lines_.size () && lines_[j].indent == indent && is_seq (lines_[j].text))
              {
                i = j;
                v = sequence (i, indent);
              }
            else
              {
                v = jnull ();
                i = j;
              }
          }
        else if (rest[0] == '|' || rest[0] == '>')
          v = block_scalar (i, indent, rest);
        else
          v = inline_value (i, indent, rest);
        (void)here;
        bool replaced = false;
        for (auto &p : kv)
          if (p.first == key)
            {
              p.second = v;
              replaced = true;
            }
        if (!replaced)
          kv.push_back ({ key, v });
        i = next_content (i);
      }
    return jobj (kv);
  }
  std::string
  scalar_key (const std::string &k, std::size_t i)
  {
    if (!k.empty () && (k[0] == '"' || k[0] == '\''))
      {
        Value v = quoted (k, i);
        return metaml::str_of (payload (v));
      }
    return k;
  }
  /// A value on the entry line; flow collections may continue on later lines,
  /// plain scalars may continue on more-indented lines.
  Value
  inline_value (std::size_t &i, int indent, std::string text)
  {
    if (text[0] == '[' || text[0] == '{')
      {
        while (!balanced (text))
          {
            std::size_t j = next_content (i + 1);
            if (j >= lines_.size ())
              fail (i, "unterminated flow collection");
            i = j;
            text += " " + lines_[j].text;
          }
        std::size_t p = 0;
        Value v = flow (text, p, i);
        while (p < text.size () && text[p] == ' ')
          ++p;
        if (p != text.size ())
          fail (i, "unexpected text after a flow collection");
        i = i + 1;
        return v;
      }
    if (text[0] == '"' || text[0] == '\'')
      {
        Value v = quoted (text, i);
        i = i + 1;
        return v;
      }
    std::string acc = text;
    std::size_t j = next_content (i + 1);
    while (j < lines_.size () && lines_[j].indent > indent && !is_seq (lines_[j].text)
           && entry_colon (lines_[j].text) == std::string::npos)
      {
        acc += " " + lines_[j].text;
        i = j;
        j = next_content (i + 1);
      }
    i = i + 1;
    return plain (acc, i - 1);
  }
  Value
  plain_lines (std::size_t &i, int indent)
  {
    std::string text = lines_[i].text;
    if (text[0] == '[' || text[0] == '{' || text[0] == '"' || text[0] == '\'')
      return inline_value (i, indent - 1, text);
    if (text[0] == '|' || text[0] == '>')
      return block_scalar (i, indent - 1, text);
    return inline_value (i, indent - 1, text);
  }
  static bool
  balanced (const std::string &t)
  {
    int d = 0;
    char q = 0;
    for (std::size_t i = 0; i < t.size (); ++i)
      {
        char c = t[i];
        if (q)
          {
            if (q == '"' && c == '\\')
              ++i;
            else if (c == q)
              q = 0;
            continue;
          }
        if (c == '"' || c == '\'')
          q = c;
        else if (c == '[' || c == '{')
          ++d;
        else if (c == ']' || c == '}')
          --d;
      }
    return d <= 0;
  }
  Value
  quoted (const std::string &t, std::size_t i)
  {
    char q = t[0];
    std::string out;
    std::size_t k = 1;
    for (; k < t.size (); ++k)
      {
        char c = t[k];
        if (q == '\'' && c == '\'')
          {
            if (k + 1 < t.size () && t[k + 1] == '\'')
              {
                out += '\'';
                ++k;
                continue;
              }
            break;
          }
        if (q == '"' && c == '"')
          break;
        if (q == '"' && c == '\\' && k + 1 < t.size ())
          {
            char e = t[++k];
            switch (e)
              {
              case 'n':
                out += '\n';
                break;
              case 't':
                out += '\t';
                break;
              case 'r':
                out += '\r';
                break;
              case '0':
                out += '\0';
                break;
              case '"':
                out += '"';
                break;
              case '\\':
                out += '\\';
                break;
              case '/':
                out += '/';
                break;
              case ' ':
                out += ' ';
                break;
              case 'x':
              case 'u':
                {
                  int n = e == 'x' ? 2 : 4;
                  if (k + static_cast<std::size_t> (n) >= t.size ())
                    fail (i, "invalid escape");
                  std::uint32_t cp = 0;
                  for (int m = 0; m < n; ++m)
                    {
                      char h = t[++k];
                      int dv = (h >= '0' && h <= '9')   ? h - '0'
                               : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                               : (h >= 'A' && h <= 'F') ? h - 'A' + 10
                                                        : -1;
                      if (dv < 0)
                        fail (i, "invalid escape");
                      cp = cp * 16 + static_cast<std::uint32_t> (dv);
                    }
                  if (!metaml::detail::append_utf8 (out, cp))
                    metaml::detail::append_utf8 (out, 0xFFFD);
                  break;
                }
              default:
                fail (i, std::string ("invalid escape `\\") + e + "`");
              }
            continue;
          }
        out += c;
      }
    if (k >= t.size ())
      fail (i, "unterminated quoted scalar");
    if (trim (t.substr (k + 1)) != "")
      fail (i, "unexpected text after a quoted scalar");
    return jstr (out);
  }
  static bool
  is_int (const std::string &s)
  {
    std::size_t k = 0;
    if (k < s.size () && (s[k] == '-' || s[k] == '+'))
      ++k;
    if (k >= s.size ())
      return false;
    for (; k < s.size (); ++k)
      if (s[k] < '0' || s[k] > '9')
        return false;
    return true;
  }
  static bool
  is_float (const std::string &s)
  {
    std::size_t k = 0;
    if (k < s.size () && (s[k] == '-' || s[k] == '+'))
      ++k;
    bool digits = false, dot = false;
    while (k < s.size () && ((s[k] >= '0' && s[k] <= '9') || s[k] == '.'))
      {
        if (s[k] == '.')
          {
            if (dot)
              return false;
            dot = true;
          }
        else
          digits = true;
        ++k;
      }
    if (!digits)
      return false;
    bool exp = false;
    if (k < s.size () && (s[k] == 'e' || s[k] == 'E'))
      {
        exp = true;
        ++k;
        if (k < s.size () && (s[k] == '-' || s[k] == '+'))
          ++k;
        if (k >= s.size ())
          return false;
        for (; k < s.size (); ++k)
          if (s[k] < '0' || s[k] > '9')
            return false;
      }
    return k == s.size () && (dot || exp);
  }
  Value
  plain (const std::string &t0, std::size_t i)
  {
    std::string t = trim (t0);
    if (!t.empty () && (t[0] == '&' || t[0] == '*' || t[0] == '!'))
      fail (i, "anchors, aliases and tags are not supported");
    if (t.empty () || t == "~" || t == "null" || t == "Null" || t == "NULL")
      return jnull ();
    if (t == "true" || t == "True" || t == "TRUE")
      return jbool (true);
    if (t == "false" || t == "False" || t == "FALSE")
      return jbool (false);
    if (t == ".inf" || t == "+.inf" || t == ".Inf")
      return jnum (HUGE_VAL);
    if (t == "-.inf" || t == "-.Inf")
      return jnum (-HUGE_VAL);
    if (t == ".nan" || t == ".NaN")
      return jnum (NAN);
    if (is_int (t))
      {
        auto v = metaml::detail::parse_int (t);
        if (v)
          return jnum (static_cast<double> (*v));
      }
    if (is_float (t))
      {
        auto f = metaml::detail::parse_float (t);
        if (f)
          return jnum (*f);
      }
    return jstr (t);
  }
  Value
  flow (const std::string &t, std::size_t &p, std::size_t i)
  {
    auto ws = [&] () {
      while (p < t.size () && t[p] == ' ')
        ++p;
    };
    ws ();
    if (p >= t.size ())
      fail (i, "unterminated flow collection");
    if (t[p] == '[')
      {
        ++p;
        std::vector<Value> xs;
        ws ();
        if (p < t.size () && t[p] == ']')
          {
            ++p;
            return jarr (xs);
          }
        while (true)
          {
            xs.push_back (flow (t, p, i));
            ws ();
            if (p < t.size () && t[p] == ',')
              {
                ++p;
                ws ();
                if (p < t.size () && t[p] == ']')
                  {
                    ++p;
                    return jarr (xs);
                  }
                continue;
              }
            if (p < t.size () && t[p] == ']')
              {
                ++p;
                return jarr (xs);
              }
            fail (i, "expected `,` or `]`");
          }
      }
    if (t[p] == '{')
      {
        ++p;
        std::vector<std::pair<std::string, Value>> kv;
        ws ();
        if (p < t.size () && t[p] == '}')
          {
            ++p;
            return jobj (kv);
          }
        while (true)
          {
            ws ();
            Value k = flow_scalar (t, p, i, true);
            std::string key = metaml::str_of (payload (k));
            if (json_detail::tag (k) != "JStr")
              {
                std::string tmp;
                json_detail::render (k, false, 0, tmp);
                key = tmp;
              }
            ws ();
            Value v = jnull ();
            if (p < t.size () && t[p] == ':')
              {
                ++p;
                ws ();
                if (p < t.size () && t[p] != ',' && t[p] != '}')
                  v = flow (t, p, i);
              }
            kv.push_back ({ key, v });
            ws ();
            if (p < t.size () && t[p] == ',')
              {
                ++p;
                ws ();
                if (p < t.size () && t[p] == '}')
                  {
                    ++p;
                    return jobj (kv);
                  }
                continue;
              }
            if (p < t.size () && t[p] == '}')
              {
                ++p;
                return jobj (kv);
              }
            fail (i, "expected `,` or `}`");
          }
      }
    return flow_scalar (t, p, i, false);
  }
  Value
  flow_scalar (const std::string &t, std::size_t &p, std::size_t i, bool key)
  {
    if (p < t.size () && (t[p] == '"' || t[p] == '\''))
      {
        char q = t[p];
        std::size_t b = p++;
        while (p < t.size ())
          {
            if (q == '"' && t[p] == '\\')
              {
                p += 2;
                continue;
              }
            if (t[p] == q)
              {
                if (q == '\'' && p + 1 < t.size () && t[p + 1] == '\'')
                  {
                    p += 2;
                    continue;
                  }
                break;
              }
            ++p;
          }
        if (p >= t.size ())
          fail (i, "unterminated quoted scalar");
        ++p;
        return quoted (t.substr (b, p - b), i);
      }
    std::size_t b = p;
    while (p < t.size () && t[p] != ',' && t[p] != ']' && t[p] != '}'
           && !(key && t[p] == ':' && (p + 1 == t.size () || t[p + 1] == ' ')))
      ++p;
    return plain (t.substr (b, p - b), i);
  }
  Value
  block_scalar (std::size_t &i, int indent, const std::string &header)
  {
    bool folded = header[0] == '>';
    char chomp = 0;
    for (std::size_t k = 1; k < header.size (); ++k)
      if (header[k] == '-' || header[k] == '+')
        chomp = header[k];
      else if (header[k] != ' ' && !(header[k] >= '1' && header[k] <= '9'))
        fail (i, "invalid block scalar header");
    std::vector<std::string> body;
    std::size_t j = i + 1;
    int bi = -1;
    for (; j < raw_.size (); ++j)
      {
        const std::string &r = raw_[j];
        std::size_t k = 0;
        while (k < r.size () && r[k] == ' ')
          ++k;
        bool blank = trim (r).empty ();
        if (!blank && static_cast<int> (k) <= indent)
          break;
        if (!blank && bi < 0)
          bi = static_cast<int> (k);
        if (blank)
          body.push_back ("");
        else
          {
            if (static_cast<int> (k) < bi)
              break;
            body.push_back (r.substr (static_cast<std::size_t> (bi)));
          }
      }
    std::size_t trailing = 0;
    while (!body.empty () && body.back ().empty ())
      {
        body.pop_back ();
        ++trailing;
      }
    std::string out;
    if (folded)
      {
        for (std::size_t k = 0; k < body.size (); ++k)
          {
            if (k > 0)
              out += body[k].empty () || body[k - 1].empty () ? "\n" : " ";
            out += body[k];
          }
      }
    else
      for (std::size_t k = 0; k < body.size (); ++k)
        out += (k ? "\n" : "") + body[k];
    if (chomp != '-' && !body.empty ())
      out += "\n";
    if (chomp == '+')
      for (std::size_t k = 0; k < trailing; ++k)
        out += "\n";
    i = j;
    if (i > 0)
      i -= 1;
    i = i + 1;
    return jstr (out);
  }
};

inline bool
plain_safe (const std::string &s)
{
  if (s.empty () || s != trim (s))
    return false;
  static const char *specials = "-?:,[]{}#&*!|>'\"%@`";
  if (std::strchr (specials, s[0]))
    return false;
  if (s.find (": ") != std::string::npos || s.find (" #") != std::string::npos
      || s.back () == ':')
    return false;
  for (char c : s)
    if (static_cast<unsigned char> (c) < 0x20)
      return false;
  static const char *words[] = { "~", "null", "Null", "NULL", "true", "True", "TRUE", "false",
                                 "False", "FALSE", ".inf", "-.inf", ".nan" };
  for (const char *w : words)
    if (s == w)
      return false;
  std::size_t k = 0;
  if (s[k] == '-' || s[k] == '+')
    ++k;
  bool numeric = k < s.size ();
  for (; k < s.size (); ++k)
    if (!((s[k] >= '0' && s[k] <= '9') || s[k] == '.' || s[k] == 'e' || s[k] == 'E' || s[k] == '-'
          || s[k] == '+'))
      numeric = false;
  return !(numeric && s.find_first_of ("0123456789") != std::string::npos);
}
inline std::string
scalar_text (const Value &v)
{
  std::string t = tag (v);
  if (t == "JNull")
    return "null";
  if (t == "JBool")
    return payload (v).i ? "true" : "false";
  if (t == "JNum")
    {
      double d = payload (v).f;
      if (std::isnan (d))
        return ".nan";
      if (std::isinf (d))
        return d < 0 ? "-.inf" : ".inf";
      return json_detail::number_text (d);
    }
  std::string s = metaml::str_of (payload (v));
  return plain_safe (s) ? s : json_detail::quote (s);
}
inline bool
is_collection (const Value &v)
{
  std::string t = tag (v);
  if (t == "JArr")
    return !metaml::list_items (payload (v)).empty ();
  if (t == "JObj")
    return !members (v).empty ();
  return false;
}
inline std::string
key_text (const std::string &k)
{
  return plain_safe (k) ? k : json_detail::quote (k);
}
inline void
emit (const Value &v, int indent, std::string &out)
{
  std::string pad (static_cast<std::size_t> (indent), ' ');
  std::string t = tag (v);
  if (t == "JObj")
    {
      for (auto &p : members (v))
        {
          if (is_collection (p.second))
            {
              out += pad + key_text (p.first) + ":\n";
              emit (p.second, indent + 2, out);
            }
          else
            out += pad + key_text (p.first) + ": "
                   + (tag (p.second) == "JArr" ? "[]"
                      : tag (p.second) == "JObj" ? "{}"
                                                 : scalar_text (p.second))
                   + "\n";
        }
      return;
    }
  if (t == "JArr")
    {
      for (auto &x : metaml::list_items (payload (v)))
        {
          if (tag (x) == "JObj" && is_collection (x))
            {
              std::string sub;
              emit (x, indent + 2, sub);
              out += pad + "- " + sub.substr (static_cast<std::size_t> (indent + 2));
            }
          else if (tag (x) == "JArr" && is_collection (x))
            {
              out += pad + "-\n";
              emit (x, indent + 2, out);
            }
          else
            out += pad + "- "
                   + (tag (x) == "JArr" ? "[]" : tag (x) == "JObj" ? "{}" : scalar_text (x))
                   + "\n";
        }
      return;
    }
  out += pad + scalar_text (v) + "\n";
}
} // namespace yaml_detail

inline void
install_YAML (metaml::Session &s)
{
  using namespace support;
  using namespace yaml_detail;
  json_detail::declare_type (s);
  prim (s, "YAML.parse", "String -> Result Json String", 1, [] (C &, V &a) {
    try
      {
        Parser p (S (a[0]));
        return ok (p.document ());
      }
    catch (const YamlFail &f)
      {
        return err (f.msg);
      }
  });
  prim (s, "YAML.render", "Json -> String", 1, [] (C &, V &a) {
    std::string out;
    std::string t = json_detail::tag (a[0]);
    if ((t == "JObj" || t == "JArr") && !is_collection (a[0]))
      out = (t == "JArr" ? "[]\n" : "{}\n");
    else
      emit (a[0], 0, out);
    return str (out);
  });
}

} // namespace metalib
