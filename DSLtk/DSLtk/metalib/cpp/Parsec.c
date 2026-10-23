// metalib/cpp/Parsec.c — implementation of Parsec.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// Parsec.h and must not be compiled on its own. Mirror: metalib/d/Parsec.d.
#pragma once

namespace metalib
{

namespace parsec_detail
{
using metaml::Value;

enum class PK
{
  Str,
  Any,
  Satisfy,
  OneOf,
  Seq,
  Alt,
  Many,
  Many1,
  Map,
  Bind,
  Pure,
  Fail,
  Opt,
  SepBy,
  Left,
  Right,
  Lazy,
  Label,
  Regex,
  Token,
  Spaces,
  Eof
};

struct PObj : metaml::ForeignObj
{
  PK kind;
  std::string text{};
  Value fn{};
  std::shared_ptr<PObj> a{}, b{};
  std::vector<std::shared_ptr<PObj>> alts{};
  dsl::peg_detail::CompiledPattern pat{};
  std::mutex mutex{};
  std::shared_ptr<PObj> forced{};
  explicit PObj (PK k) : metaml::ForeignObj ("Parser"), kind (k) {}
};
using PP = std::shared_ptr<PObj>;

inline Value
wrap (const PP &p)
{
  Value v;
  v.kind = metaml::VK::Foreign;
  v.obj = p;
  return v;
}
inline PP
get (const Value &v, metaml::PrimContext &c)
{
  auto p = std::dynamic_pointer_cast<PObj> (v.obj);
  if (v.kind != metaml::VK::Foreign || !p)
    c.error ("M0404", "expected a Parser");
  return p;
}
inline PP
mk (PK k, PP a = nullptr, PP b = nullptr)
{
  auto p = std::make_shared<PObj> (k);
  p->a = std::move (a);
  p->b = std::move (b);
  return p;
}

inline std::size_t
utf8_len (const std::string &s, std::size_t i)
{
  unsigned char c = static_cast<unsigned char> (s[i]);
  std::size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
  return i + n <= s.size () ? n : 1;
}

struct Run
{
  metaml::PrimContext &c;
  const std::string &s;
  std::size_t far = 0;
  std::vector<std::string> expected{};

  void
  expect (std::size_t pos, const std::string &what)
  {
    if (pos > far)
      {
        far = pos;
        expected.clear ();
      }
    if (pos == far && std::find (expected.begin (), expected.end (), what) == expected.end ())
      expected.push_back (what);
  }

  using R = std::optional<std::pair<Value, std::size_t>>;

