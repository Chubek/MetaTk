/**
 * metalib/d/URI.d — MetaML host module `URI` (D dialect): RFC 3986 URI parsing, rendering and percent-encoding.
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/URI.h):
 *   type Uri = { scheme, user, host, port, path, query, fragment }
 *   URI.parse, render, encode, decode, query_params
 */
module metalib.d.URI;

import MetaML;
import metalib.d.Support;


import std.string : indexOf, lastIndexOf;

private bool unreserved(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-'
        || c == '.' || c == '_' || c == '~';
}

private string encode(string s)
{
    enum d = "0123456789ABCDEF";
    string result;
    foreach (char ch; s)
    {
        ubyte c = cast(ubyte) ch;
        if (unreserved(ch)) result ~= ch;
        else
        {
            result ~= '%';
            result ~= d[c >> 4];
            result ~= d[c & 15];
        }
    }
    return result;
}

private Nullable!string decode(string s, bool plus_space)
{
    static int hexv(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }
    char[] result;
    for (size_t i = 0; i < s.length; ++i)
    {
        if (s[i] == '%')
        {
            if (i + 2 >= s.length) return Nullable!string.init;
            int h = hexv(s[i + 1]), l = hexv(s[i + 2]);
            if (h < 0 || l < 0) return Nullable!string.init;
            result ~= cast(char)(h * 16 + l);
            i += 2;
        }
        else if (plus_space && s[i] == '+') result ~= ' ';
        else result ~= s[i];
    }
    return nullable(result.idup);
}

void installURI(Session s)
{
    s.declare("type Uri = { scheme : String, user : String, host : String, port : Int, "
        ~ "path : String, query : String, fragment : String }\n", "<metalib URI>");
    prim(s, "URI.parse", "String -> Option Uri", 1, delegate Value(C c, V a) {
        string u = S(a[0]);
        string scheme, user, host, path, query, fragment;
        long port = -1;
        auto hash = u.indexOf('#');
        if (hash >= 0)
        {
            fragment = u[hash + 1 .. $];
            u = u[0 .. hash];
        }
        auto q = u.indexOf('?');
        if (q >= 0)
        {
            query = u[q + 1 .. $];
            u = u[0 .. q];
        }
        auto colon = u.indexOf(':');
        bool has_scheme = colon > 0;
        for (size_t i = 0; has_scheme && i < cast(size_t) colon; ++i)
        {
            char ch = u[i];
            bool okc = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
                || (i > 0 && ((ch >= '0' && ch <= '9') || ch == '+' || ch == '-' || ch == '.'));
            if (!okc) has_scheme = false;
        }
        if (has_scheme)
        {
            scheme = ascii_map(u[0 .. colon], false);
            u = u[colon + 1 .. $];
        }
        if (u.length >= 2 && u[0] == '/' && u[1] == '/')
        {
            u = u[2 .. $];
            auto slash = u.indexOf('/');
            string auth = slash < 0 ? u : u[0 .. slash];
            path = slash < 0 ? "" : u[slash .. $];
            auto at = auth.lastIndexOf('@');
            if (at >= 0)
            {
                user = auth[0 .. at];
                auth = auth[at + 1 .. $];
            }
            long pc = -1;
            if (auth.length && auth[0] == '[')
            {
                auto rb = auth.indexOf(']');
                if (rb < 0) return none();
                if (rb + 1 < auth.length && auth[rb + 1] == ':') pc = rb + 1;
            }
            else pc = auth.lastIndexOf(':');
            if (pc >= 0)
            {
                auto ps = auth[pc + 1 .. $];
                if (ps.length)
                {
                    foreach (ch; ps) if (ch < '0' || ch > '9') return none();
                    auto pv = parse_int(ps);
                    if (pv.isNull || pv.get > 65535) return none();
                    port = pv.get;
                }
                auth = auth[0 .. pc];
            }
            host = ascii_map(auth, false);
        }
        else path = u;
        if (!has_scheme && !host.length && !path.length && !query.length && !fragment.length) return none();
        return some(record([tuple("scheme", str(scheme)), tuple("user", str(user)), tuple("host", str(host)),
            tuple("port", integer(port)), tuple("path", str(path)), tuple("query", str(query)),
            tuple("fragment", str(fragment))]));
    });
    prim(s, "URI.render", "Uri -> String", 1, delegate Value(C c, V a) {
        Value r = a[0];
        string result;
        import std.conv : to;
        if (S(field(r, "scheme")).length) result ~= S(field(r, "scheme")) ~ ":";
        if (S(field(r, "host")).length || S(field(r, "user")).length)
        {
            result ~= "//";
            if (S(field(r, "user")).length) result ~= S(field(r, "user")) ~ "@";
            result ~= S(field(r, "host"));
            if (field(r, "port").i >= 0) result ~= ":" ~ to!string(field(r, "port").i);
        }
        result ~= S(field(r, "path"));
        if (S(field(r, "query")).length) result ~= "?" ~ S(field(r, "query"));
        if (S(field(r, "fragment")).length) result ~= "#" ~ S(field(r, "fragment"));
        return str(result);
    });
    prim(s, "URI.encode", "String -> String", 1, (C c, V a) => str(encode(S(a[0]))));
    prim(s, "URI.decode", "String -> Option String", 1, delegate Value(C c, V a) {
        auto r = decode(S(a[0]), false);
        return r.isNull ? none() : some(str(r.get));
    });
    prim(s, "URI.query_params", "String -> [(String, String)]", 1, delegate Value(C c, V a) {
        Value[] result;
        foreach (part; split_string("&", S(a[0])))
        {
            if (!part.length) continue;
            auto eq = part.indexOf('=');
            string k = eq < 0 ? part : part[0 .. eq];
            string v = eq < 0 ? "" : part[eq + 1 .. $];
            auto dk = decode(k, true), dv = decode(v, true);
            result ~= pair(str(dk.isNull ? k : dk.get), str(dv.isNull ? v : dv.get));
        }
        return list(result);
    });
}

