/**
 * metalib/d/YAML.d — MetaML host module `YAML` (D dialect): YAML parsing and rendering (values map onto Json).
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/YAML.h):
 *   YAML.parse, render
 */
module metalib.d.YAML;

import MetaML;
import metalib.d.Support;

import metalib.d.JSON;
import std.conv : to;
import std.string : indexOf;

class YamlFail : Exception
{
    this(string m) { super(m); }
}

private struct Line
{
    int indent;
    string text;
    size_t no;
    bool content;
}

private string rtrim(string s)
{
    while (s.length && (s[$ - 1] == ' ' || s[$ - 1] == '\t' || s[$ - 1] == '\r')) s = s[0 .. $ - 1];
    return s;
}
private string trim(string s)
{
    size_t b = 0;
    while (b < s.length && (s[b] == ' ' || s[b] == '\t')) ++b;
    return rtrim(s[b .. $]);
}
private bool startsWithS(string s, string p) { return s.length >= p.length && s[0 .. p.length] == p; }

/// Removes a trailing ` # comment` outside quotes.
private string strip_comment(string s)
{
    char q = 0;
    for (size_t i = 0; i < s.length; ++i)
    {
        char c = s[i];
        if (q)
        {
            if (q == '"' && c == '\\') ++i;
            else if (c == q) q = 0;
            continue;
        }
        if (c == '"' || c == '\'')
        {
            if (i == 0 || s[i - 1] == ' ' || s[i - 1] == '\t' || s[i - 1] == '[' || s[i - 1] == '{'
                || s[i - 1] == ',' || s[i - 1] == ':' || s[i - 1] == '-')
                q = c;
            continue;
        }
        if (c == '#' && (i == 0 || s[i - 1] == ' ' || s[i - 1] == '\t')) return rtrim(s[0 .. i]);
    }
    return rtrim(s);
}

private final class Parser
{
    Line[] lines_;
    string[] raw_;

    this(string src)
    {
        size_t start = 0, no = 1;
        while (start <= src.length)
        {
            auto rel = src[start .. $].indexOf('\n');
            string raw = rel < 0 ? src[start .. $] : src[start .. start + rel];
            if (raw.length && raw[$ - 1] == '\r') raw = raw[0 .. $ - 1];
            raw_ ~= raw;
            Line l;
            l.no = no;
            size_t k = 0;
            while (k < raw.length && raw[k] == ' ') ++k;
            if (k < raw.length && raw[k] == '\t')
            {
                lines_ ~= l;
                fail_line(no, "tabs are not allowed for indentation");
            }
            l.indent = cast(int) k;
            l.text = strip_comment(raw[k .. $]);
            l.content = l.text.length && l.text != "---" && l.text != "...";
            if (startsWithS(l.text, "--- "))
            {
                l.text = trim(l.text[4 .. $]);
                l.content = l.text.length > 0;
            }
            if (l.text.length && l.text[0] == '%') l.content = false;
            lines_ ~= l;
            if (rel < 0) break;
            start = start + rel + 1;
            ++no;
        }
    }

