/**
 * metalib/d/Bridge.d — MetaML host module `Bridge` (D dialect): runtime inspection and marshaling of MetaML values for host code.
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/Bridge.h):
 *   Bridge.kind, encode, fields, ctor_name, arity, size, same
 */
module metalib.d.Bridge;

import MetaML;
import metalib.d.Support;


private string kind_name(Value v)
{
    final switch (v.kind)
    {
        case VK.Unit: return "Unit";
        case VK.Bool: return "Bool";
        case VK.Int: return "Int";
        case VK.Float: return "Float";
        case VK.Str: return "String";
        case VK.List: return "List";
        case VK.Tuple: return "Tuple";
        case VK.Record: return "Record";
        case VK.Ctor: return "Ctor";
        case VK.Closure: case VK.Prim: case VK.CtorFn: case VK.Compose: return "Function";
        case VK.Module: return "Module";
        case VK.Foreign: return "Foreign";
    }
}

private long tree_size(Value v)
{
    long n = 1;
    if (v.kind == VK.List) foreach (x; list_items(v)) n += tree_size(x);
    else if (v.kind == VK.Tuple || v.kind == VK.Record || v.kind == VK.Ctor)
        foreach (x; agg_of(v).items) n += tree_size(x);
    return n;
}

void installBridge(Session s)
{
    prim(s, "Bridge.kind", "'a -> String", 1, (C c, V a) => str(kind_name(a[0])));
    prim(s, "Bridge.encode", "'a -> String", 1, (C c, V a) => str(show_value(a[0])));
    prim(s, "Bridge.fields", "'a -> [String]", 1, delegate Value(C c, V a) {
        if (a[0].kind != VK.Record) return list(null);
        return strings(agg_of(a[0]).labels);
    });
    prim(s, "Bridge.ctor_name", "'a -> Option String", 1, delegate Value(C c, V a) {
        if (a[0].kind != VK.Ctor) return none();
        return some(str(agg_of(a[0]).tag));
    });
    prim(s, "Bridge.arity", "'a -> Int", 1, delegate Value(C c, V a) {
        Value v = a[0];
        if (v.kind == VK.List) return integer(cast(long) items(v).length);
        if (v.kind == VK.Tuple || v.kind == VK.Record || v.kind == VK.Ctor)
            return integer(cast(long) agg_of(v).items.length);
        return integer(0);
    });
    prim(s, "Bridge.size", "'a -> Int", 1, (C c, V a) => integer(tree_size(a[0])));
    prim(s, "Bridge.same", "'a -> 'a -> Bool", 2, delegate Value(C c, V a) {
        if (a[0].obj is null || a[1].obj is null) return boolean(false);
        return boolean(a[0].obj is a[1].obj);
    });
}

