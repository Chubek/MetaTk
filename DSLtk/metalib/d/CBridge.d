/// Legacy C ABI, mirrored by metalib/cpp/CBridge.h. Does not collide with
/// the length-aware metaml_c_* adapter. GC roots retain sessions for C callers.
module metalib.d.CBridge;
import metalib.d.Support;
import core.memory : GC;
import core.stdc.stdlib : malloc, free;
import core.stdc.string : memcpy;
import std.string : fromStringz;

void installCBridge(Session s)
{
    prim(s, "CBridge.api_version", "Unit -> Int", 1, (C c, V a) => integer(1));
    prim(s, "CBridge.functions", "Unit -> [String]", 1, (C c, V a) => strings([
        "metaml_new", "metaml_free", "metaml_load_stdlib", "metaml_eval", "metaml_type_of",
        "metaml_free_string", "metaml_version"]));
}
private char* duplicate(string text)
{
    auto p = cast(char*) malloc(text.length + 1);
    if (p is null) return null;
    if (text.length) memcpy(p, text.ptr, text.length);
    p[text.length] = 0; return p;
}
extern(C) void* metaml_new()
{
    try { auto s = new Session; GC.addRoot(cast(void*) s); return cast(void*) s; }
    catch (Exception) { return null; }
}
extern(C) void metaml_free(void* p)
{
    if (p is null) return;
    try (cast(Session) p).close(); catch (Exception) {}
    GC.removeRoot(p);
}
extern(C) int metaml_load_stdlib(void* p, const(char)* directory)
{
    if (p is null) return 1;
    try return (cast(Session) p).load_stdlib(directory is null ? "" : fromStringz(directory).idup).is_ok() ? 0 : 1;
    catch (Exception) { return 1; }
}
extern(C) char* metaml_eval(void* p, const(char)* source, int* good)
{
    if (good !is null) *good = 0;
    if (p is null) return duplicate("invalid session");
    try
    {
        auto s = cast(Session) p;
        auto r = s.eval(source is null ? "" : fromStringz(source).idup, "<c>");
        string text;
        if (r.is_err()) foreach (d; r.unwrap_err()) text ~= s.render(d);
        else { text = show_value(r.unwrap()); if (good !is null) *good = 1; }
        return duplicate(text);
    }
    catch (Exception e) { return duplicate(e.msg); }
}
extern(C) char* metaml_type_of(void* p, const(char)* source, int* good)
{
    if (good !is null) *good = 0;
    if (p is null) return duplicate("invalid session");
    try
    {
        auto r = (cast(Session) p).type_of(source is null ? "" : fromStringz(source).idup);
        if (r.is_err()) return duplicate(r.unwrap_err().length ? r.unwrap_err()[0].headline() : "error");
        if (good !is null) *good = 1;
        return duplicate(r.unwrap());
    }
    catch (Exception e) { return duplicate(e.msg); }
}
extern(C) void metaml_free_string(char* p) { free(p); }
extern(C) const(char)* metaml_version() { return (metamlVersion ~ "\0").ptr; }
