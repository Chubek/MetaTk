/**
 * metalib/d/Parsec.d — MetaML host module `Parsec` (D dialect): parser combinators as host objects (`Parser 'a`).
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/Parsec.h):
 *   type Parser 'a   (opaque host handle)
 *   Parsec.char : String -> Parser String
 *   Parsec.string : String -> Parser String
 *   Parsec.any : Unit -> Parser String
 *   Parsec.satisfy : (String -> Bool) -> Parser String
 *   Parsec.one_of : String -> Parser String
 *   Parsec.seq : Parser 'a -> Parser 'b -> Parser ('a, 'b)
 *   Parsec.left : Parser 'a -> Parser 'b -> Parser 'a
 *   Parsec.right : Parser 'a -> Parser 'b -> Parser 'b
 *   Parsec.between : Parser 'o -> Parser 'c -> Parser 'a -> Parser 'a
 *   Parsec.alt : Parser 'a -> Parser 'a -> Parser 'a
 *   Parsec.choice : [Parser 'a] -> Parser 'a
 *   Parsec.many : Parser 'a -> Parser ['a]
 *   Parsec.many1 : Parser 'a -> Parser ['a]
 *   Parsec.sep_by : Parser 'a -> Parser 's -> Parser ['a]
 *   Parsec.map : ('a -> 'b) -> Parser 'a -> Parser 'b
 *   Parsec.bind : Parser 'a -> ('a -> Parser 'b) -> Parser 'b
 *   Parsec.pure : 'a -> Parser 'a
 *   Parsec.fail : String -> Parser 'a
 *   Parsec.optional : Parser 'a -> Parser (Option 'a)
 *   Parsec.lazy : (Unit -> Parser 'a) -> Parser 'a
 *   Parsec.label : String -> Parser 'a -> Parser 'a
 *   Parsec.regex : String -> Parser String
 *   Parsec.token : Parser 'a -> Parser 'a
 *   Parsec.spaces : Unit -> Parser Unit
 *   Parsec.eof : Unit -> Parser Unit
 *   Parsec.run : Parser 'a -> String -> Result 'a String
 *   Parsec.run_prefix : Parser 'a -> String -> Result ('a, String) String
 *
 *   Parsers backtrack on failure; errors report the furthest position reached.
 */
module metalib.d.Parsec;

import MetaML;
import metalib.d.Support;
import metalib.d.FlexBridge;

import DSLtk : CompiledPattern;
import core.sync.mutex : Mutex;
import std.algorithm.searching : canFind;
import std.conv : to;
import std.typecons : Tuple, tuple, Nullable;

private enum PK
{
    Str, Any, Satisfy, OneOf, Seq, Alt, Many, Many1, Map, Bind, Pure, Fail, Opt, SepBy,
    Left, Right, Lazy, Label, Regex, Token, Spaces, Eof
}

private final class PObj : ForeignObj
{
    PK kind;
    string text;
    Value fn;
    PObj a, b;
    PObj[] alts;
    CompiledPattern pat;
    Mutex mutex;
    PObj forced;
    this(PK k)
    {
        super("Parser");
        kind = k;
        mutex = new Mutex;
    }
}

private Value wrap(PObj p)
{
    Value v;
    v.kind = VK.Foreign;
    v.obj = p;
    return v;
}
private PObj get(Value v, C c)
{
    auto p = cast(PObj) v.obj;
    if (v.kind != VK.Foreign || p is null) c.error("M0404", "expected a Parser");
    return p;
}
private PObj mk(PK k, PObj a = null, PObj b = null)
{
    auto p = new PObj(k);
    p.a = a;
    p.b = b;
    return p;
}

private size_t utf8_len(string s, size_t i)
{
    ubyte c = cast(ubyte) s[i];
    size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
    return i + n <= s.length ? n : 1;
}

