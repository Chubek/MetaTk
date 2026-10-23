/**
 * metalib/d/Support.d — shared helpers for the D metalib host modules.
 *
 * Each host module `metalib/d/X.d` mirrors `metalib/cpp/X.h` with identical
 * primitive names, types and behaviour (AGENTS.md §18.2).
 *
 * Note: D delegates created in a loop share one closure context; modules
 * build per-iteration delegates through helper functions instead.
 */
module metalib.d.Support;

public import MetaML;
public import std.typecons : Tuple, tuple, Nullable, nullable;
import core.sync.mutex : Mutex;

alias V = Value[];
alias C = PrimContext;

string S(Value v) { return str_of(v); }
Value str(string s) { return make_str(s); }
Value integer(long i) { return Value.integer(i); }
Value floating(double d) { return Value.floating(d); }
Value boolean(bool b) { return Value.boolean(b); }
Value unit() { return Value.unit(); }
Value some(Value v) { return make_some(v); }
Value none() { return make_none(); }
Value ok(Value v) { return make_ok(v); }
Value err(string msg) { return make_err(make_str(msg)); }
Value list(Value[] xs) { return make_list(xs); }
Value[] items(Value v) { return list_items(v); }
Value pair(Value a, Value b) { return make_tuple([a, b]); }
Value triple(Value a, Value b, Value c) { return make_tuple([a, b, c]); }
Value fst(Value t) { return agg_of(t).items[0]; }
Value snd(Value t) { return agg_of(t).items[1]; }
Value record(Tuple!(string, Value)[] fields) { return make_record(fields); }
Value field(Value r, string label)
{
    auto f = record_field(r, label);
    return f is null ? Value.unit() : *f;
}
bool is_ctor(Value v, string name) { return v.kind == VK.Ctor && agg_of(v).tag == name; }
Value arg(Value ctor, size_t i) { return agg_of(ctor).items[i]; }
Value strings(const(string)[] xs)
{
    Value[] result;
    foreach (x; xs) result ~= str(x);
    return list(result);
}

/// Registers a sandboxed library primitive.
void prim(Session s, string name, string type, int arity, PrimFn fn)
{
    s.install_primitive(name, type, arity, fn);
}

/// Host handle holding a T (thread-safe access via `mutex`).
class Handle(T) : ForeignObj
{
    T data;
    Mutex mutex;
    this(string type)
    {
        super(type);
        mutex = new Mutex;
    }
}

Value wrap_handle(H)(H h)
{
    Value v;
    v.kind = VK.Foreign;
    v.obj = h;
    return v;
}

Handle!T handle(T)(Value v, string type, C ctx)
{
    auto h = cast(Handle!T) v.obj;
    if (v.kind != VK.Foreign || h is null || h.type_name != type)
        ctx.error("M0404", "expected a " ~ type ~ " handle");
    return h;
}
