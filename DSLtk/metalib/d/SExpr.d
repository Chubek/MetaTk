/**
 * metalib/d/SExpr.d — MetaML host module `SExpr` (D dialect): S-expressions (DSLtk reader and printer).
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/SExpr.h):
 *   type SExpr := | Atom String | SList [SExpr]
 *   SExpr.parse, parse_one, render, atom_text
 */
module metalib.d.SExpr;

import MetaML;
import metalib.d.Support;

import DSLtk : DSExpr = SExpr, parse_sexpressions, parse_sexpr;

private Value to_value(DSExpr e)
{
    if (e.is_atom) return make_ctor("Atom", 0, [make_str(e.text)]);
    Value[] xs;
    foreach (i; e.items) xs ~= to_value(i);
    return make_ctor("SList", 1, [make_list(xs)]);
}

private DSExpr from_value(Value v)
{
    auto a = agg_of(v);
    if (a.tag == "Atom") return DSExpr.atom(str_of(a.items[0]));
    DSExpr[] xs;
    foreach (x; list_items(a.items[0])) xs ~= from_value(x);
    return DSExpr.list(xs);
}

void installSExpr(Session s)
{
    if (s.checker().global.types.find("SExpr") is null)
        s.declare("type SExpr := | Atom String | SList [SExpr]\n", "<metalib SExpr>");
    prim(s, "SExpr.parse", "String -> Result [SExpr] String", 1, delegate Value(C c, V a) {
        try
        {
            Value[] result;
            foreach (e; parse_sexpressions(S(a[0]))) result ~= to_value(e);
            return ok(list(result));
        }
        catch (Exception e) return err(e.msg);
    });
    prim(s, "SExpr.parse_one", "String -> Result SExpr String", 1, delegate Value(C c, V a) {
        try return ok(to_value(parse_sexpr(S(a[0]))));
        catch (Exception e) return err(e.msg);
    });
    prim(s, "SExpr.render", "SExpr -> String", 1, (C c, V a) => str(from_value(a[0]).dump()));
    prim(s, "SExpr.atom_text", "SExpr -> Option String", 1, delegate Value(C c, V a) {
        auto t = agg_of(a[0]);
        if (t.tag == "Atom") return some(t.items[0]);
        return none();
    });
}

