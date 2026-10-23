// metalib/cpp/Frontend.c — implementation of Frontend.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// Frontend.h and must not be compiled on its own. Mirror: metalib/d/Frontend.d.
#pragma once

namespace metalib
{

namespace frontend_detail
{
using metaml::Value;

/// "error[M0201]: message (line 1, column 5)"
inline std::string
diag_text (const dsl::Diagnostic &d)
{
  return d.headline () + " (line " + std::to_string (d.primary_span.line) + ", column "
         + std::to_string (d.primary_span.column) + ")";
}
inline std::string
first_diag (const std::vector<dsl::Diagnostic> &ds)
{
  return ds.empty () ? std::string ("error: failed") : diag_text (ds.front ());
}

inline const char *
tok_name (metaml::Tok k)
{
  using metaml::Tok;
  switch (k)
    {
    case Tok::Eof:
      return "eof";
    case Tok::Int:
      return "int";
    case Tok::Float:
      return "float";
    case Tok::Str:
      return "string";
    case Tok::LIdent:
      return "lident";
    case Tok::UIdent:
      return "uident";
    case Tok::TyVar:
      return "tyvar";
    case Tok::Tacit:
      return "tacit";
    case Tok::Hole:
      return "hole";
    case Tok::Directive:
      return "directive";
    case Tok::Op:
      return "op";
    case Tok::Kw:
      return "keyword";
    case Tok::LParen:
      return "lparen";
    case Tok::RParen:
      return "rparen";
    case Tok::LBracket:
      return "lbracket";
    case Tok::RBracket:
      return "rbracket";
    case Tok::LBrace:
      return "lbrace";
    case Tok::RBrace:
      return "rbrace";
    case Tok::Comma:
      return "comma";
    case Tok::Semi:
      return "semi";
    case Tok::Dot:
      return "dot";
    }
  return "?";
}

/// Resolves a type expression in the session's global scope and returns it
/// generalized; the unifier is left untouched.
inline Value
format_type (metaml::Session &s, const std::string &src)
{
  auto te = metaml::parse_type_expr (src, "<type>");
  if (te.is_err ())
    return support::err (first_diag (te.unwrap_err ()));
  auto &ck = s.checker ();
  auto snap = ck.u.snapshot ();
  try
    {
      metaml::Checker::TVarMap m;
      ++ck.level;
      auto t = ck.resolve_type (te.unwrap (), ck.global, m);
      --ck.level;
      ck.generalize (t, ck.level);
      auto out = metaml::show_type (t);
      ck.u.rollback (snap);
      return support::ok (metaml::make_str (out));
    }
  catch (const metaml::detail::MetaError &e)
    {
      --ck.level;
      ck.u.rollback (snap);
      return support::err (diag_text (e.diag));
    }
}
} // namespace frontend_detail

inline void
install_Frontend (metaml::Session &s)
{
  using namespace support;
  using namespace frontend_detail;
  prim (s, "Frontend.tokens", "String -> Result [(String, String)] String", 1, [] (C &, V &a) {
    auto lx = metaml::lex (S (a[0]), "<frontend>");
    if (!lx.diagnostics.empty ())
      return err (first_diag (lx.diagnostics));
    std::vector<Value> out;
    for (auto &t : lx.tokens)
      if (t.kind != metaml::Tok::Eof)
        out.push_back (pair (str (tok_name (t.kind)), str (t.text)));
    return ok (list (out));
  });
  prim (s, "Frontend.parse", "String -> Result String String", 1, [] (C &, V &a) {
    auto p = metaml::parse (S (a[0]), "<frontend>");
    if (!p.ok ())
      return err (first_diag (p.diagnostics));
    return ok (str (metaml::dump_ast (p.program)));
  });
  prim (s, "Frontend.parse_expr", "String -> Result String String", 1, [] (C &, V &a) {
    auto e = metaml::parse_expression (S (a[0]), "<frontend>");
    if (e.is_err ())
      return err (first_diag (e.unwrap_err ()));
    return ok (str (dsl::ast_to_sexpr (metaml::detail::dump_expr (e.unwrap ())).dump ()));
  });
  prim (s, "Frontend.type_of", "String -> Result String String", 1, [] (C &c, V &a) {
    auto r = c.session.type_of (S (a[0]));
    if (r.is_err ())
      return err (first_diag (r.unwrap_err ()));
    return ok (str (r.unwrap ()));
  });
  prim (s, "Frontend.format_type", "String -> Result String String", 1,
        [] (C &c, V &a) { return format_type (c.session, S (a[0])); });
  prim (s, "Frontend.eval", "String -> Result String String", 1, [] (C &c, V &a) {
    metaml::Session sandbox;
    sandbox.allow_all_primitives ();
    sandbox.set_output ([&c] (std::string_view t) { c.print (t); });
    auto r = sandbox.eval (S (a[0]), "<frontend>");
    if (r.is_err ())
      return err (first_diag (r.unwrap_err ()));
    return ok (str (metaml::show_value (r.unwrap ())));
  });
  prim (s, "Frontend.keywords", "Unit -> [String]", 1, [] (C &, V &) {
    return strings ({ "module", "type", "sig", "struct", "end", "val", "let", "rec", "in", "if",
                      "then", "else", "match", "with", "fn", "fun", "import", "include", "open",
                      "do", "true", "false", "when", "for", "and", "of" });
  });
}

} // namespace metalib