    Value document()
    {
        size_t i = next_content(0);
        if (i >= lines_.length) return jnull();
        Value v = block(i, lines_[i].indent);
        i = next_content(i);
        if (i < lines_.length) fail_line(lines_[i].no, "unexpected content (bad indentation?)");
        return v;
    }

private:
    noreturn fail_line(size_t no, string why) { throw new YamlFail("YAML: " ~ why ~ " at line " ~ to!string(no)); }
    noreturn fail(size_t i, string why) { fail_line(i < lines_.length ? lines_[i].no : lines_.length, why); }
    size_t next_content(size_t i)
    {
        while (i < lines_.length && !lines_[i].content) ++i;
        return i;
    }
    static bool is_seq(string t) { return t == "-" || startsWithS(t, "- "); }
    static ptrdiff_t entry_colon(string t)
    {
        if (!t.length || t[0] == '[' || t[0] == '{' || is_seq(t)) return -1;
        size_t i = 0;
        if (t[0] == '"' || t[0] == '\'')
        {
            char q = t[0];
            for (i = 1; i < t.length; ++i)
            {
                if (q == '"' && t[i] == '\\') { ++i; continue; }
                if (t[i] == q)
                {
                    if (q == '\'' && i + 1 < t.length && t[i + 1] == '\'') { ++i; continue; }
                    break;
                }
            }
            ++i;
            while (i < t.length && t[i] == ' ') ++i;
            if (i < t.length && t[i] == ':' && (i + 1 == t.length || t[i + 1] == ' ')) return cast(ptrdiff_t) i;
            return -1;
        }
        for (; i < t.length; ++i)
            if (t[i] == ':' && (i + 1 == t.length || t[i + 1] == ' ')) return cast(ptrdiff_t) i;
        return -1;
    }
    Value block(ref size_t i, int indent)
    {
        string t = lines_[i].text;
        if (is_seq(t)) return sequence(i, indent);
        if (entry_colon(t) >= 0) return mapping(i, indent);
        return plain_lines(i, indent);
    }
    Value sequence(ref size_t i, int indent)
    {
        Value[] xs;
        while (i < lines_.length && lines_[i].content && lines_[i].indent == indent && is_seq(lines_[i].text))
        {
            string rest = lines_[i].text == "-" ? "" : lines_[i].text[2 .. $];
            size_t lead = 0;
            while (lead < rest.length && rest[lead] == ' ') ++lead;
            rest = rest[lead .. $];
            if (!rest.length)
            {
                size_t j = next_content(i + 1);
                if (j < lines_.length && lines_[j].indent > indent)
                {
                    i = j;
                    xs ~= block(i, lines_[j].indent);
                }
                else
                {
                    xs ~= jnull();
                    i = j;
                }
            }
            else
            {
                lines_[i].indent = indent + 2 + cast(int) lead;
                lines_[i].text = rest;
                xs ~= block(i, lines_[i].indent);
            }
            i = next_content(i);
        }
        return jarr(xs);
    }
    Value mapping(ref size_t i, int indent)
    {
        Tuple!(string, Value)[] kv;
        while (i < lines_.length && lines_[i].content && lines_[i].indent == indent)
        {
            string t = lines_[i].text;
            auto c = entry_colon(t);
            if (c < 0) fail(i, "expected a mapping entry `key: value`");
            string key = scalar_key(trim(t[0 .. c]), i);
            string rest = trim(t[c + 1 .. $]);
            Value v;
            if (!rest.length)
            {
                size_t j = next_content(i + 1);
                if (j < lines_.length && lines_[j].indent > indent)
                {
                    i = j;
                    v = block(i, lines_[j].indent);
                }
                else if (j < lines_.length && lines_[j].indent == indent && is_seq(lines_[j].text))
                {
                    i = j;
                    v = sequence(i, indent);
                }
                else
                {
                    v = jnull();
                    i = j;
                }
            }
            else if (rest[0] == '|' || rest[0] == '>') v = block_scalar(i, indent, rest);
            else v = inline_value(i, indent, rest);
            bool replaced = false;
            foreach (ref p; kv) if (p[0] == key) { p[1] = v; replaced = true; }
            if (!replaced) kv ~= tuple(key, v);
            i = next_content(i);
        }
        return jobj(kv);
    }
    string scalar_key(string k, size_t i)
    {
        if (k.length && (k[0] == '"' || k[0] == '\'')) return str_of(payload(quoted(k, i)));
        return k;
    }
    Value inline_value(ref size_t i, int indent, string text)
    {
        if (text[0] == '[' || text[0] == '{')
        {
            while (!balanced(text))
            {
                size_t j = next_content(i + 1);
                if (j >= lines_.length) fail(i, "unterminated flow collection");
                i = j;
                text ~= " " ~ lines_[j].text;
            }
            size_t p = 0;
            Value v = flow(text, p, i);
            while (p < text.length && text[p] == ' ') ++p;
            if (p != text.length) fail(i, "unexpected text after a flow collection");
            i = i + 1;
            return v;
        }
        if (text[0] == '"' || text[0] == '\'')
        {
            Value v = quoted(text, i);
            i = i + 1;
            return v;
        }
        string acc = text;
        size_t j = next_content(i + 1);
        while (j < lines_.length && lines_[j].indent > indent && !is_seq(lines_[j].text)
            && entry_colon(lines_[j].text) < 0)
        {
            acc ~= " " ~ lines_[j].text;
            i = j;
            j = next_content(i + 1);
        }
        i = i + 1;
        return plain(acc, i - 1);
    }
    Value plain_lines(ref size_t i, int indent)
    {
        string text = lines_[i].text;
        if (text[0] == '|' || text[0] == '>') return block_scalar(i, indent - 1, text);
        return inline_value(i, indent - 1, text);
    }
    static bool balanced(string t)
    {
        int d = 0;
        char q = 0;
        for (size_t i = 0; i < t.length; ++i)
        {
            char c = t[i];
            if (q)
            {
                if (q == '"' && c == '\\') ++i;
                else if (c == q) q = 0;
                continue;
            }
            if (c == '"' || c == '\'') q = c;
            else if (c == '[' || c == '{') ++d;
            else if (c == ']' || c == '}') --d;
        }
        return d <= 0;
    }
    Value quoted(string t, size_t i)
    {
        char q = t[0];
        string result;
        size_t k = 1;
        for (; k < t.length; ++k)
        {
            char c = t[k];
            if (q == '\'' && c == '\'')
            {
                if (k + 1 < t.length && t[k + 1] == '\'')
                {
                    result ~= '\'';
                    ++k;
                    continue;
                }
                break;
            }
            if (q == '"' && c == '"') break;
            if (q == '"' && c == '\\' && k + 1 < t.length)
            {
                char e = t[++k];
                switch (e)
                {
                    case 'n': result ~= '\n'; break;
                    case 't': result ~= '\t'; break;
                    case 'r': result ~= '\r'; break;
                    case '0': result ~= '\0'; break;
                    case '"': result ~= '"'; break;
                    case '\\': result ~= '\\'; break;
                    case '/': result ~= '/'; break;
                    case ' ': result ~= ' '; break;
                    case 'x': case 'u':
                    {
                        int n = e == 'x' ? 2 : 4;
                        if (k + cast(size_t) n >= t.length) fail(i, "invalid escape");
                        uint cp = 0;
                        foreach (m; 0 .. n)
                        {
                            char h = t[++k];
                            int dv = (h >= '0' && h <= '9') ? h - '0' : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                                : (h >= 'A' && h <= 'F') ? h - 'A' + 10 : -1;
                            if (dv < 0) fail(i, "invalid escape");
                            cp = cp * 16 + cast(uint) dv;
                        }
                        if (!append_utf8(result, cp)) append_utf8(result, 0xFFFD);
                        break;
                    }
                    default:
                        fail(i, "invalid escape `\\" ~ e ~ "`");
                }
                continue;
            }
            result ~= c;
        }
        if (k >= t.length) fail(i, "unterminated quoted scalar");
        if (trim(t[k + 1 .. $]) != "") fail(i, "unexpected text after a quoted scalar");
        return jstr(result);
    }
    static bool is_int(string s)
    {
        size_t k = 0;
        if (k < s.length && (s[k] == '-' || s[k] == '+')) ++k;
        if (k >= s.length) return false;
        for (; k < s.length; ++k) if (s[k] < '0' || s[k] > '9') return false;
        return true;
    }
    static bool is_float(string s)
    {
        size_t k = 0;
        if (k < s.length && (s[k] == '-' || s[k] == '+')) ++k;
        bool digits = false, dot = false;
        while (k < s.length && ((s[k] >= '0' && s[k] <= '9') || s[k] == '.'))
        {
            if (s[k] == '.')
            {
                if (dot) return false;
                dot = true;
            }
            else digits = true;
            ++k;
        }
        if (!digits) return false;
        bool exp = false;
        if (k < s.length && (s[k] == 'e' || s[k] == 'E'))
        {
            exp = true;
            ++k;
            if (k < s.length && (s[k] == '-' || s[k] == '+')) ++k;
            if (k >= s.length) return false;
            for (; k < s.length; ++k) if (s[k] < '0' || s[k] > '9') return false;
        }
        return k == s.length && (dot || exp);
    }
    Value plain(string t0, size_t i)
    {
        string t = trim(t0);
        if (t.length && (t[0] == '&' || t[0] == '*' || t[0] == '!')) fail(i, "anchors, aliases and tags are not supported");
        if (!t.length || t == "~" || t == "null" || t == "Null" || t == "NULL") return jnull();
        if (t == "true" || t == "True" || t == "TRUE") return jbool(true);
        if (t == "false" || t == "False" || t == "FALSE") return jbool(false);
        if (t == ".inf" || t == "+.inf" || t == ".Inf") return jnum(double.infinity);
        if (t == "-.inf" || t == "-.Inf") return jnum(-double.infinity);
        if (t == ".nan" || t == ".NaN") return jnum(double.nan);
        if (is_int(t))
        {
            auto v = parse_int(t);
            if (!v.isNull) return jnum(cast(double) v.get);
        }
        if (is_float(t))
        {
            auto f = parse_float(t);
            if (!f.isNull) return jnum(f.get);
        }
        return jstr(t);
    }
    Value flow(string t, ref size_t p, size_t i)
    {
        void ws() { while (p < t.length && t[p] == ' ') ++p; }
        ws();
        if (p >= t.length) fail(i, "unterminated flow collection");
        if (t[p] == '[')
        {
            ++p;
            Value[] xs;
            ws();
            if (p < t.length && t[p] == ']') { ++p; return jarr(xs); }
            while (true)
            {
                xs ~= flow(t, p, i);
                ws();
                if (p < t.length && t[p] == ',')
                {
                    ++p;
                    ws();
                    if (p < t.length && t[p] == ']') { ++p; return jarr(xs); }
                    continue;
                }
                if (p < t.length && t[p] == ']') { ++p; return jarr(xs); }
                fail(i, "expected `,` or `]`");
            }
        }
        if (t[p] == '{')
        {
            ++p;
            Tuple!(string, Value)[] kv;
            ws();
            if (p < t.length && t[p] == '}') { ++p; return jobj(kv); }
            while (true)
            {
                ws();
                Value k = flow_scalar(t, p, i, true);
                string key = str_of(payload(k));
                if (jtag(k) != "JStr")
                {
                    string tmp;
                    jrender(k, false, 0, tmp);
                    key = tmp;
                }
                ws();
                Value v = jnull();
                if (p < t.length && t[p] == ':')
                {
                    ++p;
                    ws();
                    if (p < t.length && t[p] != ',' && t[p] != '}') v = flow(t, p, i);
                }
                kv ~= tuple(key, v);
                ws();
                if (p < t.length && t[p] == ',')
                {
                    ++p;
                    ws();
                    if (p < t.length && t[p] == '}') { ++p; return jobj(kv); }
                    continue;
                }
                if (p < t.length && t[p] == '}') { ++p; return jobj(kv); }
                fail(i, "expected `,` or `}`");
            }
        }
        return flow_scalar(t, p, i, false);
    }
    Value flow_scalar(string t, ref size_t p, size_t i, bool key)
    {
        if (p < t.length && (t[p] == '"' || t[p] == '\''))
        {
            char q = t[p];
            size_t b = p++;
            while (p < t.length)
            {
                if (q == '"' && t[p] == '\\') { p += 2; continue; }
                if (t[p] == q)
                {
                    if (q == '\'' && p + 1 < t.length && t[p + 1] == '\'') { p += 2; continue; }
                    break;
                }
                ++p;
            }
            if (p >= t.length) fail(i, "unterminated quoted scalar");
            ++p;
            return quoted(t[b .. p], i);
        }
        size_t b = p;
        while (p < t.length && t[p] != ',' && t[p] != ']' && t[p] != '}'
            && !(key && t[p] == ':' && (p + 1 == t.length || t[p + 1] == ' ')))
            ++p;
        return plain(t[b .. p], i);
    }
    Value block_scalar(ref size_t i, int indent, string header)
    {
        bool folded = header[0] == '>';
        char chomp = 0;
        foreach (k; 1 .. header.length)
        {
            if (header[k] == '-' || header[k] == '+') chomp = header[k];
            else if (header[k] != ' ' && !(header[k] >= '1' && header[k] <= '9')) fail(i, "invalid block scalar header");
        }
        string[] body;
        size_t j = i + 1;
        int bi = -1;
        for (; j < raw_.length; ++j)
        {
            string r = raw_[j];
            size_t k = 0;
            while (k < r.length && r[k] == ' ') ++k;
            bool blank = trim(r).length == 0;
            if (!blank && cast(int) k <= indent) break;
            if (!blank && bi < 0) bi = cast(int) k;
            if (blank) body ~= "";
            else
            {
                if (cast(int) k < bi) break;
                body ~= r[cast(size_t) bi .. $];
            }
        }
        size_t trailing = 0;
        while (body.length && !body[$ - 1].length)
        {
            body = body[0 .. $ - 1];
            ++trailing;
        }
        string result;
        if (folded)
        {
            foreach (k; 0 .. body.length)
            {
                if (k > 0) result ~= (!body[k].length || !body[k - 1].length) ? "\n" : " ";
                result ~= body[k];
            }
        }
        else foreach (k; 0 .. body.length) result ~= (k ? "\n" : "") ~ body[k];
        if (chomp != '-' && body.length) result ~= "\n";
        if (chomp == '+') foreach (k; 0 .. trailing) result ~= "\n";
        i = j;
        return jstr(result);
    }
}

