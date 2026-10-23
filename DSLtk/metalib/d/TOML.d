/**
 * metalib/d/TOML.d — MetaML host module `TOML` (D dialect): TOML parsing and rendering (values map onto Json).
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/TOML.h):
 *   TOML.parse, render
 */
module metalib.d.TOML;

import MetaML;
import metalib.d.Support;

import metalib.d.JSON;
import std.conv : to;
import std.string : indexOf;
import std.math : floor, fabs;

private final class Node
{
    int kind; // 0 table, 1 array, 2 value
    Tuple!(string, Node)[] kv;
    Node[] items;
    Value value;
    bool explicit_table;
    bool aot;
    bool frozen;
    Node find(string k)
    {
        foreach (p; kv) if (p[0] == k) return p[1];
        return null;
    }
}

class TomlFail : Exception
{
    this(string m) { super(m); }
}

private bool startsAt(string s, size_t pos, string w)
{
    return pos + w.length <= s.length && s[pos .. pos + w.length] == w;
}

private final class Parser
{
    string s_;
    size_t pos_;
    this(string s) { s_ = s; }

    Value document()
    {
        auto root = new Node;
        Node current = root;
        while (true)
        {
            skip_blank_lines();
            if (pos_ >= s_.length) break;
            if (s_[pos_] == '[')
            {
                bool aot = pos_ + 1 < s_.length && s_[pos_ + 1] == '[';
                pos_ += aot ? 2 : 1;
                auto path = key_path();
                spaces();
                if (aot)
                {
                    if (!startsAt(s_, pos_, "]]")) fail("expected `]]`");
                    pos_ += 2;
                }
                else
                {
                    if (pos_ >= s_.length || s_[pos_] != ']') fail("expected `]`");
                    ++pos_;
                }
                current = aot ? open_aot(root, path) : open_table(root, path);
            }
            else
            {
                auto path = key_path();
                spaces();
                if (pos_ >= s_.length || s_[pos_] != '=') fail("expected `=` after the key");
                ++pos_;
                spaces();
                auto v = value();
                insert(current, path, v);
            }
            end_of_line();
        }
        return convert(root);
    }

private:
    noreturn fail(string why)
    {
        size_t line = 1;
        for (size_t i = 0; i < pos_ && i < s_.length; ++i) line += s_[i] == '\n';
        throw new TomlFail("TOML: " ~ why ~ " at line " ~ to!string(line));
    }
    void spaces() { while (pos_ < s_.length && (s_[pos_] == ' ' || s_[pos_] == '\t')) ++pos_; }
    void comment()
    {
        if (pos_ < s_.length && s_[pos_] == '#')
            while (pos_ < s_.length && s_[pos_] != '\n') ++pos_;
    }
    void skip_blank_lines()
    {
        while (pos_ < s_.length)
        {
            spaces();
            comment();
            if (pos_ < s_.length && (s_[pos_] == '\n' || s_[pos_] == '\r')) ++pos_;
            else break;
        }
    }
    void skip_ws_comments()
    {
        while (pos_ < s_.length)
        {
            char c = s_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
            else if (c == '#') comment();
            else break;
        }
    }
    void end_of_line()
    {
        spaces();
        comment();
        if (pos_ < s_.length && s_[pos_] == '\r') ++pos_;
        if (pos_ < s_.length && s_[pos_] != '\n') fail("expected a newline after the value");
        if (pos_ < s_.length) ++pos_;
    }
    static bool bare(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    }
    string key()
    {
        spaces();
        if (pos_ < s_.length && s_[pos_] == '"') return basic_string();
        if (pos_ < s_.length && s_[pos_] == '\'') return literal_string();
        size_t b = pos_;
        while (pos_ < s_.length && bare(s_[pos_])) ++pos_;
        if (b == pos_) fail("expected a key");
        return s_[b .. pos_];
    }
    string[] key_path()
    {
        string[] path = [key()];
        while (true)
        {
            spaces();
            if (pos_ < s_.length && s_[pos_] == '.')
            {
                ++pos_;
                path ~= key();
            }
            else return path;
        }
    }
    Node descend(Node t, string k)
    {
        auto n = t.find(k);
        if (n is null)
        {
            n = new Node;
            t.kv ~= tuple(k, n);
            return n;
        }
        if (n.kind == 1 && n.aot && n.items.length) return n.items[$ - 1];
        if (n.kind != 0 || n.frozen) fail("key `" ~ k ~ "` is not a table");
        return n;
    }
    Node open_table(Node root, string[] path)
    {
        Node t = root;
        foreach (i; 0 .. path.length - 1) t = descend(t, path[i]);
        auto n = t.find(path[$ - 1]);
        if (n !is null)
        {
            if (n.kind != 0 || n.explicit_table || n.frozen) fail("duplicate table `" ~ path[$ - 1] ~ "`");
            n.explicit_table = true;
            return n;
        }
        n = new Node;
        n.explicit_table = true;
        t.kv ~= tuple(path[$ - 1], n);
        return n;
    }
    Node open_aot(Node root, string[] path)
    {
        Node t = root;
        foreach (i; 0 .. path.length - 1) t = descend(t, path[i]);
        auto n = t.find(path[$ - 1]);
        if (n is null)
        {
            n = new Node;
            n.kind = 1;
            n.aot = true;
            t.kv ~= tuple(path[$ - 1], n);
        }
        else if (n.kind != 1 || !n.aot) fail("key `" ~ path[$ - 1] ~ "` is not an array of tables");
        auto e = new Node;
        e.explicit_table = true;
        n.items ~= e;
        return e;
    }
    void insert(Node table, string[] path, Node v)
    {
        Node t = table;
        foreach (i; 0 .. path.length - 1) t = descend(t, path[i]);
        if (t.find(path[$ - 1]) !is null) fail("duplicate key `" ~ path[$ - 1] ~ "`");
        t.kv ~= tuple(path[$ - 1], v);
    }
    static Node leaf(Value v)
    {
        auto n = new Node;
        n.kind = 2;
        n.value = v;
        return n;
    }
    int hexn(int n)
    {
        if (pos_ + cast(size_t) n > s_.length) fail("invalid unicode escape");
        int v = 0;
        foreach (k; 0 .. n)
        {
            char h = s_[pos_++];
            int d = (h >= '0' && h <= '9') ? h - '0' : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                : (h >= 'A' && h <= 'F') ? h - 'A' + 10 : -1;
            if (d < 0) fail("invalid unicode escape");
            v = v * 16 + d;
        }
        return v;
    }
    void escape(ref string result)
    {
        if (pos_ >= s_.length) fail("unterminated string");
        char e = s_[pos_++];
        switch (e)
        {
            case 'b': result ~= '\b'; break;
            case 't': result ~= '\t'; break;
            case 'n': result ~= '\n'; break;
            case 'f': result ~= '\f'; break;
            case 'r': result ~= '\r'; break;
            case '"': result ~= '"'; break;
            case '\\': result ~= '\\'; break;
            case 'u': case 'U':
            {
                int cp = hexn(e == 'u' ? 4 : 8);
                if (!append_utf8(result, cast(uint) cp)) fail("invalid unicode escape");
                break;
            }
            default:
                fail("invalid escape `\\" ~ e ~ "`");
        }
    }
    string basic_string()
    {
        if (startsAt(s_, pos_, "\"\"\""))
        {
            pos_ += 3;
            if (pos_ < s_.length && s_[pos_] == '\n') ++pos_;
            else if (startsAt(s_, pos_, "\r\n")) pos_ += 2;
            string result;
            while (true)
            {
                if (pos_ >= s_.length) fail("unterminated string");
                if (startsAt(s_, pos_, "\"\"\""))
                {
                    pos_ += 3;
                    while (pos_ < s_.length && s_[pos_] == '"')
                    {
                        result ~= '"';
                        ++pos_;
                    }
                    return result;
                }
                char c = s_[pos_++];
                if (c == '\\')
                {
                    size_t q = pos_;
                    while (q < s_.length && (s_[q] == ' ' || s_[q] == '\t')) ++q;
                    if (q < s_.length && (s_[q] == '\n' || s_[q] == '\r'))
                    {
                        pos_ = q;
                        while (pos_ < s_.length && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r'))
                            ++pos_;
                        continue;
                    }
                    escape(result);
                }
                else result ~= c;
            }
        }
        ++pos_;
        string result;
        while (true)
        {
            if (pos_ >= s_.length || s_[pos_] == '\n') fail("unterminated string");
            char c = s_[pos_++];
            if (c == '"') return result;
            if (c == '\\') escape(result);
            else result ~= c;
        }
    }
    string literal_string()
    {
        if (startsAt(s_, pos_, "'''"))
        {
            pos_ += 3;
            if (pos_ < s_.length && s_[pos_] == '\n') ++pos_;
            else if (startsAt(s_, pos_, "\r\n")) pos_ += 2;
            auto e = s_[pos_ .. $].indexOf("'''");
            if (e < 0) fail("unterminated string");
            string result = s_[pos_ .. pos_ + e];
            pos_ = pos_ + e + 3;
            while (pos_ < s_.length && s_[pos_] == '\'')
            {
                result ~= '\'';
                ++pos_;
            }
            return result;
        }
        ++pos_;
        auto e = s_[pos_ .. $].indexOf('\'');
        auto nl = s_[pos_ .. $].indexOf('\n');
        if (e < 0 || (nl >= 0 && nl < e)) fail("unterminated string");
        string result = s_[pos_ .. pos_ + e];
        pos_ = pos_ + e + 1;
        return result;
    }
    Node value()
    {
        if (pos_ >= s_.length) fail("expected a value");
        char c = s_[pos_];
        if (c == '"') return leaf(jstr(basic_string()));
        if (c == '\'') return leaf(jstr(literal_string()));
        if (c == '[')
        {
            ++pos_;
            auto n = new Node;
            n.kind = 1;
            n.frozen = true;
            while (true)
            {
                skip_ws_comments();
                if (pos_ < s_.length && s_[pos_] == ']') { ++pos_; return n; }
                n.items ~= value();
                skip_ws_comments();
                if (pos_ < s_.length && s_[pos_] == ',') { ++pos_; continue; }
                if (pos_ < s_.length && s_[pos_] == ']') { ++pos_; return n; }
                fail("expected `,` or `]` in array");
            }
        }
        if (c == '{')
        {
            ++pos_;
            auto n = new Node;
            spaces();
            if (pos_ < s_.length && s_[pos_] == '}')
            {
                ++pos_;
                n.frozen = true;
                return n;
            }
            while (true)
            {
                auto path = key_path();
                spaces();
                if (pos_ >= s_.length || s_[pos_] != '=') fail("expected `=` in inline table");
                ++pos_;
                spaces();
                auto v = value();
                insert(n, path, v);
                spaces();
                if (pos_ < s_.length && s_[pos_] == ',')
                {
                    ++pos_;
                    spaces();
                    continue;
                }
                if (pos_ < s_.length && s_[pos_] == '}')
                {
                    ++pos_;
                    n.frozen = true;
                    return n;
                }
                fail("expected `,` or `}` in inline table");
            }
        }
        size_t b = pos_;
        while (pos_ < s_.length)
        {
            char d = s_[pos_];
            if (d == ' ' || d == '\t' || d == '\n' || d == '\r' || d == ',' || d == ']' || d == '}' || d == '#') break;
            ++pos_;
        }
        string tok = s_[b .. pos_];
        if (!tok.length) fail("expected a value");
        return leaf(scalar(tok));
    }
    Value scalar(string tok)
    {
        if (tok == "true") return jbool(true);
        if (tok == "false") return jbool(false);
        if (tok == "inf" || tok == "+inf") return jnum(double.infinity);
        if (tok == "-inf") return jnum(-double.infinity);
        if (tok == "nan" || tok == "+nan" || tok == "-nan") return jnum(double.nan);
        string body = tok;
        bool neg = false;
        if (body.length && (body[0] == '+' || body[0] == '-'))
        {
            neg = body[0] == '-';
            body = body[1 .. $];
        }
        if (body.length > 2 && body[0] == '0' && body[1] == 'o')
        {
            long v = 0;
            foreach (k; 2 .. body.length)
            {
                if (body[k] == '_') continue;
                if (body[k] < '0' || body[k] > '7') fail("invalid integer `" ~ tok ~ "`");
                v = v * 8 + (body[k] - '0');
            }
            return jnum(cast(double)(neg ? -v : v));
        }
        if (body.length > 2 && body[0] == '0' && (body[1] == 'x' || body[1] == 'b'))
        {
            auto v = parse_int(body);
            if (v.isNull) fail("invalid integer `" ~ tok ~ "`");
            return jnum(cast(double)(neg ? -v.get : v.get));
        }
        bool has_dot = false, has_exp = false, other = false;
        foreach (char d; body)
        {
            if (d == '.') has_dot = true;
            else if (d == 'e' || d == 'E') has_exp = true;
            else if (!((d >= '0' && d <= '9') || d == '_')) other = true;
        }
        if (body.length >= 5 && body[0] >= '0' && body[0] <= '9'
            && (body.indexOf(':') >= 0 || body.indexOf('-') >= 0 || body.indexOf('T') >= 0))
            return jstr(tok);
        import std.algorithm : canFind;
        if (other && !(has_exp && (body.canFind('+') || body.canFind('-')))) fail("invalid value `" ~ tok ~ "`");
        if (has_dot || has_exp)
        {
            auto f = parse_float(tok);
            if (f.isNull) fail("invalid float `" ~ tok ~ "`");
            return jnum(f.get);
        }
        auto v = parse_int(tok);
        if (v.isNull) fail("invalid integer `" ~ tok ~ "`");
        return jnum(cast(double) v.get);
    }
    Value convert(Node n)
    {
        if (n.kind == 2) return n.value;
        if (n.kind == 1)
        {
            Value[] xs;
            foreach (i; n.items) xs ~= convert(i);
            return jarr(xs);
        }
        Tuple!(string, Value)[] kv;
        foreach (p; n.kv) kv ~= tuple(p[0], convert(p[1]));
        return jobj(kv);
    }
}

private bool bare_key(string k)
{
    if (!k.length) return false;
    foreach (char c; k)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return false;
    return true;
}
private string key_text(string k) { return bare_key(k) ? k : jquote(k); }
private bool is_table_array(Value v)
{
    if (jtag(v) != "JArr") return false;
    auto xs = list_items(payload(v));
    if (!xs.length) return false;
    foreach (x; xs) if (jtag(x) != "JObj") return false;
    return true;
}
private string value_text(Value v)
{
    string t = jtag(v);
    if (t == "JStr") return jquote(str_of(payload(v)));
    if (t == "JBool") return payload(v).i ? "true" : "false";
    if (t == "JNum")
    {
        double d = payload(v).f;
        if (d != d) return "nan";
        if (d == double.infinity) return "inf";
        if (d == -double.infinity) return "-inf";
        if (d == floor(d) && fabs(d) < 1e15) return to!string(cast(long) d);
        return format_float(d);
    }
    if (t == "JArr")
    {
        string result = "[";
        foreach (i, x; list_items(payload(v))) result ~= (i ? ", " : "") ~ value_text(x);
        return result ~ "]";
    }
    if (t == "JObj")
    {
        auto kv = members(v);
        if (!kv.length) return "{}";
        string result = "{ ";
        foreach (i, p; kv) result ~= (i ? ", " : "") ~ key_text(p[0]) ~ " = " ~ value_text(p[1]);
        return result ~ " }";
    }
    throw new TomlFail("TOML: cannot represent null");
}
private void emit(Value obj, string path, ref string result)
{
    auto kv = members(obj);
    foreach (p; kv)
        if (jtag(p[1]) != "JObj" && !is_table_array(p[1]))
            result ~= key_text(p[0]) ~ " = " ~ value_text(p[1]) ~ "\n";
    foreach (p; kv)
    {
        string sub = !path.length ? key_text(p[0]) : path ~ "." ~ key_text(p[0]);
        if (jtag(p[1]) == "JObj")
        {
            if (result.length) result ~= "\n";
            result ~= "[" ~ sub ~ "]\n";
            emit(p[1], sub, result);
        }
        else if (is_table_array(p[1]))
            foreach (x; list_items(payload(p[1])))
            {
                if (result.length) result ~= "\n";
                result ~= "[[" ~ sub ~ "]]\n";
                emit(x, sub, result);
            }
    }
}

void installTOML(Session s)
{
    declare_json_type(s);
    prim(s, "TOML.parse", "String -> Result Json String", 1, delegate Value(C c, V a) {
        try return ok((new Parser(S(a[0]))).document());
        catch (TomlFail f) return err(f.msg);
    });
    prim(s, "TOML.render", "Json -> Result String String", 1, delegate Value(C c, V a) {
        if (jtag(a[0]) != "JObj") return err("TOML: the top level must be an object");
        try
        {
            string result;
            emit(a[0], "", result);
            return ok(str(result));
        }
        catch (TomlFail f) return err(f.msg);
    });
}