private bool is_space(char ch) { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r'; }

private alias R = Nullable!(Tuple!(Value, size_t));
private R yes(Value v, size_t p) { return R(tuple(v, p)); }

private final class Run
{
    C c;
    string s;
    size_t far = 0;
    string[] expected;
    this(C c, string s)
    {
        this.c = c;
        this.s = s;
    }

    void expect(size_t pos, string what)
    {
        if (pos > far)
        {
            far = pos;
            expected = null;
        }
        if (pos == far && !expected.canFind(what)) expected ~= what;
    }

    R go(PObj p, size_t pos)
    {
        final switch (p.kind)
        {
            case PK.Str:
                if (s.length - pos >= p.text.length && s[pos .. pos + p.text.length] == p.text)
                    return yes(make_str(p.text), pos + p.text.length);
                expect(pos, quote_string(p.text));
                return R.init;
            case PK.Any:
                if (pos < s.length)
                {
                    auto n = utf8_len(s, pos);
                    return yes(make_str(s[pos .. pos + n]), pos + n);
                }
                expect(pos, "any character");
                return R.init;
            case PK.Satisfy:
                if (pos < s.length)
                {
                    auto n = utf8_len(s, pos);
                    auto ch = make_str(s[pos .. pos + n]);
                    if (c.apply(p.fn, [ch]).i) return yes(ch, pos + n);
                }
                expect(pos, p.text);
                return R.init;
            case PK.OneOf:
                if (pos < s.length && p.text.canFind(s[pos])) return yes(make_str(s[pos .. pos + 1]), pos + 1);
                expect(pos, "one of " ~ quote_string(p.text));
                return R.init;
            case PK.Seq:
            {
                auto x = go(p.a, pos);
                if (x.isNull) return R.init;
                auto y = go(p.b, x.get[1]);
                if (y.isNull) return R.init;
                return yes(make_tuple([x.get[0], y.get[0]]), y.get[1]);
            }
            case PK.Left:
            case PK.Right:
            {
                auto x = go(p.a, pos);
                if (x.isNull) return R.init;
                auto y = go(p.b, x.get[1]);
                if (y.isNull) return R.init;
                return yes(p.kind == PK.Left ? x.get[0] : y.get[0], y.get[1]);
            }
            case PK.Alt:
                foreach (q; p.alts)
                {
                    auto x = go(q, pos);
                    if (!x.isNull) return x;
                }
                return R.init;
            case PK.Many:
            case PK.Many1:
            {
                Value[] xs;
                size_t cur = pos;
                while (true)
                {
                    auto x = go(p.a, cur);
                    if (x.isNull) break;
                    xs ~= x.get[0];
                    if (x.get[1] == cur) break;
                    cur = x.get[1];
                }
                if (p.kind == PK.Many1 && !xs.length) return R.init;
                return yes(make_list(xs), cur);
            }
            case PK.SepBy:
            {
                Value[] xs;
                auto x = go(p.a, pos);
                if (x.isNull) return yes(make_list(xs), pos);
                xs ~= x.get[0];
                size_t cur = x.get[1];
                while (true)
                {
                    auto sep = go(p.b, cur);
                    if (sep.isNull) break;
                    auto y = go(p.a, sep.get[1]);
                    if (y.isNull || y.get[1] == cur) break;
                    xs ~= y.get[0];
                    cur = y.get[1];
                }
                return yes(make_list(xs), cur);
            }
            case PK.Map:
            {
                auto x = go(p.a, pos);
                if (x.isNull) return R.init;
                return yes(c.apply(p.fn, [x.get[0]]), x.get[1]);
            }
            case PK.Bind:
            {
                auto x = go(p.a, pos);
                if (x.isNull) return R.init;
                auto next = get(c.apply(p.fn, [x.get[0]]), c);
                return go(next, x.get[1]);
            }
            case PK.Pure:
                return yes(p.fn, pos);
            case PK.Fail:
                expect(pos, p.text);
                return R.init;
            case PK.Opt:
            {
                auto x = go(p.a, pos);
                if (!x.isNull) return yes(make_some(x.get[0]), x.get[1]);
                return yes(make_none(), pos);
            }
            case PK.Lazy:
            {
                PObj q;
                p.mutex.lock();
                q = p.forced;
                p.mutex.unlock();
                if (q is null)
                {
                    q = get(c.apply(p.fn, [Value.unit()]), c);
                    p.mutex.lock();
                    p.forced = q;
                    p.mutex.unlock();
                }
                return go(q, pos);
            }
            case PK.Label:
            {
                size_t far0 = far;
                auto saved = expected.dup;
                auto x = go(p.a, pos);
                if (x.isNull && far <= pos)
                {
                    far = far0;
                    expected = saved;
                    expect(pos, p.text);
                }
                return x;
            }
            case PK.Regex:
            {
                auto e = p.pat.match_at(s, pos);
                if (!e.isNull) return yes(make_str(s[pos .. e.get]), e.get);
                expect(pos, "a match of " ~ quote_string(p.text));
                return R.init;
            }
            case PK.Token:
            {
                auto x = go(p.a, pos);
                if (x.isNull) return R.init;
                size_t cur = x.get[1];
                while (cur < s.length && is_space(s[cur])) ++cur;
                return yes(x.get[0], cur);
            }
            case PK.Spaces:
            {
                size_t cur = pos;
                while (cur < s.length && is_space(s[cur])) ++cur;
                return yes(Value.unit(), cur);
            }
            case PK.Eof:
                if (pos == s.length) return yes(Value.unit(), pos);
                expect(pos, "end of input");
                return R.init;
        }
    }

    string message()
    {
        size_t line = 1, col = 1;
        for (size_t k = 0; k < far && k < s.length; ++k)
            if (s[k] == '\n')
            {
                ++line;
                col = 1;
            }
            else ++col;
        string exp;
        foreach (i, e; expected) exp ~= (i == 0 ? "" : i + 1 == expected.length ? " or " : ", ") ~ e;
        string found = far < s.length ? quote_string(s[far .. far + utf8_len(s, far)]) : "end of input";
        return "Parsec: expected " ~ (exp.length ? exp : "something else") ~ " at line " ~ to!string(line)
            ~ ", column " ~ to!string(col) ~ ", found " ~ found;
    }
}

private PrimFn leaf_parser(PK k)
{
    return delegate Value(C c, V a) => wrap(mk(k));
}
private PrimFn unary_parser(PK k)
{
    return delegate Value(C c, V a) => wrap(mk(k, get(a[0], c)));
}
private PrimFn binary_parser(PK k)
{
    return delegate Value(C c, V a) => wrap(mk(k, get(a[0], c), get(a[1], c)));
}

void installParsec(Session s)
{
    if (s.checker().global.types.find("Parser") is null)
        s.declare("type Parser 'a\n", "<metalib Parsec>");
    prim(s, "Parsec.char", "String -> Parser String", 1, delegate Value(C c, V a) {
        if (!S(a[0]).length || utf8_len(S(a[0]), 0) != S(a[0]).length)
            c.error("M0404", "Parsec.char expects a one-character string");
        auto p = mk(PK.Str);
        p.text = S(a[0]);
        return wrap(p);
    });
    prim(s, "Parsec.string", "String -> Parser String", 1, delegate Value(C c, V a) {
        auto p = mk(PK.Str);
        p.text = S(a[0]);
        return wrap(p);
    });
    prim(s, "Parsec.any", "Unit -> Parser String", 1, leaf_parser(PK.Any));
    prim(s, "Parsec.satisfy", "(String -> Bool) -> Parser String", 1, delegate Value(C c, V a) {
        auto p = mk(PK.Satisfy);
        p.fn = a[0];
        p.text = "a character satisfying the predicate";
        return wrap(p);
    });
    prim(s, "Parsec.one_of", "String -> Parser String", 1, delegate Value(C c, V a) {
        auto p = mk(PK.OneOf);
        p.text = S(a[0]);
        return wrap(p);
    });
    prim(s, "Parsec.seq", "Parser 'a -> Parser 'b -> Parser ('a, 'b)", 2, binary_parser(PK.Seq));
    prim(s, "Parsec.left", "Parser 'a -> Parser 'b -> Parser 'a", 2, binary_parser(PK.Left));
    prim(s, "Parsec.right", "Parser 'a -> Parser 'b -> Parser 'b", 2, binary_parser(PK.Right));
    prim(s, "Parsec.between", "Parser 'o -> Parser 'c -> Parser 'a -> Parser 'a", 3, delegate Value(C c, V a) {
        return wrap(mk(PK.Left, mk(PK.Right, get(a[0], c), get(a[2], c)), get(a[1], c)));
    });
    prim(s, "Parsec.alt", "Parser 'a -> Parser 'a -> Parser 'a", 2, delegate Value(C c, V a) {
        auto p = mk(PK.Alt);
        p.alts = [get(a[0], c), get(a[1], c)];
        return wrap(p);
    });
    prim(s, "Parsec.choice", "[Parser 'a] -> Parser 'a", 1, delegate Value(C c, V a) {
        auto p = mk(PK.Alt);
        foreach (x; items(a[0])) p.alts ~= get(x, c);
        return wrap(p);
    });
    prim(s, "Parsec.many", "Parser 'a -> Parser ['a]", 1, unary_parser(PK.Many));
    prim(s, "Parsec.many1", "Parser 'a -> Parser ['a]", 1, unary_parser(PK.Many1));
    prim(s, "Parsec.sep_by", "Parser 'a -> Parser 's -> Parser ['a]", 2, binary_parser(PK.SepBy));
    prim(s, "Parsec.map", "('a -> 'b) -> Parser 'a -> Parser 'b", 2, delegate Value(C c, V a) {
        auto p = mk(PK.Map, get(a[1], c));
        p.fn = a[0];
        return wrap(p);
    });
    prim(s, "Parsec.bind", "Parser 'a -> ('a -> Parser 'b) -> Parser 'b", 2, delegate Value(C c, V a) {
        auto p = mk(PK.Bind, get(a[0], c));
        p.fn = a[1];
        return wrap(p);
    });
    prim(s, "Parsec.pure", "'a -> Parser 'a", 1, delegate Value(C c, V a) {
        auto p = mk(PK.Pure);
        p.fn = a[0];
        return wrap(p);
    });
    prim(s, "Parsec.fail", "String -> Parser 'a", 1, delegate Value(C c, V a) {
        auto p = mk(PK.Fail);
        p.text = S(a[0]);
        return wrap(p);
    });
    prim(s, "Parsec.optional", "Parser 'a -> Parser (Option 'a)", 1, unary_parser(PK.Opt));
    prim(s, "Parsec.lazy", "(Unit -> Parser 'a) -> Parser 'a", 1, delegate Value(C c, V a) {
        auto p = mk(PK.Lazy);
        p.fn = a[0];
        return wrap(p);
    });
    prim(s, "Parsec.label", "String -> Parser 'a -> Parser 'a", 2, delegate Value(C c, V a) {
        auto p = mk(PK.Label, get(a[1], c));
        p.text = S(a[0]);
        return wrap(p);
    });
    prim(s, "Parsec.regex", "String -> Parser String", 1, delegate Value(C c, V a) {
        auto p = mk(PK.Regex);
        p.text = S(a[0]);
        p.pat = flex_compile(S(a[0]));
        if (!p.pat.valid) c.error("M0404", "Parsec.regex: invalid pattern: " ~ p.pat.error);
        return wrap(p);
    });
    prim(s, "Parsec.token", "Parser 'a -> Parser 'a", 1, unary_parser(PK.Token));
    prim(s, "Parsec.spaces", "Unit -> Parser Unit", 1, leaf_parser(PK.Spaces));
    prim(s, "Parsec.eof", "Unit -> Parser Unit", 1, leaf_parser(PK.Eof));
    prim(s, "Parsec.run", "Parser 'a -> String -> Result 'a String", 2, delegate Value(C c, V a) {
        auto r = new Run(c, S(a[1]));
        auto x = r.go(get(a[0], c), 0);
        if (!x.isNull && x.get[1] == r.s.length) return make_ok(x.get[0]);
        if (!x.isNull) r.expect(x.get[1], "end of input");
        return err(r.message());
    });
    prim(s, "Parsec.run_prefix", "Parser 'a -> String -> Result ('a, String) String", 2, delegate Value(C c, V a) {
        auto r = new Run(c, S(a[1]));
        auto x = r.go(get(a[0], c), 0);
        if (!x.isNull) return make_ok(pair(x.get[0], str(r.s[x.get[1] .. $])));
        return err(r.message());
    });
}

