/**
 * metalib/d/ASTBuilder.d — MetaML host module `ASTBuilder` (D dialect): generic syntax trees (`Ast`) bridged to DSLtk `ASTNode`.
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/ASTBuilder.h):
 *   type Ast := | Leaf String String | Node String [Ast]
 *   ASTBuilder.leaf : String -> String -> Ast
 *   ASTBuilder.node : String -> [Ast] -> Ast
 *   ASTBuilder.dump : Ast -> String
 *   ASTBuilder.pretty : Ast -> String
 *   ASTBuilder.tag : Ast -> String
 *   ASTBuilder.text : Ast -> Option String
 *   ASTBuilder.children : Ast -> [Ast]
 *   ASTBuilder.is_leaf : Ast -> Bool
 *   ASTBuilder.size : Ast -> Int
 *   ASTBuilder.depth : Ast -> Int
 *   ASTBuilder.find : String -> Ast -> [Ast]
 *   ASTBuilder.with_children : [Ast] -> Ast -> Ast
 *   ASTBuilder.to_sexpr : Ast -> SExpr
 *   ASTBuilder.of_sexpr : SExpr -> Result Ast String
 *   ASTBuilder.parse : String -> Result Ast String
 */
module metalib.d.ASTBuilder;

import MetaML;
import metalib.d.Support;

import DSLtk : ASTNode, DSExpr = SExpr, parse_sexpr;
import std.algorithm.comparison : max;

/// Declares `type Ast := | Leaf String String | Node String [Ast]` once.
void declare_ast_type(Session s)
{
    if (s.checker().global.types.find("Ast") !is null) return;
    s.declare("type Ast := | Leaf String String | Node String [Ast]\n", "<metalib ASTBuilder>");
}

Value leaf(string tag, string text) { return make_ctor("Leaf", 0, [make_str(tag), make_str(text)]); }
Value node(string tag, Value[] ks) { return make_ctor("Node", 1, [make_str(tag), make_list(ks)]); }
bool is_leaf(Value v) { return agg_of(v).tag == "Leaf"; }
string tag_of(Value v) { return str_of(agg_of(v).items[0]); }
Value[] kids(Value v) { return is_leaf(v) ? null : list_items(agg_of(v).items[1]); }
string text_of(Value v) { return str_of(agg_of(v).items[1]); }

/// MetaML `Ast` <-> DSLtk `ASTNode`.
ASTNode to_node(Value v)
{
    if (is_leaf(v)) return ASTNode(tag_of(v), text_of(v));
    ASTNode[] xs;
    foreach (k; kids(v)) xs ~= to_node(k);
    return ASTNode(tag_of(v), xs);
}
Value of_node(ref const ASTNode n)
{
    if (n.is_leaf()) return leaf(n.tag(), n.value());
    Value[] xs;
    foreach (i; 0 .. n.size()) xs ~= of_node(n.child(i));
    return node(n.tag(), xs);
}

private long size_of(Value v)
{
    long n = 1;
    foreach (k; kids(v)) n += size_of(k);
    return n;
}
private long depth_of(Value v)
{
    long d = 0;
    foreach (k; kids(v)) d = max(d, depth_of(k));
    return d + 1;
}
private void pretty(Value v, int indent, ref string result)
{
    import std.array : replicate;
    result ~= replicate(" ", cast(size_t) indent);
    if (is_leaf(v))
    {
        result ~= tag_of(v) ~ " " ~ quote_string(text_of(v)) ~ "\n";
        return;
    }
    result ~= tag_of(v) ~ "\n";
    foreach (k; kids(v)) pretty(k, indent + 2, result);
}
private Value sexpr_atom(string s) { return make_ctor("Atom", 0, [make_str(s)]); }
private Value sexpr_list(Value[] xs) { return make_ctor("SList", 1, [make_list(xs)]); }
private Value to_sexpr(Value v)
{
    if (is_leaf(v)) return sexpr_list([sexpr_atom(tag_of(v)), sexpr_atom(text_of(v))]);
    Value[] xs = [sexpr_atom(tag_of(v))];
    foreach (k; kids(v)) xs ~= to_sexpr(k);
    return sexpr_list(xs);
}
private class OfSexprFail : Exception
{
    this(string m) { super(m); }
}
private Value of_sexpr(Value e)
{
    auto a = agg_of(e);
    if (a.tag == "Atom")
        throw new OfSexprFail("ASTBuilder.of_sexpr: expected a list, found atom `" ~ str_of(a.items[0]) ~ "`");
    auto xs = list_items(a.items[0]);
    if (!xs.length || agg_of(xs[0]).tag != "Atom")
        throw new OfSexprFail("ASTBuilder.of_sexpr: a node must start with an atom tag");
    string tag = str_of(agg_of(xs[0]).items[0]);
    if (xs.length == 2 && agg_of(xs[1]).tag == "Atom") return leaf(tag, str_of(agg_of(xs[1]).items[0]));
    Value[] ks;
    foreach (x; xs[1 .. $]) ks ~= of_sexpr(x);
    return node(tag, ks);
}
private void find_tag(Value v, string tag, ref Value[] result)
{
    if (tag_of(v) == tag) result ~= v;
    foreach (k; kids(v)) find_tag(k, tag, result);
}
private Value conv_sexpr(ref const DSExpr x)
{
    if (x.is_atom) return sexpr_atom(x.text);
    Value[] xs;
    foreach (ref i; x.items) xs ~= conv_sexpr(i);
    return sexpr_list(xs);
}

