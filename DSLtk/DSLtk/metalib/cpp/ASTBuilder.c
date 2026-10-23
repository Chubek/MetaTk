// metalib/cpp/ASTBuilder.c — implementation of ASTBuilder.h.
// This is C++ (the metalib host modules are header-only); it is #included by
// ASTBuilder.h and must not be compiled on its own. Mirror: metalib/d/ASTBuilder.d.
#pragma once

namespace metalib
{

namespace ast_detail
{
using metaml::Value;

/// Declares `type Ast := | Leaf String String | Node String [Ast]` once.
inline void
declare_ast_type (metaml::Session &s)
{
  if (s.checker ().global->types.find ("Ast"))
    return;
  s.declare ("type Ast := | Leaf String String | Node String [Ast]\n", "<metalib ASTBuilder>");
}

inline Value
leaf (std::string tag, std::string text)
{
  return metaml::make_ctor ("Leaf", 0, { metaml::make_str (std::move (tag)),
                                         metaml::make_str (std::move (text)) });
}
inline Value
node (std::string tag, const std::vector<Value> &kids)
{
  return metaml::make_ctor ("Node", 1, { metaml::make_str (std::move (tag)),
                                         metaml::make_list (kids) });
}
inline bool
is_leaf (const Value &v)
{
  return metaml::agg_of (v)->tag == "Leaf";
}
inline const std::string &
tag_of (const Value &v)
{
  return metaml::str_of (metaml::agg_of (v)->items[0]);
}
inline std::vector<Value>
kids (const Value &v)
{
  if (is_leaf (v))
    return {};
  return metaml::list_items (metaml::agg_of (v)->items[1]);
}
inline const std::string &
text_of (const Value &v)
{
  return metaml::str_of (metaml::agg_of (v)->items[1]);
}

/// MetaML `Ast` <-> DSLtk `ASTNode`.
inline dsl::ASTNode
to_node (const Value &v)
{
  if (is_leaf (v))
    return dsl::ASTNode (tag_of (v), text_of (v));
  std::vector<dsl::ASTNode> xs;
  for (auto &k : kids (v))
    xs.push_back (to_node (k));
  return dsl::ASTNode (tag_of (v), std::move (xs));
}
inline Value
of_node (const dsl::ASTNode &n)
{
  if (n.is_leaf ())
    return leaf (std::string (n.tag ()), std::string (n.value ()));
  std::vector<Value> xs;
  for (std::size_t i = 0; i < n.size (); ++i)
    xs.push_back (of_node (n.child (i)));
  return node (std::string (n.tag ()), xs);
}

inline std::int64_t
size_of (const Value &v)
{
  std::int64_t n = 1;
  for (auto &k : kids (v))
    n += size_of (k);
  return n;
}
inline std::int64_t
depth_of (const Value &v)
{
  std::int64_t d = 0;
  for (auto &k : kids (v))
    d = std::max (d, depth_of (k));
  return d + 1;
}

inline void
pretty (const Value &v, int indent, std::string &out)
{
  out += std::string (static_cast<std::size_t> (indent), ' ');
  if (is_leaf (v))
    {
      out += tag_of (v) + " " + metaml::detail::quote_string (text_of (v)) + "\n";
      return;
    }
  out += tag_of (v) + "\n";
  for (auto &k : kids (v))
    pretty (k, indent + 2, out);
}

inline Value
sexpr_atom (std::string s)
{
  return metaml::make_ctor ("Atom", 0, { metaml::make_str (std::move (s)) });
}
inline Value
sexpr_list (const std::vector<Value> &xs)
{
  return metaml::make_ctor ("SList", 1, { metaml::make_list (xs) });
}
inline Value
to_sexpr (const Value &v)
{
  if (is_leaf (v))
    return sexpr_list ({ sexpr_atom (tag_of (v)), sexpr_atom (text_of (v)) });
  std::vector<Value> xs{ sexpr_atom (tag_of (v)) };
  for (auto &k : kids (v))
    xs.push_back (to_sexpr (k));
  return sexpr_list (xs);
}

struct OfSexprFail
{
  std::string why;
};
inline Value
of_sexpr (const Value &e)
{
  auto a = metaml::agg_of (e);
  if (a->tag == "Atom")
    throw OfSexprFail{ "ASTBuilder.of_sexpr: expected a list, found atom `"
                       + metaml::str_of (a->items[0]) + "`" };
  auto xs = metaml::list_items (a->items[0]);
  if (xs.empty () || metaml::agg_of (xs[0])->tag != "Atom")
    throw OfSexprFail{ "ASTBuilder.of_sexpr: a node must start with an atom tag" };
  std::string tag = metaml::str_of (metaml::agg_of (xs[0])->items[0]);
  if (xs.size () == 2 && metaml::agg_of (xs[1])->tag == "Atom")
    return leaf (tag, metaml::str_of (metaml::agg_of (xs[1])->items[0]));
  std::vector<Value> ks;
  for (std::size_t i = 1; i < xs.size (); ++i)
    ks.push_back (of_sexpr (xs[i]));
  return node (tag, ks);
}

inline void
find_tag (const Value &v, const std::string &tag, std::vector<Value> &out)
{
  if (tag_of (v) == tag)
    out.push_back (v);
  for (auto &k : kids (v))
    find_tag (k, tag, out);
}
} // namespace ast_detail

inline void
install_ASTBuilder (metaml::Session &s)
{
  using namespace support;
  using namespace ast_detail;
  declare_ast_type (s);
  if (!s.checker ().global->types.find ("SExpr"))
    s.declare ("type SExpr := | Atom String | SList [SExpr]\n", "<metalib SExpr>");
  prim (s, "ASTBuilder.leaf", "String -> String -> Ast", 2,
        [] (C &, V &a) { return leaf (S (a[0]), S (a[1])); });
  prim (s, "ASTBuilder.node", "String -> [Ast] -> Ast", 2,
        [] (C &, V &a) { return node (S (a[0]), items (a[1])); });
  prim (s, "ASTBuilder.dump", "Ast -> String", 1,
        [] (C &, V &a) { return str (to_node (a[0]).dump ()); });
  prim (s, "ASTBuilder.pretty", "Ast -> String", 1, [] (C &, V &a) {
    std::string out;
    pretty (a[0], 0, out);
    return str (out);
  });
  prim (s, "ASTBuilder.tag", "Ast -> String", 1, [] (C &, V &a) { return str (tag_of (a[0])); });
  prim (s, "ASTBuilder.text", "Ast -> Option String", 1, [] (C &, V &a) {
    return is_leaf (a[0]) ? some (str (text_of (a[0]))) : none ();
  });
  prim (s, "ASTBuilder.children", "Ast -> [Ast]", 1,
        [] (C &, V &a) { return list (kids (a[0])); });
  prim (s, "ASTBuilder.is_leaf", "Ast -> Bool", 1,
        [] (C &, V &a) { return boolean (is_leaf (a[0])); });
  prim (s, "ASTBuilder.size", "Ast -> Int", 1, [] (C &, V &a) { return integer (size_of (a[0])); });
  prim (s, "ASTBuilder.depth", "Ast -> Int", 1,
        [] (C &, V &a) { return integer (depth_of (a[0])); });
  prim (s, "ASTBuilder.find", "String -> Ast -> [Ast]", 2, [] (C &, V &a) {
    std::vector<Value> out;
    find_tag (a[1], S (a[0]), out);
    return list (out);
  });
  prim (s, "ASTBuilder.with_children", "[Ast] -> Ast -> Ast", 2, [] (C &, V &a) {
    return node (tag_of (a[1]), items (a[0]));
  });
  prim (s, "ASTBuilder.to_sexpr", "Ast -> SExpr", 1, [] (C &, V &a) { return to_sexpr (a[0]); });
  prim (s, "ASTBuilder.of_sexpr", "SExpr -> Result Ast String", 1, [] (C &, V &a) {
    try
      {
        return ok (of_sexpr (a[0]));
      }
    catch (const OfSexprFail &f)
      {
        return err (f.why);
      }
  });
  prim (s, "ASTBuilder.parse", "String -> Result Ast String", 1, [] (C &, V &a) {
    try
      {
        auto e = dsl::parse_sexpr (S (a[0]));
        std::function<Value (const dsl::SExpr &)> conv = [&] (const dsl::SExpr &x) -> Value
          {
            if (x.is_atom)
              return sexpr_atom (x.text);
            std::vector<Value> xs;
            for (auto &i : x.items)
              xs.push_back (conv (i));
            return sexpr_list (xs);
          };
        return ok (of_sexpr (conv (e)));
      }
    catch (const OfSexprFail &f)
      {
        return err (f.why);
      }
    catch (const std::exception &e)
      {
        return err (e.what ());
      }
  });
}

} // namespace metalib
