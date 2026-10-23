/**
 * metalib/d/JSON.d — MetaML host module `JSON` (D dialect): JSON parsing and rendering.
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/JSON.h):
 *   type Json := | JNull | JBool Bool | JNum Float | JStr String | JArr [Json] | JObj [(String, Json)]
 *   JSON.parse, render, pretty, get, at, of_value
 */
module metalib.d.JSON;

import MetaML;
import metalib.d.Support;

import std.math : floor, fabs;
import std.conv : to;

void declare_json_type(Session s)
{
    if (s.checker().global.types.find("Json") !is null) return;
    s.declare("type Json := | JNull | JBool Bool | JNum Float | JStr String | JArr [Json] "
        ~ "| JObj [(String, Json)]\n", "<metalib JSON>");
}

Value jnull() { return make_ctor("JNull", 0); }
Value jbool(bool b) { return make_ctor("JBool", 1, [Value.boolean(b)]); }
Value jnum(double d) { return make_ctor("JNum", 2, [Value.floating(d)]); }
Value jstr(string s) { return make_ctor("JStr", 3, [make_str(s)]); }
Value jarr(Value[] xs) { return make_ctor("JArr", 4, [make_list(xs)]); }
Value jobj(Tuple!(string, Value)[] kv)
{
    Value[] items;
    foreach (p; kv) items ~= make_tuple([make_str(p[0]), p[1]]);
    return make_ctor("JObj", 5, [make_list(items)]);
}
string jtag(Value v) { return v.kind == VK.Ctor ? agg_of(v).tag : ""; }
Value payload(Value v) { return agg_of(v).items[0]; }
Tuple!(string, Value)[] members(Value obj)
{
    Tuple!(string, Value)[] result;
    foreach (p; list_items(payload(obj))) result ~= tuple(str_of(agg_of(p).items[0]), agg_of(p).items[1]);
    return result;
}

/// Integral values print without a fraction; NaN/inf are not JSON numbers.
string number_text(double d)
{
    if (d != d || d == double.infinity || d == -double.infinity) return "null";
    if (d == floor(d) && fabs(d) < 1e15) return to!string(cast(long) d);
    return format_float(d);
}

string jquote(string s)
{
    enum hex = "0123456789abcdef";
    string result = "\"";
    foreach (char ch; s)
    {
        ubyte c = cast(ubyte) ch;
        switch (c)
        {
            case '"': result ~= "\\\""; break;
            case '\\': result ~= "\\\\"; break;
            case '\b': result ~= "\\b"; break;
            case '\f': result ~= "\\f"; break;
            case '\n': result ~= "\\n"; break;
            case '\r': result ~= "\\r"; break;
            case '\t': result ~= "\\t"; break;
            default:
                if (c < 0x20)
                {
                    result ~= "\\u00";
                    result ~= hex[c >> 4];
                    result ~= hex[c & 15];
                }
                else result ~= ch;
        }
    }
    return result ~ "\"";
}

void jrender(Value v, bool pretty, int indent, ref string result)
{
    import std.array : replicate;
    string t = jtag(v);
    string pad = pretty ? replicate(" ", cast(size_t)(indent + 2)) : "";
    string close = pretty ? replicate(" ", cast(size_t) indent) : "";
    if (t == "JNull") result ~= "null";
    else if (t == "JBool") result ~= payload(v).i ? "true" : "false";
    else if (t == "JNum") result ~= number_text(payload(v).f);
    else if (t == "JStr") result ~= jquote(str_of(payload(v)));
    else if (t == "JArr")
    {
        auto xs = list_items(payload(v));
        if (!xs.length) { result ~= "[]"; return; }
        result ~= pretty ? "[\n" : "[";
        foreach (i, x; xs)
        {
            if (i) result ~= pretty ? ",\n" : ",";
            result ~= pad;
            jrender(x, pretty, indent + 2, result);
        }
        result ~= pretty ? "\n" ~ close ~ "]" : "]";
    }
    else if (t == "JObj")
    {
        auto kv = members(v);
        if (!kv.length) { result ~= "{}"; return; }
        result ~= pretty ? "{\n" : "{";
        foreach (i, p; kv)
        {
            if (i) result ~= pretty ? ",\n" : ",";
            result ~= pad ~ jquote(p[0]) ~ (pretty ? ": " : ":");
            jrender(p[1], pretty, indent + 2, result);
        }
        result ~= pretty ? "\n" ~ close ~ "}" : "}";
    }
    else result ~= "null";
}