private bool plain_safe(string s)
{
    import core.stdc.string : strchr;
    if (!s.length || s != trim(s)) return false;
    enum specials = "-?:,[]{}#&*!|>'\"%@`";
    foreach (ch; specials) if (s[0] == ch) return false;
    if (s.indexOf(": ") >= 0 || s.indexOf(" #") >= 0 || s[$ - 1] == ':') return false;
    foreach (char c; s) if (cast(ubyte) c < 0x20) return false;
    static immutable words = ["~", "null", "Null", "NULL", "true", "True", "TRUE", "false", "False", "FALSE",
        ".inf", "-.inf", ".nan"];
    foreach (w; words) if (s == w) return false;
    size_t k = 0;
    if (s[k] == '-' || s[k] == '+') ++k;
    bool numeric = k < s.length;
    for (; k < s.length; ++k)
        if (!((s[k] >= '0' && s[k] <= '9') || s[k] == '.' || s[k] == 'e' || s[k] == 'E' || s[k] == '-' || s[k] == '+'))
            numeric = false;
    import std.algorithm.searching : any;
    return !(numeric && s.any!(c => c >= '0' && c <= '9'));
}
private string scalar_text(Value v)
{
    string t = jtag(v);
    if (t == "JNull") return "null";
    if (t == "JBool") return payload(v).i ? "true" : "false";
    if (t == "JNum")
    {
        double d = payload(v).f;
        if (d != d) return ".nan";
        if (d == double.infinity) return ".inf";
        if (d == -double.infinity) return "-.inf";
        return number_text(d);
    }
    string s = str_of(payload(v));
    return plain_safe(s) ? s : jquote(s);
}
private bool is_collection(Value v)
{
    string t = jtag(v);
    if (t == "JArr") return list_items(payload(v)).length > 0;
    if (t == "JObj") return members(v).length > 0;
    return false;
}
private string key_text(string k) { return plain_safe(k) ? k : jquote(k); }
private void emit(Value v, int indent, ref string result)
{
    import std.array : replicate;
    string pad = replicate(" ", cast(size_t) indent);
    string t = jtag(v);
    if (t == "JObj")
    {
        foreach (p; members(v))
        {
            if (is_collection(p[1]))
            {
                result ~= pad ~ key_text(p[0]) ~ ":\n";
                emit(p[1], indent + 2, result);
            }
            else
                result ~= pad ~ key_text(p[0]) ~ ": "
                    ~ (jtag(p[1]) == "JArr" ? "[]" : jtag(p[1]) == "JObj" ? "{}" : scalar_text(p[1])) ~ "\n";
        }
        return;
    }
    if (t == "JArr")
    {
        foreach (x; list_items(payload(v)))
        {
            if (jtag(x) == "JObj" && is_collection(x))
            {
                string sub;
                emit(x, indent + 2, sub);
                result ~= pad ~ "- " ~ sub[cast(size_t)(indent + 2) .. $];
            }
            else if (jtag(x) == "JArr" && is_collection(x))
            {
                result ~= pad ~ "-\n";
                emit(x, indent + 2, result);
            }
            else
                result ~= pad ~ "- " ~ (jtag(x) == "JArr" ? "[]" : jtag(x) == "JObj" ? "{}" : scalar_text(x)) ~ "\n";
        }
        return;
    }
    result ~= pad ~ scalar_text(v) ~ "\n";
}

void installYAML(Session s)
{
    declare_json_type(s);
    prim(s, "YAML.parse", "String -> Result Json String", 1, delegate Value(C c, V a) {
        try return ok((new Parser(S(a[0]))).document());
        catch (YamlFail f) return err(f.msg);
    });
    prim(s, "YAML.render", "Json -> String", 1, delegate Value(C c, V a) {
        string result;
        string t = jtag(a[0]);
        if ((t == "JObj" || t == "JArr") && !is_collection(a[0])) result = t == "JArr" ? "[]\n" : "{}\n";
        else emit(a[0], 0, result);
        return str(result);
    });
}

