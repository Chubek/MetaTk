/**
 * metalib/d/Module.d — MetaML host module `Module` (D dialect): introspection of the modules known to the checker.
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/Module.h):
 *   Module.names : Unit -> [String]
 *   Module.functors : Unit -> [String]
 *   Module.signatures : Unit -> [String]
 *   Module.exists : String -> Bool
 *   Module.members : String -> Option [String]
 *   Module.types : String -> Option [String]
 *   Module.submodules : String -> Option [String]
 *   Module.member_type : String -> String -> Option String
 *   Module.signature : String -> Option String
 */
module metalib.d.Module;

import MetaML;
import metalib.d.Support;

import std.string : split;

private Scope find_module(Session s, string path)
{
    auto segs = path.split(".");
    auto m = s.checker().global.module_(segs[0]);
    Scope cur = m is null ? null : *m;
    foreach (seg; segs[1 .. $])
    {
        if (cur is null) break;
        auto n = cur.modules.find(seg);
        cur = n is null ? null : *n;
    }
    return cur;
}

private string type_head(TypeDef d)
{
    string result = "type " ~ d.name;
    foreach (i; 0 .. d.params.length) result ~= " '" ~ cast(char)('a' + cast(int)(i % 26));
    return result;
}

private string signature(Scope m, string indent)
{
    string result = "sig\n";
    foreach (k; m.types.keys())
    {
        auto d = *m.types.find(k);
        result ~= indent ~ "  " ~ type_head(d);
        if (d.alias_ && d.body !is null) result ~= " = " ~ show_type(d.body);
        result ~= "\n";
        foreach (c; d.ctors) result ~= indent ~ "    | " ~ c.name ~ " : " ~ show_type(c.scheme) ~ "\n";
    }
    foreach (k; m.vars.keys()) result ~= indent ~ "  val " ~ k ~ " : " ~ show_type(m.vars.find(k).scheme) ~ "\n";
    foreach (k; m.modules.keys()) result ~= indent ~ "  module " ~ k ~ " : " ~ signature(*m.modules.find(k), indent ~ "  ");
    foreach (k; m.functors.keys()) result ~= indent ~ "  functor " ~ k ~ "\n";
    return result ~ indent ~ "end\n";
}

private Value keys_value(const(string)[] ks) { return strings(ks); }

void installModule(Session s)
{
    prim(s, "Module.names", "Unit -> [String]", 1, delegate Value(C c, V a) => keys_value(c.session.checker().global.modules.keys()));
    prim(s, "Module.functors", "Unit -> [String]", 1, delegate Value(C c, V a) => keys_value(c.session.checker().global.functors.keys()));
    prim(s, "Module.signatures", "Unit -> [String]", 1, delegate Value(C c, V a) => keys_value(c.session.checker().global.sigs.keys()));
    prim(s, "Module.exists", "String -> Bool", 1, delegate Value(C c, V a) => boolean(find_module(c.session, S(a[0])) !is null));
    prim(s, "Module.members", "String -> Option [String]", 1, delegate Value(C c, V a) {
        auto m = find_module(c.session, S(a[0]));
        return m !is null ? some(keys_value(m.vars.keys())) : none();
    });
    prim(s, "Module.types", "String -> Option [String]", 1, delegate Value(C c, V a) {
        auto m = find_module(c.session, S(a[0]));
        return m !is null ? some(keys_value(m.types.keys())) : none();
    });
    prim(s, "Module.submodules", "String -> Option [String]", 1, delegate Value(C c, V a) {
        auto m = find_module(c.session, S(a[0]));
        return m !is null ? some(keys_value(m.modules.keys())) : none();
    });
    prim(s, "Module.member_type", "String -> String -> Option String", 2, delegate Value(C c, V a) {
        auto m = find_module(c.session, S(a[0]));
        if (m is null) return none();
        auto v = m.vars.find(S(a[1]));
        return v !is null ? some(str(show_type(v.scheme))) : none();
    });
    prim(s, "Module.signature", "String -> Option String", 1, delegate Value(C c, V a) {
        auto m = find_module(c.session, S(a[0]));
        return m !is null ? some(str(signature(m, ""))) : none();
    });
}

