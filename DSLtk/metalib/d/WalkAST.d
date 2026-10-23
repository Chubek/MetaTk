/**
 * metalib/d/WalkAST.d — MetaML host module `WalkAST` (D dialect): traversals and rewriting of `Ast` trees.
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/WalkAST.h):
 *   WalkAST.preorder : Ast -> [Ast]
 *   WalkAST.postorder : Ast -> [Ast]
 *   WalkAST.breadth_first : Ast -> [Ast]
 *   WalkAST.leaves : Ast -> [Ast]
 *   WalkAST.map_bottom_up : (Ast -> Ast) -> Ast -> Ast
 *   WalkAST.map_top_down : (Ast -> Ast) -> Ast -> Ast
 *   WalkAST.rewrite : (Ast -> Option Ast) -> Ast -> Ast
 *   WalkAST.find_all : (Ast -> Bool) -> Ast -> [Ast]
 */
module metalib.d.WalkAST;

import MetaML;
import metalib.d.Support;
import metalib.d.ASTBuilder;

private void pre(Value v, ref Value[] result)
{
    result ~= v;
    foreach (k; kids(v)) pre(k, result);
}
private void post(Value v, ref Value[] result)
{
    foreach (k; kids(v)) post(k, result);
    result ~= v;
}
private Value map_up(C c, Value f, Value v)
{
    if (is_leaf(v)) return c.apply(f, [v]);
    Value[] ks;
    foreach (k; kids(v)) ks ~= map_up(c, f, k);
    return c.apply(f, [node(tag_of(v), ks)]);
}
private Value map_down(C c, Value f, Value v)
{
    Value r = c.apply(f, [v]);
    if (is_leaf(r)) return r;
    Value[] ks;
    foreach (k; kids(r)) ks ~= map_down(c, f, k);
    return node(tag_of(r), ks);
}

private enum long rewrite_limit = 100000;

private Value rewrite(C c, Value rule, Value v0, ref long steps)
{
    import std.conv : to;
    Value v = v0;
    if (!is_leaf(v))
    {
        Value[] ks;
        foreach (k; kids(v)) ks ~= rewrite(c, rule, k, steps);
        v = node(tag_of(v), ks);
    }
    while (true)
    {
        Value r = c.apply(rule, [v]);
        auto a = agg_of(r);
        if (a.tag != "Some") return v;
        if (++steps > rewrite_limit)
            c.error("M0404", "WalkAST.rewrite: no normal form after " ~ to!string(rewrite_limit) ~ " rewrites");
        v = rewrite(c, rule, a.items[0], steps);
    }
}
private void find_all(C c, Value p, Value v, ref Value[] result)
{
    if (c.apply(p, [v]).i) result ~= v;
    foreach (k; kids(v)) find_all(c, p, k, result);
}

void installWalkAST(Session s)
{
    declare_ast_type(s);
    prim(s, "WalkAST.preorder", "Ast -> [Ast]", 1, delegate Value(C c, V a) {
        Value[] result;
        pre(a[0], result);
        return list(result);
    });
    prim(s, "WalkAST.postorder", "Ast -> [Ast]", 1, delegate Value(C c, V a) {
        Value[] result;
        post(a[0], result);
        return list(result);
    });
    prim(s, "WalkAST.breadth_first", "Ast -> [Ast]", 1, delegate Value(C c, V a) {
        Value[] result = [a[0]];
        for (size_t i = 0; i < result.length; ++i) foreach (k; kids(result[i])) result ~= k;
        return list(result);
    });
    prim(s, "WalkAST.leaves", "Ast -> [Ast]", 1, delegate Value(C c, V a) {
        Value[] all, result;
        pre(a[0], all);
        foreach (v; all) if (is_leaf(v)) result ~= v;
        return list(result);
    });
    prim(s, "WalkAST.map_bottom_up", "(Ast -> Ast) -> Ast -> Ast", 2, delegate Value(C c, V a) => map_up(c, a[0], a[1]));
    prim(s, "WalkAST.map_top_down", "(Ast -> Ast) -> Ast -> Ast", 2, delegate Value(C c, V a) => map_down(c, a[0], a[1]));
    prim(s, "WalkAST.rewrite", "(Ast -> Option Ast) -> Ast -> Ast", 2, delegate Value(C c, V a) {
        long steps = 0;
        return rewrite(c, a[0], a[1], steps);
    });
    prim(s, "WalkAST.find_all", "(Ast -> Bool) -> Ast -> [Ast]", 2, delegate Value(C c, V a) {
        Value[] result;
        find_all(c, a[0], a[1], result);
        return list(result);
    });
}

