/**
 * metalib/d/WalkVisitor.d — MetaML host module `WalkVisitor` (D dialect): callback-driven walks over `Ast` trees.
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/WalkVisitor.h):
 *   WalkVisitor.walk : (Ast -> Unit) -> Ast -> Unit
 *   WalkVisitor.walk_depth : (Int -> Ast -> Unit) -> Ast -> Unit
 *   WalkVisitor.enter_leave : (Ast -> Unit) -> (Ast -> Unit) -> Ast -> Unit
 *   WalkVisitor.path_to : (Ast -> Bool) -> Ast -> Option [Int]
 *   WalkVisitor.at_path : [Int] -> Ast -> Option Ast
 */
module metalib.d.WalkVisitor;

import MetaML;
import metalib.d.Support;
import metalib.d.ASTBuilder;

private void walk(C c, Value f, Value v)
{
    c.apply(f, [v]);
    foreach (k; kids(v)) walk(c, f, k);
}
private void walk_depth(C c, Value f, Value v, long d)
{
    c.apply(f, [Value.integer(d), v]);
    foreach (k; kids(v)) walk_depth(c, f, k, d + 1);
}
private void enter_leave(C c, Value enter, Value leave, Value v)
{
    c.apply(enter, [v]);
    foreach (k; kids(v)) enter_leave(c, enter, leave, k);
    c.apply(leave, [v]);
}
private bool path_to(C c, Value p, Value v, ref Value[] path)
{
    if (c.apply(p, [v]).i) return true;
    auto ks = kids(v);
    foreach (i, k; ks)
    {
        path ~= Value.integer(cast(long) i);
        if (path_to(c, p, k, path)) return true;
        path = path[0 .. $ - 1];
    }
    return false;
}

void installWalkVisitor(Session s)
{
    declare_ast_type(s);
    prim(s, "WalkVisitor.walk", "(Ast -> Unit) -> Ast -> Unit", 2, delegate Value(C c, V a) {
        walk(c, a[0], a[1]);
        return unit();
    });
    prim(s, "WalkVisitor.walk_depth", "(Int -> Ast -> Unit) -> Ast -> Unit", 2, delegate Value(C c, V a) {
        walk_depth(c, a[0], a[1], 0);
        return unit();
    });
    prim(s, "WalkVisitor.enter_leave", "(Ast -> Unit) -> (Ast -> Unit) -> Ast -> Unit", 3, delegate Value(C c, V a) {
        enter_leave(c, a[0], a[1], a[2]);
        return unit();
    });
    prim(s, "WalkVisitor.path_to", "(Ast -> Bool) -> Ast -> Option [Int]", 2, delegate Value(C c, V a) {
        Value[] path;
        if (path_to(c, a[0], a[1], path)) return some(list(path));
        return none();
    });
    prim(s, "WalkVisitor.at_path", "[Int] -> Ast -> Option Ast", 2, delegate Value(C c, V a) {
        Value cur = a[1];
        foreach (i; items(a[0]))
        {
            auto ks = kids(cur);
            if (i.i < 0 || cast(size_t) i.i >= ks.length) return none();
            cur = ks[cast(size_t) i.i];
        }
        return some(cur);
    });
}