  R
  go (const PP &p, std::size_t pos)
  {
    switch (p->kind)
      {
      case PK::Str:
        if (s.compare (pos, p->text.size (), p->text) == 0)
          return std::make_pair (metaml::make_str (p->text), pos + p->text.size ());
        expect (pos, metaml::detail::quote_string (p->text));
        return std::nullopt;
      case PK::Any:
        if (pos < s.size ())
          {
            auto n = utf8_len (s, pos);
            return std::make_pair (metaml::make_str (s.substr (pos, n)), pos + n);
          }
        expect (pos, "any character");
        return std::nullopt;
      case PK::Satisfy:
        if (pos < s.size ())
          {
            auto n = utf8_len (s, pos);
            auto ch = metaml::make_str (s.substr (pos, n));
            if (c.apply (p->fn, { ch }).i)
              return std::make_pair (ch, pos + n);
          }
        expect (pos, p->text);
        return std::nullopt;
      case PK::OneOf:
        if (pos < s.size () && p->text.find (s[pos]) != std::string::npos)
          return std::make_pair (metaml::make_str (s.substr (pos, 1)), pos + 1);
        expect (pos, "one of " + metaml::detail::quote_string (p->text));
        return std::nullopt;
      case PK::Seq:
        {
          auto x = go (p->a, pos);
          if (!x)
            return std::nullopt;
          auto y = go (p->b, x->second);
          if (!y)
            return std::nullopt;
          return std::make_pair (metaml::make_tuple ({ x->first, y->first }), y->second);
        }
      case PK::Left:
      case PK::Right:
        {
          auto x = go (p->a, pos);
          if (!x)
            return std::nullopt;
          auto y = go (p->b, x->second);
          if (!y)
            return std::nullopt;
          return std::make_pair (p->kind == PK::Left ? x->first : y->first, y->second);
        }
      case PK::Alt:
        for (auto &q : p->alts)
          if (auto x = go (q, pos))
            return x;
        return std::nullopt;
      case PK::Many:
      case PK::Many1:
        {
          std::vector<Value> xs;
          std::size_t cur = pos;
          while (auto x = go (p->a, cur))
            {
              xs.push_back (x->first);
              if (x->second == cur)
                break;
              cur = x->second;
            }
          if (p->kind == PK::Many1 && xs.empty ())
            return std::nullopt;
          return std::make_pair (metaml::make_list (xs), cur);
        }
      case PK::SepBy:
        {
          std::vector<Value> xs;
          auto x = go (p->a, pos);
          if (!x)
            return std::make_pair (metaml::make_list (xs), pos);
          xs.push_back (x->first);
          std::size_t cur = x->second;
          while (true)
            {
              auto sep = go (p->b, cur);
              if (!sep)
                break;
              auto y = go (p->a, sep->second);
              if (!y || y->second == cur)
                break;
              xs.push_back (y->first);
              cur = y->second;
            }
          return std::make_pair (metaml::make_list (xs), cur);
        }
      case PK::Map:
        {
          auto x = go (p->a, pos);
          if (!x)
            return std::nullopt;
          return std::make_pair (c.apply (p->fn, { x->first }), x->second);
        }
      case PK::Bind:
        {
          auto x = go (p->a, pos);
          if (!x)
            return std::nullopt;
          auto next = get (c.apply (p->fn, { x->first }), c);
          return go (next, x->second);
        }
      case PK::Pure:
        return std::make_pair (p->fn, pos);
      case PK::Fail:
        expect (pos, p->text);
        return std::nullopt;
      case PK::Opt:
        if (auto x = go (p->a, pos))
          return std::make_pair (metaml::make_option (x->first), x->second);
        return std::make_pair (metaml::make_option (std::nullopt), pos);
      case PK::Lazy:
        {
          PP q;
          {
            std::lock_guard<std::mutex> g (p->mutex);
            q = p->forced;
          }
          if (!q)
            {
              q = get (c.apply (p->fn, { Value::unit () }), c);
              std::lock_guard<std::mutex> g (p->mutex);
              p->forced = q;
            }
          return go (q, pos);
        }
      case PK::Label:
        {
          std::size_t far0 = far;
          auto saved = expected;
          auto x = go (p->a, pos);
          if (!x && far <= pos)
            {
              far = far0;
              expected = saved;
              expect (pos, p->text);
            }
          return x;
        }
      case PK::Regex:
        {
          auto e = p->pat.match_at (s, pos);
          if (e)
            return std::make_pair (metaml::make_str (s.substr (pos, *e - pos)), *e);
          expect (pos, "a match of " + metaml::detail::quote_string (p->text));
          return std::nullopt;
        }
      case PK::Token:
        {
          auto x = go (p->a, pos);
          if (!x)
            return std::nullopt;
          std::size_t cur = x->second;
          while (cur < s.size ()
                 && (s[cur] == ' ' || s[cur] == '\t' || s[cur] == '\n' || s[cur] == '\r'))
            ++cur;
          return std::make_pair (x->first, cur);
        }
      case PK::Spaces:
        {
          std::size_t cur = pos;
          while (cur < s.size ()
                 && (s[cur] == ' ' || s[cur] == '\t' || s[cur] == '\n' || s[cur] == '\r'))
            ++cur;
          return std::make_pair (Value::unit (), cur);
        }
      case PK::Eof:
        if (pos == s.size ())
          return std::make_pair (Value::unit (), pos);
        expect (pos, "end of input");
        return std::nullopt;
      }
    return std::nullopt;
  }

