/**
 * metalib/d/Type.d — MetaML host module `Type` (D dialect): type expressions: normalisation, unification, instance checks.
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/Type.h):
 *   Type.normalize : String -> Result String String
 *   Type.unify : String -> String -> Result String String
 *   Type.instance_of : String -> String -> Bool
 *   Type.equal : String -> String -> Bool
 *   Type.arity : String -> Result Int String
 */
module metalib.d.Type;

import MetaML;
import metalib.d.Support;
import metalib.d.Frontend;

import std.conv : to;

private class TypeFail : Exception
{
    this(string m) { super(m); }
}

/// Resolves `src` in the global scope (at a fresh level). Names shared
/// through `m` denote the same variable.
private TypeNode resolve(Checker ck, string src, Checker.TVarMap m)
{
    auto te = parse_type_expr(src, "<type>");
    if (te.is_err()) throw new TypeFail(first_diag(te.unwrap_err()));
    try return ck.resolve_type(te.unwrap(), ck.global, m);
    catch (MetaError e) throw new TypeFail(diag_text(e.diag));
}

private void make_rigid(TypeNode t0, ref bool[TypeNode] seen)
{
    TypeNode t = prune(t0);
    if (t is null || t in seen) return;
    seen[t] = true;
    if (t.kind == TK.Var)
    {
        t.rigid = true;
        if (!t.hint.length) t.hint = "'r" ~ to!string(seen.length);
        return;
    }
    foreach (x; t.args) make_rigid(x, seen);
    if (t.field !is null) make_rigid(t.field, seen);
    if (t.rest !is null) make_rigid(t.rest, seen);
}

/// Runs `body` with the unifier trail rolled back afterwards.
private Value scoped(Session s, Value delegate(Checker) body)
{
    auto ck = s.checker();
    auto snap = ck.u.snapshot();
    int level = ck.level;
    ++ck.level;
    try
    {
        Value v = body(ck);
        ck.level = level;
        ck.u.rollback(snap);
        return v;
    }
    catch (TypeFail f)
    {
        ck.level = level;
        ck.u.rollback(snap);
        return err(f.msg);
    }
}

private bool instance_of(Checker ck, string a, string b)
{
    auto ta = resolve(ck, a, new Checker.TVarMap);
    auto tb = resolve(ck, b, new Checker.TVarMap);
    bool[TypeNode] seen;
    make_rigid(ta, seen);
    return ck.u.unify(tb, ta);
}

private long arity(TypeNode t0)
{
    TypeNode t = prune(t0);
    long n = 0;
    while (is_fun(t))
    {
        ++n;
        t = prune(t.args[1]);
    }
    return n;
}

void installType(Session s)
{
    prim(s, "Type.normalize", "String -> Result String String", 1, delegate Value(C c, V a) => format_type(c.session, S(a[0])));
    prim(s, "Type.unify", "String -> String -> Result String String", 2, delegate Value(C c, V a) {
        return scoped(c.session, delegate Value(Checker ck) {
            auto m = new Checker.TVarMap;
            auto x = resolve(ck, S(a[0]), m);
            auto y = resolve(ck, S(a[1]), m);
            if (!ck.u.unify(x, y))
                return err("Type.unify: `" ~ S(a[0]) ~ "` and `" ~ S(a[1]) ~ "` do not unify"
                    ~ (ck.u.reason.length ? ": " ~ ck.u.reason : ""));
            ck.generalize(x, ck.level - 1);
            return ok(str(show_type(x)));
        });
    });
    prim(s, "Type.instance_of", "String -> String -> Bool", 2, delegate Value(C c, V a) {
        auto r = scoped(c.session, delegate Value(Checker ck) => boolean(instance_of(ck, S(a[0]), S(a[1]))));
        return r.kind == VK.Bool ? r : boolean(false);
    });
    prim(s, "Type.equal", "String -> String -> Bool", 2, delegate Value(C c, V a) {
        auto one = scoped(c.session, delegate Value(Checker ck) => boolean(instance_of(ck, S(a[0]), S(a[1]))));
        auto two = scoped(c.session, delegate Value(Checker ck) => boolean(instance_of(ck, S(a[1]), S(a[0]))));
        return boolean(one.kind == VK.Bool && two.kind == VK.Bool && one.i && two.i);
    });
    prim(s, "Type.arity", "String -> Result Int String", 1, delegate Value(C c, V a) {
        return scoped(c.session, delegate Value(Checker ck) => ok(integer(arity(resolve(ck, S(a[0]), new Checker.TVarMap)))));
    });
}

