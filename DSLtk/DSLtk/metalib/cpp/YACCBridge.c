// metalib/cpp/YACCBridge.c — implementation of YACCBridge.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// YACCBridge.h and must not be compiled on its own. Mirror: metalib/d/YACCBridge.d.
#pragma once

namespace metalib
{

namespace yacc_detail
{
using metaml::Value;
using namespace ast_detail;

struct GrammarFail
{
  std::string why;
};

/// A yacc-style grammar compiled onto DSLtk's Earley CFG.
///   rule : alt | alt ... ;      first rule is the start symbol
///   symbols: `name` (a nonterminal if it has rules, else a token kind),
///   "text" or 'text' (a literal token); an empty alternative is epsilon;
///   `#` or `//` start a comment.
struct Grammar
{
  dsl::CFG cfg{};
  std::vector<std::string> nonterminals{};
  std::vector<std::string> literals{};
};

inline Grammar
compile (const std::string &src)
{
  struct GT
  {
    char kind; // i = ident, l = literal, : | ;
    std::string text;
    std::size_t line;
  };
  std::vector<GT> ts;
  std::size_t i = 0, line = 1;
  auto fail = [&] (std::size_t ln, const std::string &why) -> GrammarFail
    { return GrammarFail{ "YACCBridge: grammar line " + std::to_string (ln) + ": " + why }; };
  while (i < src.size ())
    {
      char c = src[i];
      if (c == '\n')
        {
          ++line;
          ++i;
        }
      else if (c == ' ' || c == '\t' || c == '\r')
        ++i;
      else if (c == '#' || (c == '/' && i + 1 < src.size () && src[i + 1] == '/'))
        while (i < src.size () && src[i] != '\n')
          ++i;
      else if (c == ':' || c == '|' || c == ';')
        {
          ts.push_back ({ c, std::string (1, c), line });
          ++i;
        }
      else if (c == '"' || c == '\'')
        {
          std::size_t j = i + 1;
          std::string text;
          while (j < src.size () && src[j] != c && src[j] != '\n')
            {
              if (src[j] == '\\' && j + 1 < src.size ())
                ++j;
              text += src[j++];
            }
          if (j >= src.size () || src[j] != c)
            throw fail (line, "unterminated literal");
          if (text.empty ())
            throw fail (line, "empty literal");
          ts.push_back ({ 'l', text, line });
          i = j + 1;
        }
      else if (std::isalpha (static_cast<unsigned char> (c)) || c == '_')
        {
          std::size_t j = i;
          while (j < src.size ()
                 && (std::isalnum (static_cast<unsigned char> (src[j])) || src[j] == '_'))
            ++j;
          ts.push_back ({ 'i', src.substr (i, j - i), line });
          i = j;
        }
      else
        throw fail (line, "unexpected character `" + std::string (1, c) + "`");
    }
  struct Raw
  {
    std::string lhs;
    std::vector<std::vector<GT>> alts;
  };
  std::vector<Raw> raws;
  std::size_t k = 0;
  while (k < ts.size ())
    {
      if (ts[k].kind != 'i')
        throw fail (ts[k].line, "expected a rule name, found `" + ts[k].text + "`");
      if (k + 1 >= ts.size () || ts[k + 1].kind != ':')
        throw fail (ts[k].line, "expected `:` after `" + ts[k].text + "`");
      Raw r{ ts[k].text, { {} } };
      k += 2;
      while (true)
        {
          if (k >= ts.size ())
            throw fail (ts.back ().line, "rule `" + r.lhs + "` is missing its final `;`");
          if (ts[k].kind == ';')
            {
              ++k;
              break;
            }
          if (ts[k].kind == '|')
            r.alts.emplace_back ();
          else if (ts[k].kind == ':')
            throw fail (ts[k].line, "unexpected `:` (missing `;` before this rule?)");
          else
            r.alts.back ().push_back (ts[k]);
          ++k;
        }
      raws.push_back (std::move (r));
    }
  if (raws.empty ())
    throw GrammarFail{ "YACCBridge: the grammar has no rules" };
  Grammar g;
  std::set<std::string> nts;
  for (auto &r : raws)
    if (nts.insert (r.lhs).second)
      g.nonterminals.push_back (r.lhs);
  g.cfg.start = raws[0].lhs;
  for (auto &r : raws)
    for (auto &alt : r.alts)
      {
        std::vector<dsl::CFGSymbol> rhs;
        for (auto &t : alt)
          {
            if (t.kind == 'l')
              {
                rhs.push_back (dsl::CFGSymbol::token ("'" + t.text));
                if (std::find (g.literals.begin (), g.literals.end (), t.text) == g.literals.end ())
                  g.literals.push_back (t.text);
              }
            else if (nts.count (t.text))
              rhs.push_back (dsl::CFGSymbol::nonterminal (t.text));
            else
              rhs.push_back (dsl::CFGSymbol::token ("%" + t.text));
          }
        g.cfg.add (r.lhs, std::move (rhs));
      }
  return g;
}

struct Tok
{
  std::string kind, text;
};

inline bool
is_word (const std::string &s)
{
  for (char c : s)
    if (!(std::isalnum (static_cast<unsigned char> (c)) || c == '_'))
      return false;
  return true;
}

/// Default lexer: literals of the grammar, NUM, ID, STR; whitespace skipped.
inline std::vector<Tok>
lex (const Grammar &g, const std::string &s)
{
  std::vector<Tok> out;
  std::size_t i = 0;
  while (i < s.size ())
    {
      char c = s[i];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
        {
          ++i;
          continue;
        }
      std::size_t cls = 0;
      std::string kind;
      if (std::isdigit (static_cast<unsigned char> (c)))
        {
          std::size_t j = i;
          while (j < s.size () && std::isdigit (static_cast<unsigned char> (s[j])))
            ++j;
          if (j + 1 < s.size () && s[j] == '.' && std::isdigit (static_cast<unsigned char> (s[j + 1])))
            {
              ++j;
              while (j < s.size () && std::isdigit (static_cast<unsigned char> (s[j])))
                ++j;
            }
          cls = j - i;
          kind = "NUM";
        }
      else if (std::isalpha (static_cast<unsigned char> (c)) || c == '_')
        {
          std::size_t j = i;
          while (j < s.size () && (std::isalnum (static_cast<unsigned char> (s[j])) || s[j] == '_'))
            ++j;
          cls = j - i;
          kind = "ID";
        }
      else if (c == '"')
        {
          std::size_t j = i + 1;
          while (j < s.size () && s[j] != '"')
            j += (s[j] == '\\' && j + 1 < s.size ()) ? 2 : 1;
          if (j >= s.size ())
            throw GrammarFail{ "YACCBridge: unterminated string at offset " + std::to_string (i) };
          cls = j + 1 - i;
          kind = "STR";
        }
      std::size_t lit = 0;
      for (auto &l : g.literals)
        if (l.size () > lit && s.compare (i, l.size (), l) == 0)
          {
            if (is_word (l) && i + l.size () < s.size ()
                && (std::isalnum (static_cast<unsigned char> (s[i + l.size ()])) || s[i + l.size ()] == '_'))
              continue;
            lit = l.size ();
          }
      if (lit && lit >= cls)
        {
          out.push_back ({ "LIT", s.substr (i, lit) });
          i += lit;
        }
      else if (cls)
        {
          out.push_back ({ kind, s.substr (i, cls) });
          i += cls;
        }
      else
        throw GrammarFail{ "YACCBridge: unexpected character "
                           + metaml::detail::quote_string (s.substr (i, 1)) + " at offset "
                           + std::to_string (i) };
    }
  return out;
}

inline std::string
show_key (const std::string &k)
{
  if (k == "<end-of-input>")
    return "end of input";
  if (k[0] == '\'')
    return metaml::detail::quote_string (k.substr (1));
  return k.substr (1);
}

inline Value
convert (const dsl::ASTNode &n, const std::vector<Tok> &toks, std::size_t &next)
{
  if (n.tag () == "token" && n.is_leaf ())
    {
      const Tok &t = toks[next++];
      return leaf (t.kind, t.text);
    }
  std::vector<Value> ks;
  for (std::size_t i = 0; i < n.size (); ++i)
    ks.push_back (convert (n.child (i), toks, next));
  return node (std::string (n.tag ()), ks);
}

inline Value
run (const Grammar &g, const std::vector<Tok> &toks)
{
  std::vector<std::string> keys;
  for (auto &t : toks)
    keys.push_back (std::find (g.literals.begin (), g.literals.end (), t.text) != g.literals.end ()
                        ? "'" + t.text
                        : "%" + t.kind);
  auto r = g.cfg.parse (keys);
  if (!r.ok ())
    {
      std::string where = r.position < toks.size ()
                              ? "unexpected " + metaml::detail::quote_string (toks[r.position].text)
                                    + " at token " + std::to_string (r.position + 1)
                              : std::string ("unexpected end of input");
      std::string exp;
      for (auto &e : r.expected)
        exp += (exp.empty () ? "" : ", ") + show_key (e);
      return support::err ("YACCBridge: " + where + (exp.empty () ? "" : "; expected " + exp));
    }
  std::size_t next = 0;
  return support::ok (convert (*r.tree, toks, next));
}
} // namespace yacc_detail

inline void
install_YACCBridge (metaml::Session &s)
{
  using namespace support;
  using namespace yacc_detail;
  declare_ast_type (s);
  prim (s, "YACCBridge.check", "String -> Result [String] String", 1, [] (C &, V &a) {
    try
      {
        return ok (strings (compile (S (a[0])).nonterminals));
      }
    catch (const GrammarFail &f)
      {
        return err (f.why);
      }
  });
  prim (s, "YACCBridge.literals", "String -> Result [String] String", 1, [] (C &, V &a) {
    try
      {
        return ok (strings (compile (S (a[0])).literals));
      }
    catch (const GrammarFail &f)
      {
        return err (f.why);
      }
  });
  prim (s, "YACCBridge.parse", "String -> String -> Result Ast String", 2, [] (C &, V &a) {
    try
      {
        auto g = compile (S (a[0]));
        return run (g, lex (g, S (a[1])));
      }
    catch (const GrammarFail &f)
      {
        return err (f.why);
      }
  });
  prim (s, "YACCBridge.parse_tokens", "String -> [(String, String)] -> Result Ast String", 2,
        [] (C &, V &a) {
          try
            {
              auto g = compile (S (a[0]));
              std::vector<Tok> toks;
              for (auto &t : items (a[1]))
                toks.push_back ({ S (fst (t)), S (snd (t)) });
              return run (g, toks);
            }
          catch (const GrammarFail &f)
            {
              return err (f.why);
            }
        });
}

} // namespace metalib