class JsonFail : Exception
{
    this(string m) { super(m); }
}

private final class Reader
{
    string s_;
    size_t pos_;
    this(string s) { s_ = s; }

    Value document()
    {
        auto v = value(0);
        ws();
        if (pos_ < s_.length) fail("trailing characters");
        return v;
    }

    noreturn fail(string why) { throw new JsonFail("JSON: " ~ why ~ " at offset " ~ to!string(pos_)); }
    void ws()
    {
        while (pos_ < s_.length && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r')) ++pos_;
    }
    void literal(string word)
    {
        if (pos_ + word.length > s_.length || s_[pos_ .. pos_ + word.length] != word) fail("invalid literal");
        pos_ += word.length;
    }
    Value value(int depth)
    {
        if (depth > 512) fail("nesting too deep");
        ws();
        if (pos_ >= s_.length) fail("unexpected end of input");
        char c = s_[pos_];
        if (c == 'n') { literal("null"); return jnull(); }
        if (c == 't') { literal("true"); return jbool(true); }
        if (c == 'f') { literal("false"); return jbool(false); }
        if (c == '"') return jstr(str_());
        if (c == '[')
        {
            ++pos_;
            Value[] xs;
            ws();
            if (pos_ < s_.length && s_[pos_] == ']') { ++pos_; return jarr(xs); }
            while (true)
            {
                xs ~= value(depth + 1);
                ws();
                if (pos_ >= s_.length) fail("unexpected end of input");
                if (s_[pos_] == ',') { ++pos_; continue; }
                if (s_[pos_] == ']') { ++pos_; return jarr(xs); }
                fail("expected `,` or `]`");
            }
        }
        if (c == '{')
        {
            ++pos_;
            Tuple!(string, Value)[] kv;
            ws();
            if (pos_ < s_.length && s_[pos_] == '}') { ++pos_; return jobj(kv); }
            while (true)
            {
                ws();
                if (pos_ >= s_.length || s_[pos_] != '"') fail("expected a string key");
                string k = str_();
                ws();
                if (pos_ >= s_.length || s_[pos_] != ':') fail("expected `:`");
                ++pos_;
                Value v = value(depth + 1);
                kv ~= tuple(k, v);
                ws();
                if (pos_ >= s_.length) fail("unexpected end of input");
                if (s_[pos_] == ',') { ++pos_; continue; }
                if (s_[pos_] == '}') { ++pos_; return jobj(kv); }
                fail("expected `,` or `}`");
            }
        }
        if (c == '-' || (c >= '0' && c <= '9')) return number();
        fail("unexpected character `" ~ c ~ "`");
    }
    Value number()
    {
        import core.stdc.stdlib : strtod;
        size_t b = pos_;
        if (s_[pos_] == '-') ++pos_;
        bool digit() { return pos_ < s_.length && s_[pos_] >= '0' && s_[pos_] <= '9'; }
        if (!digit()) fail("invalid number");
        if (s_[pos_] == '0') ++pos_;
        else while (digit()) ++pos_;
        if (pos_ < s_.length && s_[pos_] == '.')
        {
            ++pos_;
            if (!digit()) fail("invalid number");
            while (digit()) ++pos_;
        }
        if (pos_ < s_.length && (s_[pos_] == 'e' || s_[pos_] == 'E'))
        {
            ++pos_;
            if (pos_ < s_.length && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
            if (!digit()) fail("invalid number");
            while (digit()) ++pos_;
        }
        string text = s_[b .. pos_] ~ "\0";
        return jnum(strtod(text.ptr, null));
    }
    int hex4()
    {
        if (pos_ + 4 > s_.length) fail("invalid escape");
        int v = 0;
        foreach (k; 0 .. 4)
        {
            char h = s_[pos_++];
            int d = (h >= '0' && h <= '9') ? h - '0' : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                : (h >= 'A' && h <= 'F') ? h - 'A' + 10 : -1;
            if (d < 0) fail("invalid escape");
            v = v * 16 + d;
        }
        return v;
    }
    string str_()
    {
        ++pos_;
        string result;
        while (true)
        {
            if (pos_ >= s_.length) fail("unterminated string");
            char c = s_[pos_++];
            if (c == '"') return result;
            if (cast(ubyte) c < 0x20) fail("control character in string");
            if (c != '\\') { result ~= c; continue; }
            if (pos_ >= s_.length) fail("unterminated string");
            char e = s_[pos_++];
            switch (e)
            {
                case '"': result ~= '"'; break;
                case '\\': result ~= '\\'; break;
                case '/': result ~= '/'; break;
                case 'b': result ~= '\b'; break;
                case 'f': result ~= '\f'; break;
                case 'n': result ~= '\n'; break;
                case 'r': result ~= '\r'; break;
                case 't': result ~= '\t'; break;
                case 'u':
                {
                    int cp = hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF && pos_ + 1 < s_.length && s_[pos_] == '\\' && s_[pos_ + 1] == 'u')
                    {
                        pos_ += 2;
                        int lo = hex4();
                        if (lo >= 0xDC00 && lo <= 0xDFFF) cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        else cp = 0xFFFD;
                    }
                    if (!append_utf8(result, cast(uint) cp)) append_utf8(result, 0xFFFD);
                    break;
                }
                default:
                    fail("invalid escape");
            }
        }
    }
}

Value json_of_value(Value v)
{
    switch (v.kind)
    {
        case VK.Unit: return jnull();
        case VK.Bool: return jbool(v.i != 0);
        case VK.Int: return jnum(cast(double) v.i);
        case VK.Float: return jnum(v.f);
        case VK.Str: return jstr(str_of(v));
        case VK.List:
        {
            Value[] xs;
            foreach (x; list_items(v)) xs ~= json_of_value(x);
            return jarr(xs);
        }
        case VK.Tuple:
        {
            Value[] xs;
            foreach (x; agg_of(v).items) xs ~= json_of_value(x);
            return jarr(xs);
        }
        case VK.Record:
        {
            Tuple!(string, Value)[] kv;
            auto a = agg_of(v);
            foreach (i, x; a.items) kv ~= tuple(a.labels[i], json_of_value(x));
            return jobj(kv);
        }
        case VK.Ctor:
        {
            string t = jtag(v);
            if (t == "JNull" || t == "JBool" || t == "JNum" || t == "JStr" || t == "JArr" || t == "JObj") return v;
            auto a = agg_of(v);
            if (t == "None") return jnull();
            if (t == "Some" && a.items.length == 1) return json_of_value(a.items[0]);
            Value[] args;
            foreach (x; a.items) args ~= json_of_value(x);
            return jobj([tuple("tag", jstr(t)), tuple("args", jarr(args))]);
        }
        default:
            return jstr(show_value(v));
    }
}

void installJSON(Session s)
{
    declare_json_type(s);
    prim(s, "JSON.parse", "String -> Result Json String", 1, delegate Value(C c, V a) {
        try return ok((new Reader(S(a[0]))).document());
        catch (JsonFail f) return err(f.msg);
    });
    prim(s, "JSON.render", "Json -> String", 1, delegate Value(C c, V a) {
        string result;
        jrender(a[0], false, 0, result);
        return str(result);
    });
    prim(s, "JSON.pretty", "Json -> String", 1, delegate Value(C c, V a) {
        string result;
        jrender(a[0], true, 0, result);
        return str(result);
    });
    prim(s, "JSON.get", "String -> Json -> Option Json", 2, delegate Value(C c, V a) {
        if (jtag(a[1]) != "JObj") return none();
        foreach (kv; members(a[1])) if (kv[0] == S(a[0])) return some(kv[1]);
        return none();
    });
    prim(s, "JSON.at", "Int -> Json -> Option Json", 2, delegate Value(C c, V a) {
        if (jtag(a[1]) != "JArr") return none();
        auto xs = items(payload(a[1]));
        if (a[0].i < 0 || a[0].i >= cast(long) xs.length) return none();
        return some(xs[cast(size_t) a[0].i]);
    });
    prim(s, "JSON.of_value", "'a -> Json", 1, (C c, V a) => json_of_value(a[0]));
}

