/**
 * metalib/d/String.d — MetaML host module `String` (D dialect): Unicode-aware and formatting string utilities.
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/String.h):
 *   String.utf8_length, utf8_chars, utf8_valid, codepoints, of_codepoints,
 *   String.pad_left, pad_right, words, last_index_of, count, strip_prefix,
 *   String.strip_suffix, capitalize, is_digit, is_alpha, is_space, is_alnum,
 *   String.format, escape, compare_ci   (see metalib/cpp/String.h for types)
 */
module metalib.d.String;

import MetaML;
import metalib.d.Support;


import std.string : lastIndexOf, indexOf;

/// Decodes one UTF-8 sequence at i; returns (code point, length) or (-1, 1).
private Tuple!(long, size_t) decode(string s, size_t i)
{
    ubyte c = cast(ubyte) s[i];
    size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
    if (n == 0 || i + n > s.length) return tuple(-1L, cast(size_t) 1);
    if (n == 1) return tuple(cast(long) c, cast(size_t) 1);
    long cp = c & (0x7F >> n);
    foreach (k; 1 .. n)
    {
        ubyte d = cast(ubyte) s[i + k];
        if ((d & 0xC0) != 0x80) return tuple(-1L, cast(size_t) 1);
        cp = (cp << 6) | (d & 0x3F);
    }
    static immutable long[5] mins = [0, 0, 0x80, 0x800, 0x10000];
    if (cp < mins[n] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return tuple(-1L, cast(size_t) 1);
    return tuple(cp, n);
}

private bool all_of(string s, bool function(char) pred)
{
    if (!s.length) return false;
    foreach (char c; s) if (!pred(c)) return false;
    return true;
}

private string padding(size_t n, string fill)
{
    string result;
    if (!fill.length) return result;
    while (result.length < n) result ~= fill[result.length % fill.length];
    return result;
}

private PrimFn classPrim(bool function(char) pred)
{
    return (C c, V a) => boolean(all_of(S(a[0]), pred));
}

void installString(Session s)
{
    prim(s, "String.utf8_length", "String -> Int", 1, delegate Value(C c, V a) {
        string x = S(a[0]);
        long n = 0;
        for (size_t i = 0; i < x.length; i += decode(x, i)[1]) ++n;
        return integer(n);
    });
    prim(s, "String.utf8_chars", "String -> [String]", 1, delegate Value(C c, V a) {
        string x = S(a[0]);
        Value[] result;
        for (size_t i = 0; i < x.length;)
        {
            auto n = decode(x, i)[1];
            result ~= str(x[i .. i + n]);
            i += n;
        }
        return list(result);
    });
    prim(s, "String.utf8_valid", "String -> Bool", 1, delegate Value(C c, V a) {
        string x = S(a[0]);
        for (size_t i = 0; i < x.length;)
        {
            auto d = decode(x, i);
            if (d[0] < 0) return boolean(false);
            i += d[1];
        }
        return boolean(true);
    });
    prim(s, "String.codepoints", "String -> [Int]", 1, delegate Value(C c, V a) {
        string x = S(a[0]);
        Value[] result;
        for (size_t i = 0; i < x.length;)
        {
            auto d = decode(x, i);
            result ~= integer(d[0] < 0 ? 0xFFFD : d[0]);
            i += d[1];
        }
        return list(result);
    });
    prim(s, "String.of_codepoints", "[Int] -> String", 1, delegate Value(C c, V a) {
        string result;
        foreach (v; items(a[0]))
        {
            long cp = v.i;
            if (cp < 0 || cp > 0x10FFFF || !append_utf8(result, cast(uint) cp)) append_utf8(result, 0xFFFD);
        }
        return str(result);
    });
    prim(s, "String.pad_left", "Int -> String -> String -> String", 3, delegate Value(C c, V a) {
        string x = S(a[2]);
        size_t w = a[0].i < 0 ? 0 : cast(size_t) a[0].i;
        if (x.length >= w) return str(x);
        return str(padding(w - x.length, S(a[1])) ~ x);
    });
    prim(s, "String.pad_right", "Int -> String -> String -> String", 3, delegate Value(C c, V a) {
        string x = S(a[2]);
        size_t w = a[0].i < 0 ? 0 : cast(size_t) a[0].i;
        if (x.length >= w) return str(x);
        return str(x ~ padding(w - x.length, S(a[1])));
    });
    prim(s, "String.words", "String -> [String]", 1, delegate Value(C c, V a) {
        Value[] result;
        string cur;
        foreach (char ch; S(a[0]))
        {
            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
            {
                if (cur.length) result ~= str(cur);
                cur = null;
            }
            else cur ~= ch;
        }
        if (cur.length) result ~= str(cur);
        return list(result);
    });
    prim(s, "String.last_index_of", "String -> String -> Option Int", 2, delegate Value(C c, V a) {
        string n = S(a[0]), x = S(a[1]);
        if (!n.length) return some(integer(cast(long) x.length));
        auto p = x.lastIndexOf(n);
        if (p < 0) return none();
        return some(integer(cast(long) p));
    });
    prim(s, "String.count", "String -> String -> Int", 2, delegate Value(C c, V a) {
        string n = S(a[0]), x = S(a[1]);
        if (!n.length) return integer(0);
        long cnt = 0;
        size_t start = 0;
        while (true)
        {
            auto p = x[start .. $].indexOf(n);
            if (p < 0) break;
            ++cnt;
            start += p + n.length;
        }
        return integer(cnt);
    });
    prim(s, "String.strip_prefix", "String -> String -> Option String", 2, delegate Value(C c, V a) {
        string p = S(a[0]), x = S(a[1]);
        if (x.length >= p.length && x[0 .. p.length] == p) return some(str(x[p.length .. $]));
        return none();
    });
    prim(s, "String.strip_suffix", "String -> String -> Option String", 2, delegate Value(C c, V a) {
        string p = S(a[0]), x = S(a[1]);
        if (x.length >= p.length && x[$ - p.length .. $] == p) return some(str(x[0 .. $ - p.length]));
        return none();
    });
    prim(s, "String.capitalize", "String -> String", 1, delegate Value(C c, V a) {
        char[] x = S(a[0]).dup;
        if (x.length && x[0] >= 'a' && x[0] <= 'z') x[0] = cast(char)(x[0] - 32);
        return str(x.idup);
    });
    static bool isDigitC(char c) { return c >= '0' && c <= '9'; }
    static bool isAlphaC(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
    static bool isSpaceC(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
    static bool isAlnumC(char c) { return isAlphaC(c) || isDigitC(c); }
    prim(s, "String.is_digit", "String -> Bool", 1, classPrim(&isDigitC));
    prim(s, "String.is_alpha", "String -> Bool", 1, classPrim(&isAlphaC));
    prim(s, "String.is_space", "String -> Bool", 1, classPrim(&isSpaceC));
    prim(s, "String.is_alnum", "String -> Bool", 1, classPrim(&isAlnumC));
    prim(s, "String.format", "String -> [String] -> String", 2, delegate Value(C c, V a) {
        string f = S(a[0]);
        auto args = items(a[1]);
        string result;
        size_t next = 0;
        for (size_t i = 0; i < f.length; ++i)
        {
            if (f[i] == '{' && i + 1 < f.length && f[i + 1] == '{') { result ~= '{'; ++i; }
            else if (f[i] == '}' && i + 1 < f.length && f[i + 1] == '}') { result ~= '}'; ++i; }
            else if (f[i] == '{' && i + 1 < f.length && f[i + 1] == '}')
            {
                if (next < args.length) result ~= S(args[next]);
                ++next;
                ++i;
            }
            else result ~= f[i];
        }
        return str(result);
    });
    prim(s, "String.escape", "String -> String", 1, delegate Value(C c, V a) {
        auto q = quote_string(S(a[0]));
        return str(q[1 .. $ - 1]);
    });
    prim(s, "String.compare_ci", "String -> String -> Int", 2, delegate Value(C c, V a) {
        auto x = ascii_map(S(a[0]), false);
        auto y = ascii_map(S(a[1]), false);
        return integer(x < y ? -1 : x > y ? 1 : 0);
    });
}

