/**
 * metalib/d/VisitorBuilder.d — MetaML host module `VisitorBuilder` (D dialect): build visitors / folds over `Ast` (functions made with `make_function`).
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/VisitorBuilder.h):
 *   VisitorBuilder.make : (String -> String -> 'r) -> (String -> ['r] -> 'r) -> (Ast -> 'r)
 *   VisitorBuilder.fold : (String -> String -> 'r) -> (String -> ['r] -> 'r) -> Ast -> 'r
 *   VisitorBuilder.dispatch : [(String, Ast -> 'r)] -> (Ast -> 'r) -> Ast -> 'r
 *   VisitorBuilder.count_tags : Ast -> [(String, Int)]
 *   VisitorBuilder.tags : Ast -> [String]
 */
module metalib.d.VisitorBuilder;

import MetaML;
import metalib.d.Support;
import metalib.d.ASTBuilder;

private Value fold(C c, Value on_leaf, Value on_node, Value v)
{
    if (is_leaf(v)) return c.apply(on_leaf, [make_str(tag_of(v)), make_str(text_of(v))]);
    Value[] rs;
    foreach (k; kids(v)) rs ~= fold(c, on_leaf, on_node, k);
    return c.apply(on_node, [make_str(tag_of(v)), make_list(rs)]);
}
private void count(Value v, ref long[string] m)
{
    m[tag_of(v)] = m.get(tag_of(v), 0) + 1;
    foreach (k; kids(v)) count(k, m);
}
private void tags(Value v, ref string[] result)
{
    import std.algorithm.searching : canFind;
    if (!result.canFind(tag_of(v))) result ~= tag_of(v);
    foreach (k; kids(v)) tags(k, result);
}
private PrimFn visitor(Value on_leaf, Value on_node)
{
    return delegate Value(C cc, V b) => fold(cc, on_leaf, on_node, b[0]);
}

void installVisitorBuilder(Session s)
{
    declare_ast_type(s);
    prim(s, "VisitorBuilder.make", "(String -> String -> 'r) -> (String -> ['r] -> 'r) -> (Ast -> 'r)", 2,
        delegate Value(C c, V a) => c.session.make_function("visitor", 1, visitor(a[0], a[1])));
    prim(s, "VisitorBuilder.fold", "(String -> String -> 'r) -> (String -> ['r] -> 'r) -> Ast -> 'r", 3,
        delegate Value(C c, V a) => fold(c, a[0], a[1], a[2]));
    prim(s, "VisitorBuilder.dispatch", "[(String, Ast -> 'r)] -> (Ast -> 'r) -> Ast -> 'r", 3,
        delegate Value(C c, V a) {
            string t = tag_of(a[2]);
            foreach (h; items(a[0]))
                if (S(fst(h)) == t) return c.apply(snd(h), [a[2]]);
            return c.apply(a[1], [a[2]]);
        });
    prim(s, "VisitorBuilder.count_tags", "Ast -> [(String, Int)]", 1, delegate Value(C c, V a) {
        import std.algorithm.sorting : sort;
        long[string] m;
        count(a[0], m);
        Value[] result;
        foreach (k; m.keys.sort) result ~= pair(str(k), integer(m[k]));
        return list(result);
    });
    prim(s, "VisitorBuilder.tags", "Ast -> [String]", 1, delegate Value(C c, V a) {
        string[] result;
        tags(a[0], result);
        return strings(result);
    });
}

