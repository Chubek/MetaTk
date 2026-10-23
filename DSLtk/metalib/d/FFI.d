/// Dynamic native calls via libffi, mirrored by metalib/cpp/FFI.h.
module metalib.d.FFI;
import metalib.d.Support;
import core.sys.posix.dlfcn;
import core.sync.mutex : Mutex;
import std.string : toStringz, fromStringz, indexOf;
import std.conv : to;

private struct Api
{
    void* library;
    extern(C) int function(void*, uint, uint, void*, void**) prep;
    extern(C) void function(void*, void*, void*, void**) invoke;
    void* i; void* l; void* d; void* s; void* v;
    void* type(char ch) { return ch == 'i' ? i : ch == 'l' ? l : ch == 'd' ? d : ch == 's' ? s : v; }
    bool ready() { return prep !is null && invoke !is null && i && l && d && s && v; }
}
private __gshared Api api;
private __gshared Mutex lock;
private __gshared void*[string] libraries;
shared static this()
{
    lock = new Mutex;
    version (X86_64) enum supported = true;
    else version (AArch64) enum supported = true;
    else enum supported = false;
    static if (supported)
    {
        foreach (name; ["libffi.so.8", "libffi.so", "libffi.dylib"])
            if ((api.library = dlopen(name.toStringz, RTLD_NOW | RTLD_LOCAL)) !is null) break;
        if (api.library !is null)
        {
            api.prep = cast(typeof(api.prep)) dlsym(api.library, "ffi_prep_cif");
            api.invoke = cast(typeof(api.invoke)) dlsym(api.library, "ffi_call");
            api.i = dlsym(api.library, "ffi_type_sint32"); api.l = dlsym(api.library, "ffi_type_sint64");
            api.d = dlsym(api.library, "ffi_type_double"); api.s = dlsym(api.library, "ffi_type_pointer");
            api.v = dlsym(api.library, "ffi_type_void");
        }
    }
}
private void* openLibrary(string name, ref string why)
{
    lock.lock(); scope (exit) lock.unlock();
    if (auto p = name in libraries) return *p;
    auto h = dlopen(name.length ? name.toStringz : null, RTLD_NOW | RTLD_LOCAL);
    if (h is null)
    {
        auto e = dlerror();
        why = "cannot open library `" ~ name ~ "`" ~ (e is null ? "" : ": " ~ fromStringz(e).idup);
        return null;
    }
    libraries[name] = h; return h;
}
private Value invoke(void* fn, string sig, Value[] args)
{
    if (sig.length < 3 || sig[1] != '(' || sig[$ - 1] != ')')
        return err("FFI.call: malformed signature `" ~ sig ~ "` (expected e.g. `d(d)`)");
    char ret = sig[0];
    auto ps = sig[2 .. $ - 1];
    if ("ildsv".indexOf(ret) < 0) return err("FFI.call: unknown return type `" ~ ret ~ "`");
    if (ps.length != args.length) return err("FFI.call: signature `" ~ sig ~ "` takes " ~ to!string(ps.length)
        ~ " argument(s) but " ~ to!string(args.length) ~ " were given");
    struct Arg { int i; long l; double d; const(char)* s; }
    auto storage = new Arg[args.length];
    void*[] types, values;
    size_t ni, nd;
    foreach (k, p; ps)
    {
        auto a = agg_of(args[k]);
        string tag = a.tag;
        if (p == 'i' || p == 'l')
        {
            if (tag != "FInt") return err("FFI.call: argument " ~ to!string(k + 1) ~ " must be FInt");
            if (ni++ >= 6) return err("FFI.call: too many integer arguments (at most 6)");
            storage[k].i = cast(int) a.items[0].i; storage[k].l = a.items[0].i;
            values ~= p == 'i' ? cast(void*) &storage[k].i : &storage[k].l;
        }
        else if (p == 'd')
        {
            if (tag != "FFloat") return err("FFI.call: argument " ~ to!string(k + 1) ~ " must be FFloat");
            if (nd++ >= 8) return err("FFI.call: too many float arguments (at most 8)");
            storage[k].d = a.items[0].f; values ~= &storage[k].d;
        }
        else if (p == 's')
        {
            if (tag != "FStr") return err("FFI.call: argument " ~ to!string(k + 1) ~ " must be FStr");
            if (ni++ >= 6) return err("FFI.call: too many integer arguments (at most 6)");
            storage[k].s = str_of(a.items[0]).toStringz; values ~= &storage[k].s;
        }
        else return err("FFI.call: unknown argument type `" ~ p ~ "`");
        types ~= api.type(p);
    }
    align(16) ubyte[256] cif;
    version (X86_64) enum uint abi = 2;
    else enum uint abi = 1;
    if (api.prep(cif.ptr, abi, cast(uint) types.length, api.type(ret), types.ptr))
        return err("FFI.call: cannot prepare the native call");
    union NativeResult { ulong bits; double number; const(char)* text; }
    NativeResult r;
    api.invoke(cif.ptr, fn, &r, values.ptr);
    if (ret == 'd') return ok(make_ctor("FFloat", 1, [floating(r.number)]));
    if (ret == 'i') return ok(make_ctor("FInt", 0, [integer(cast(int) r.bits)]));
    if (ret == 'l') return ok(make_ctor("FInt", 0, [integer(cast(long) r.bits)]));
    if (ret == 's') return ok(make_ctor("FStr", 2, [str(r.text is null ? "" : fromStringz(r.text).idup)]));
    return ok(make_ctor("FVoid", 3));
}
void installFFI(Session s)
{
    if (s.checker().global.types.find("FfiValue") is null)
        s.declare("type FfiValue := | FInt Int | FFloat Float | FStr String | FVoid\n", "<metalib FFI>");
    prim(s, "FFI.available", "Unit -> Bool", 1, (C c, V a) => boolean(api.ready()));
    prim(s, "FFI.symbol_exists", "String -> String -> Bool", 2, (C c, V a) {
        if (!api.ready()) return boolean(false);
        string why; auto h = openLibrary(S(a[0]), why);
        return boolean(h !is null && dlsym(h, S(a[1]).toStringz) !is null);
    });
    prim(s, "FFI.call", "String -> String -> String -> [FfiValue] -> Result FfiValue String", 4, (C c, V a) {
        if (!api.ready()) return err("FFI.call: not supported on this platform");
        string why; auto h = openLibrary(S(a[0]), why);
        if (h is null) return err("FFI.call: " ~ why);
        auto fn = dlsym(h, S(a[1]).toStringz);
        if (fn is null) return err("FFI.call: no symbol `" ~ S(a[1]) ~ "` in `" ~ S(a[0]) ~ "`");
        return invoke(fn, S(a[2]), items(a[3]));
    });
}