void installASTBuilder(Session s)
{
    declare_ast_type(s);
    if (s.checker().global.types.find("SExpr") is null)
        s.declare("type SExpr := | Atom String | SList [SExpr]\n", "<metalib SExpr>");
    prim(s, "ASTBuilder.leaf", "String -> String -> Ast", 2, delegate Value(C c, V a) => leaf(S(a[0]), S(a[1])));
    prim(s, "ASTBuilder.node", "String -> [Ast] -> Ast", 2, delegate Value(C c, V a) => node(S(a[0]), items(a[1])));
    prim(s, "ASTBuilder.dump", "Ast -> String", 1, delegate Value(C c, V a) => str(to_node(a[0]).dump()));
    prim(s, "ASTBuilder.pretty", "Ast -> String", 1, delegate Value(C c, V a) {
        string result;
        pretty(a[0], 0, result);
        return str(result);
    });
    prim(s, "ASTBuilder.tag", "Ast -> String", 1, delegate Value(C c, V a) => str(tag_of(a[0])));
    prim(s, "ASTBuilder.text", "Ast -> Option String", 1,
        delegate Value(C c, V a) => is_leaf(a[0]) ? some(str(text_of(a[0]))) : none());
    prim(s, "ASTBuilder.children", "Ast -> [Ast]", 1, delegate Value(C c, V a) => list(kids(a[0])));
    prim(s, "ASTBuilder.is_leaf", "Ast -> Bool", 1, delegate Value(C c, V a) => boolean(is_leaf(a[0])));
    prim(s, "ASTBuilder.size", "Ast -> Int", 1, delegate Value(C c, V a) => integer(size_of(a[0])));
    prim(s, "ASTBuilder.depth", "Ast -> Int", 1, delegate Value(C c, V a) => integer(depth_of(a[0])));
    prim(s, "ASTBuilder.find", "String -> Ast -> [Ast]", 2, delegate Value(C c, V a) {
        Value[] result;
        find_tag(a[1], S(a[0]), result);
        return list(result);
    });
    prim(s, "ASTBuilder.with_children", "[Ast] -> Ast -> Ast", 2,
        delegate Value(C c, V a) => node(tag_of(a[1]), items(a[0])));
    prim(s, "ASTBuilder.to_sexpr", "Ast -> SExpr", 1, delegate Value(C c, V a) => to_sexpr(a[0]));
    prim(s, "ASTBuilder.of_sexpr", "SExpr -> Result Ast String", 1, delegate Value(C c, V a) {
        try return ok(of_sexpr(a[0]));
        catch (OfSexprFail f) return err(f.msg);
    });
    prim(s, "ASTBuilder.parse", "String -> Result Ast String", 1, delegate Value(C c, V a) {
        try
        {
            auto e = parse_sexpr(S(a[0]));
            return ok(of_sexpr(conv_sexpr(e)));
        }
        catch (OfSexprFail f) return err(f.msg);
        catch (Exception e) return err(e.msg);
    });
}

