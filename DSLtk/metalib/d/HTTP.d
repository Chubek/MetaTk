/// HTTP/HTTPS client via dynamically loaded libcurl, mirrored by HTTP.h.
module metalib.d.HTTP;
import metalib.d.Support;
import core.sys.posix.dlfcn;
import core.stdc.config : c_long;
import std.string : toStringz, fromStringz, indexOf;

enum size_t maxBody = 16 * 1024 * 1024, maxHeaders = 65536;
bool httpToken(string s)
{
    if (!s.length) return false;
    foreach (char ch; s)
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')
            || "!#$%&'*+-.^_`|~".indexOf(ch) >= 0)) return false;
    return true;
}
bool httpHeaderValue(string s)
{
    foreach (ubyte ch; cast(const(ubyte)[]) s) if ((ch < 32 && ch != '\t') || ch == 127) return false;
    return true;
}
private struct Api
{
    void* library;
    extern(C) int function(c_long) globalInit;
    extern(C) void* function() init;
    extern(C) int function(void*, int, ...) setopt;
    extern(C) int function(void*) perform;
    extern(C) int function(void*, int, ...) getinfo;
    extern(C) void function(void*) cleanup;
    extern(C) void* function(void*, const(char)*) append;
    extern(C) void function(void*) freeList;
    extern(C) const(char)* function(int) error;
    bool initialized;
}
private __gshared Api api;
shared static this()
{
    foreach (name; ["libcurl.so.4", "libcurl.so", "libcurl.dylib"])
        if ((api.library = dlopen(name.toStringz, RTLD_NOW | RTLD_LOCAL)) !is null) break;
    if (api.library is null) return;
    api.globalInit = cast(typeof(api.globalInit)) dlsym(api.library, "curl_global_init");
    api.init = cast(typeof(api.init)) dlsym(api.library, "curl_easy_init");
    api.setopt = cast(typeof(api.setopt)) dlsym(api.library, "curl_easy_setopt");
    api.perform = cast(typeof(api.perform)) dlsym(api.library, "curl_easy_perform");
    api.getinfo = cast(typeof(api.getinfo)) dlsym(api.library, "curl_easy_getinfo");
    api.cleanup = cast(typeof(api.cleanup)) dlsym(api.library, "curl_easy_cleanup");
    api.append = cast(typeof(api.append)) dlsym(api.library, "curl_slist_append");
    api.freeList = cast(typeof(api.freeList)) dlsym(api.library, "curl_slist_free_all");
    api.error = cast(typeof(api.error)) dlsym(api.library, "curl_easy_strerror");
    api.initialized = api.globalInit !is null && api.init !is null && api.setopt !is null && api.perform !is null
        && api.getinfo !is null && api.cleanup !is null && api.append !is null && api.freeList !is null
        && api.error !is null && api.globalInit(3L) == 0;
}
private struct Response
{
    string body;
    Tuple!(string, string)[] headers;
    size_t headerBytes;
    bool exceeded;
    Interp interp;
}
private extern(C) size_t bodyCallback(char* data, size_t size, size_t count, void* user) nothrow
{
    auto r = cast(Response*) user;
    if (size && count > maxBody / size) { r.exceeded = true; return 0; }
    auto n = size * count;
    if (n > maxBody - r.body.length) { r.exceeded = true; return 0; }
    try { r.body ~= data[0 .. n]; return n; } catch (Throwable) { return 0; }
}
private extern(C) size_t headerCallback(char* data, size_t size, size_t count, void* user) nothrow
{
    auto r = cast(Response*) user;
    if (size && count > maxHeaders / size) { r.exceeded = true; return 0; }
    auto n = size * count;
    if (n > maxHeaders - r.headerBytes) { r.exceeded = true; return 0; }
    r.headerBytes += n;
    try
    {
        auto line = data[0 .. n].idup;
        if (line.length >= 5 && line[0 .. 5] == "HTTP/") r.headers = null;
        else
        {
            auto p = line.indexOf(':');
            if (p >= 0)
                r.headers ~= tuple(ascii_map(line[0 .. p], false), trim_ws(line[p + 1 .. $]));
        }
        return n;
    }
    catch (Throwable) { return 0; }
}
private extern(C) int progressCallback(void* user, long, long, long, long) nothrow
{
    auto i = (cast(Response*) user).interp;
    try return i !is null && i.cancelled !is null && i.cancelled() ? 1 : 0;
    catch (Throwable) { return 1; }
}
private Value request(C c, string method, string url, Value headers, string body, long timeout)
{
    if (!api.initialized) return err("HTTP.request: libcurl is unavailable");
    if (!httpToken(method)) return err("HTTP.request: invalid method");
    if (!((url.length >= 7 && url[0 .. 7] == "http://") || (url.length >= 8 && url[0 .. 8] == "https://"))
        || !httpHeaderValue(url) || url.indexOf('\t') >= 0 || url.indexOf(' ') >= 0)
        return err("HTTP.request: expected an http:// or https:// URL");
    if (timeout <= 0 || timeout > 300000) return err("HTTP.request: timeout must be in 1..300000 milliseconds");
    if (body.length > maxBody) return err("HTTP.request: body exceeds 16 MiB");
    auto easy = api.init();
    if (easy is null) return err("HTTP.request: cannot allocate a client");
    scope(exit) api.cleanup(easy);
    void* hs;
    scope(exit) if (hs !is null) api.freeList(hs);
    size_t headerBytes;
    foreach (h; items(headers))
    {
        auto name = S(fst(h)), value = S(snd(h));
        if (!httpToken(name) || !httpHeaderValue(value)) return err("HTTP.request: invalid header");
        auto line = name ~ ": " ~ value;
        if (line.length + 2 > maxHeaders - headerBytes) return err("HTTP.request: headers exceed 64 KiB");
        headerBytes += line.length + 2;
        auto next = api.append(hs, line.toStringz);
        if (next is null) return err("HTTP.request: cannot allocate headers");
        hs = next;
    }
    Response response; response.interp = c.interp;
    bool configured = api.setopt(easy, 10002, url.toStringz) == 0 && api.setopt(easy, 10036, method.toStringz) == 0
        && api.setopt(easy, 155, cast(c_long) timeout) == 0 && api.setopt(easy, 156, cast(c_long) timeout) == 0
        && api.setopt(easy, 99, 1L) == 0 && api.setopt(easy, 52, 0L) == 0 && api.setopt(easy, 181, 3L) == 0
        && api.setopt(easy, 10177, "*".ptr) == 0 && api.setopt(easy, 20011, &bodyCallback) == 0
        && api.setopt(easy, 10001, &response) == 0 && api.setopt(easy, 20079, &headerCallback) == 0
        && api.setopt(easy, 10029, &response) == 0 && api.setopt(easy, 20219, &progressCallback) == 0
        && api.setopt(easy, 10057, &response) == 0 && api.setopt(easy, 43, 0L) == 0;
    if (hs !is null) configured = configured && api.setopt(easy, 10023, hs) == 0;
    auto bytes = body.toStringz;
    if (body.length || method == "POST" || method == "PUT" || method == "PATCH")
        configured = configured && api.setopt(easy, 30120, cast(long) body.length) == 0 && api.setopt(easy, 10015, bytes) == 0;
    if (method == "HEAD") configured = configured && api.setopt(easy, 44, 1L) == 0;
    if (!configured) return err("HTTP.request: cannot configure the client");
    int status = api.perform(easy);
    c.interp.check_cancelled(c.span);
    if (response.exceeded) return err("HTTP.request: response exceeds size limit");
    if (status) return err("HTTP.request: " ~ fromStringz(api.error(status)).idup);
    c_long code; api.getinfo(easy, 0x200002, &code);
    Value[] fields; foreach (h; response.headers) fields ~= pair(str(h[0]), str(h[1]));
    return ok(record([tuple("status", integer(code)), tuple("headers", list(fields)), tuple("body", str(response.body))]));
}
void installHTTP(Session s)
{
    if (s.find_primitive("HTTP.request") !is null) return;
    s.declare("type HttpResponse = { status : Int, headers : [(String, String)], body : String }\n", "<metalib HTTP>");
    prim(s, "HTTP.available", "Unit -> Bool", 1, (C c, V a) => boolean(api.initialized));
    prim(s, "HTTP.request", "String -> String -> [(String, String)] -> String -> Int -> Result HttpResponse String", 5,
        (C c, V a) => request(c, S(a[0]), S(a[1]), a[2], S(a[3]), a[4].i));
    prim(s, "HTTP.get", "String -> Result HttpResponse String", 1,
        (C c, V a) => request(c, "GET", S(a[0]), list([]), "", 10000));
    prim(s, "HTTP.post", "String -> String -> Result HttpResponse String", 2,
        (C c, V a) => request(c, "POST", S(a[0]), list([]), S(a[1]), 10000));
}
