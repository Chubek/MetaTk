// metalib/cpp/FlexBridge.c — implementation of FlexBridge.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// FlexBridge.h and must not be compiled on its own. Mirror: metalib/d/FlexBridge.d.
#pragma once

namespace metalib
{

namespace flex_detail
{
using metaml::Value;
using Pattern = dsl::peg_detail::CompiledPattern;

/// Compiles the FlexBridge pattern syntax onto DSLtk's CompiledPattern:
/// the DSLtk subset (literals, `.`, `[...]`, `[^...]`, `*`, `+`, `?`, `!`)
/// plus backslash escapes `\n \t \r \d \w \s` and `\X` for a literal X.
inline Pattern
compile (const std::string &pat)
{
  Pattern out;
  auto bad = [&] (std::string why)
    {
      out.valid = false;
      out.error = std::move (why);
      out.atoms.clear ();
      return out;
    };
  auto escape_class = [] (char e, std::string &cls) -> bool
    {
      switch (e)
        {
        case 'd':
          cls += "0-9";
          return true;
        case 'w':
          cls += "a-zA-Z0-9_";
          return true;
        case 's':
          cls += " \t\n\r";
          return true;
        default:
          return false;
        }
    };
  auto escape_char = [] (char e) -> char
    {
      switch (e)
        {
        case 'n':
          return '\n';
        case 't':
          return '\t';
        case 'r':
          return '\r';
        default:
          return e;
        }
    };
  std::size_t i = 0;
  while (i < pat.size ())
    {
      Pattern::Atom a;
      char c = pat[i];
      if (c == '*' || c == '+' || c == '?' || c == '!')
        return bad ("quantifier `" + std::string (1, c) + "` has nothing to repeat");
      if (c == '.')
        {
          a.any = true;
          ++i;
        }
      else if (c == '\\')
        {
          if (i + 1 >= pat.size ())
            return bad ("trailing backslash");
          char e = pat[i + 1];
          std::string cls;
          if (escape_class (e, cls))
            {
              a.is_class = true;
              a.cls = cls;
            }
          else
            a.ch = escape_char (e);
          i += 2;
        }
      else if (c == '[')
        {
          ++i;
          a.is_class = true;
          if (i < pat.size () && pat[i] == '^')
            {
              a.negate = true;
              ++i;
            }
          bool closed = false;
          std::string dash;
          while (i < pat.size ())
            {
              char d = pat[i];
              if (d == ']' && !(a.cls.empty () && dash.empty ()))
                {
                  closed = true;
                  ++i;
                  break;
                }
              if (d == '\\' && i + 1 < pat.size ())
                {
                  char e = pat[i + 1];
                  if (!escape_class (e, a.cls))
                    {
                      char lit = escape_char (e);
                      if (lit == '-')
                        dash = "-";
                      else
                        a.cls += lit;
                    }
                  i += 2;
                  continue;
                }
              if (d == '-' && (a.cls.empty () || i + 1 >= pat.size () || pat[i + 1] == ']'))
                {
                  dash = "-";
                  ++i;
                  continue;
                }
              a.cls += d;
              ++i;
            }
          if (!closed)
            return bad ("unterminated character class");
          a.cls += dash;
        }
      else
        {
          a.ch = c;
          ++i;
        }
      if (i < pat.size () && (pat[i] == '*' || pat[i] == '+' || pat[i] == '?' || pat[i] == '!'))
        a.quant = pat[i++];
      out.atoms.push_back (std::move (a));
    }
  if (out.atoms.empty ())
    return bad ("empty pattern");
  return out;
}

struct Rule
{
  std::string name;
  Pattern pat;
};
struct Tok
{
  std::string name, text;
  std::size_t offset;
};

inline std::string
lex_rules (const std::vector<Value> &rs, std::vector<Rule> &out)
{
  for (auto &r : rs)
    {
      Rule x{ metaml::str_of (support::fst (r)), compile (metaml::str_of (support::snd (r))) };
      if (!x.pat.valid)
        return "FlexBridge: invalid pattern for rule `" + x.name + "`: " + x.pat.error;
      out.push_back (std::move (x));
    }
  return "";
}

inline std::string
tokenize (const std::vector<Rule> &rules, const std::string &text, std::vector<Tok> &out)
{
  std::size_t pos = 0;
  while (pos < text.size ())
    {
      std::size_t best = 0;
      const Rule *win = nullptr;
      for (auto &r : rules)
        {
          auto e = r.pat.match_at (text, pos);
          if (e && *e > pos && *e - pos > best)
            {
              best = *e - pos;
              win = &r;
            }
        }
      if (!win)
        {
          std::size_t line = 1, col = 1;
          for (std::size_t k = 0; k < pos; ++k)
            if (text[k] == '\n')
              {
                ++line;
                col = 1;
              }
            else
              ++col;
          return "FlexBridge: no rule matches " + metaml::detail::quote_string (text.substr (pos, 1))
                 + " at line " + std::to_string (line) + ", column " + std::to_string (col);
        }
      if (win->name.empty () || win->name[0] != '_')
        out.push_back ({ win->name, text.substr (pos, best), pos });
      pos += best;
    }
  return "";
}
} // namespace flex_detail

inline void
install_FlexBridge (metaml::Session &s)
{
  using namespace support;
  using namespace flex_detail;
  prim (s, "FlexBridge.tokenize", "[(String, String)] -> String -> Result [(String, String)] String", 2,
        [] (C &, V &a) {
          std::vector<Rule> rules;
          auto e = lex_rules (items (a[0]), rules);
          if (!e.empty ())
            return err (e);
          std::vector<Tok> toks;
          e = tokenize (rules, S (a[1]), toks);
          if (!e.empty ())
            return err (e);
          std::vector<Value> out;
          for (auto &t : toks)
            out.push_back (pair (str (t.name), str (t.text)));
          return ok (list (out));
        });
  prim (s, "FlexBridge.tokenize_spans",
        "[(String, String)] -> String -> Result [(String, String, Int)] String", 2, [] (C &, V &a) {
          std::vector<Rule> rules;
          auto e = lex_rules (items (a[0]), rules);
          if (!e.empty ())
            return err (e);
          std::vector<Tok> toks;
          e = tokenize (rules, S (a[1]), toks);
          if (!e.empty ())
            return err (e);
          std::vector<Value> out;
          for (auto &t : toks)
            out.push_back (triple (str (t.name), str (t.text),
                                   integer (static_cast<std::int64_t> (t.offset))));
          return ok (list (out));
        });
  prim (s, "FlexBridge.valid_pattern", "String -> Result Unit String", 1, [] (C &, V &a) {
    auto p = compile (S (a[0]));
    if (!p.valid)
      return err (p.error);
    return ok (unit ());
  });
  prim (s, "FlexBridge.match_prefix", "String -> String -> Option String", 2, [] (C &, V &a) {
    auto p = compile (S (a[0]));
    if (!p.valid)
      return none ();
    auto e = p.match_at (S (a[1]), 0);
    if (!e)
      return none ();
    return some (str (S (a[1]).substr (0, *e)));
  });
  prim (s, "FlexBridge.matches", "String -> String -> Bool", 2, [] (C &, V &a) {
    auto p = compile (S (a[0]));
    if (!p.valid)
      return boolean (false);
    auto e = p.match_at (S (a[1]), 0);
    return boolean (e && *e == S (a[1]).size ());
  });
}

} // namespace metalib