  std::string
  message () const
  {
    std::size_t line = 1, col = 1;
    for (std::size_t k = 0; k < far && k < s.size (); ++k)
      if (s[k] == '\n')
        {
          ++line;
          col = 1;
        }
      else
        ++col;
    std::string exp;
    for (std::size_t i = 0; i < expected.size (); ++i)
      exp += (i == 0 ? "" : i + 1 == expected.size () ? " or " : ", ") + expected[i];
    std::string found = far < s.size ()
                            ? metaml::detail::quote_string (s.substr (far, utf8_len (s, far)))
                            : std::string ("end of input");
    return "Parsec: expected " + (exp.empty () ? std::string ("something else") : exp)
           + " at line " + std::to_string (line) + ", column " + std::to_string (col)
           + ", found " + found;
  }
};
} // namespace parsec_detail

inline void
install_Parsec (metaml::Session &s)
{
  using namespace support;
  using namespace parsec_detail;
  if (!s.checker ().global->types.find ("Parser"))
    s.declare ("type Parser 'a\n", "<metalib Parsec>");
  prim (s, "Parsec.char", "String -> Parser String", 1, [] (C &c, V &a) {
    if (S (a[0]).empty () || utf8_len (S (a[0]), 0) != S (a[0]).size ())
      c.error ("M0404", "Parsec.char expects a one-character string");
    auto p = mk (PK::Str);
    p->text = S (a[0]);
    return wrap (p);
  });
  prim (s, "Parsec.string", "String -> Parser String", 1, [] (C &, V &a) {
    auto p = mk (PK::Str);
    p->text = S (a[0]);
    return wrap (p);
  });
  prim (s, "Parsec.any", "Unit -> Parser String", 1, [] (C &, V &) { return wrap (mk (PK::Any)); });
  prim (s, "Parsec.satisfy", "(String -> Bool) -> Parser String", 1, [] (C &, V &a) {
    auto p = mk (PK::Satisfy);
    p->fn = a[0];
    p->text = "a character satisfying the predicate";
    return wrap (p);
  });
  prim (s, "Parsec.one_of", "String -> Parser String", 1, [] (C &, V &a) {
    auto p = mk (PK::OneOf);
    p->text = S (a[0]);
    return wrap (p);
  });
  prim (s, "Parsec.seq", "Parser 'a -> Parser 'b -> Parser ('a, 'b)", 2,
        [] (C &c, V &a) { return wrap (mk (PK::Seq, get (a[0], c), get (a[1], c))); });
  prim (s, "Parsec.left", "Parser 'a -> Parser 'b -> Parser 'a", 2,
        [] (C &c, V &a) { return wrap (mk (PK::Left, get (a[0], c), get (a[1], c))); });
  prim (s, "Parsec.right", "Parser 'a -> Parser 'b -> Parser 'b", 2,
        [] (C &c, V &a) { return wrap (mk (PK::Right, get (a[0], c), get (a[1], c))); });
  prim (s, "Parsec.between", "Parser 'o -> Parser 'c -> Parser 'a -> Parser 'a", 3, [] (C &c, V &a) {
    return wrap (mk (PK::Left, mk (PK::Right, get (a[0], c), get (a[2], c)), get (a[1], c)));
  });
  prim (s, "Parsec.alt", "Parser 'a -> Parser 'a -> Parser 'a", 2, [] (C &c, V &a) {
    auto p = mk (PK::Alt);
    p->alts = { get (a[0], c), get (a[1], c) };
    return wrap (p);
  });
  prim (s, "Parsec.choice", "[Parser 'a] -> Parser 'a", 1, [] (C &c, V &a) {
    auto p = mk (PK::Alt);
    for (auto &x : items (a[0]))
      p->alts.push_back (get (x, c));
    return wrap (p);
  });
  prim (s, "Parsec.many", "Parser 'a -> Parser ['a]", 1,
        [] (C &c, V &a) { return wrap (mk (PK::Many, get (a[0], c))); });
  prim (s, "Parsec.many1", "Parser 'a -> Parser ['a]", 1,
        [] (C &c, V &a) { return wrap (mk (PK::Many1, get (a[0], c))); });
  prim (s, "Parsec.sep_by", "Parser 'a -> Parser 's -> Parser ['a]", 2,
        [] (C &c, V &a) { return wrap (mk (PK::SepBy, get (a[0], c), get (a[1], c))); });
  prim (s, "Parsec.map", "('a -> 'b) -> Parser 'a -> Parser 'b", 2, [] (C &c, V &a) {
    auto p = mk (PK::Map, get (a[1], c));
    p->fn = a[0];
    return wrap (p);
  });
  prim (s, "Parsec.bind", "Parser 'a -> ('a -> Parser 'b) -> Parser 'b", 2, [] (C &c, V &a) {
    auto p = mk (PK::Bind, get (a[0], c));
    p->fn = a[1];
    return wrap (p);
  });
  prim (s, "Parsec.pure", "'a -> Parser 'a", 1, [] (C &, V &a) {
    auto p = mk (PK::Pure);
    p->fn = a[0];
    return wrap (p);
  });
  prim (s, "Parsec.fail", "String -> Parser 'a", 1, [] (C &, V &a) {
    auto p = mk (PK::Fail);
    p->text = S (a[0]);
    return wrap (p);
  });
  prim (s, "Parsec.optional", "Parser 'a -> Parser (Option 'a)", 1,
        [] (C &c, V &a) { return wrap (mk (PK::Opt, get (a[0], c))); });
  prim (s, "Parsec.lazy", "(Unit -> Parser 'a) -> Parser 'a", 1, [] (C &, V &a) {
    auto p = mk (PK::Lazy);
    p->fn = a[0];
    return wrap (p);
  });
  prim (s, "Parsec.label", "String -> Parser 'a -> Parser 'a", 2, [] (C &c, V &a) {
    auto p = mk (PK::Label, get (a[1], c));
    p->text = S (a[0]);
    return wrap (p);
  });
  prim (s, "Parsec.regex", "String -> Parser String", 1, [] (C &c, V &a) {
    auto p = mk (PK::Regex);
    p->text = S (a[0]);
    p->pat = flex_detail::compile (S (a[0]));
    if (!p->pat.valid)
      c.error ("M0404", "Parsec.regex: invalid pattern: " + p->pat.error);
    return wrap (p);
  });
  prim (s, "Parsec.token", "Parser 'a -> Parser 'a", 1,
        [] (C &c, V &a) { return wrap (mk (PK::Token, get (a[0], c))); });
  prim (s, "Parsec.spaces", "Unit -> Parser Unit", 1,
        [] (C &, V &) { return wrap (mk (PK::Spaces)); });
  prim (s, "Parsec.eof", "Unit -> Parser Unit", 1, [] (C &, V &) { return wrap (mk (PK::Eof)); });
  prim (s, "Parsec.run", "Parser 'a -> String -> Result 'a String", 2, [] (C &c, V &a) {
    Run r{ c, S (a[1]) };
    auto x = r.go (get (a[0], c), 0);
    if (x && x->second == r.s.size ())
      return metaml::make_ok (x->first);
    if (x)
      r.expect (x->second, "end of input");
    return err (r.message ());
  });
  prim (s, "Parsec.run_prefix", "Parser 'a -> String -> Result ('a, String) String", 2,
        [] (C &c, V &a) {
          Run r{ c, S (a[1]) };
          auto x = r.go (get (a[0], c), 0);
          if (x)
            return metaml::make_ok (pair (x->first, str (r.s.substr (x->second))));
          return err (r.message ());
        });
}

} // namespace metalib
