/**
 * metalib/d/FlexBridge.d — MetaML host module `FlexBridge` (D dialect): flex-style longest-match tokenizer (DSLtk `CompiledPattern`).
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/FlexBridge.h):
 *   FlexBridge.tokenize : [(String, String)] -> String -> Result [(String, String)] String
 *   FlexBridge.tokenize_spans : [(String, String)] -> String -> Result [(String, String, Int)] String
 *   FlexBridge.valid_pattern : String -> Result Unit String
 *   FlexBridge.match_prefix : String -> String -> Option String
 *   FlexBridge.matches : String -> String -> Bool
 *
 *   Patterns: literals, `.`, `[a-z]`, `[^...]`, quantifiers `* + ?` and `!` (negative
 *   lookahead), escapes `\n \t \r \d \w \s \X`. Matching is greedy without
 *   backtracking. Rules whose name starts with `_` are skipped; the longest
 *   match wins and ties go to the earlier rule.
 */
module metalib.d.FlexBridge;

import MetaML;
import metalib.d.Support;

import DSLtk : CompiledPattern;
import std.conv : to;

alias Pattern = CompiledPattern;

private bool escape_class(char e, ref string cls)
{
    switch (e)
    {
        case 'd': cls ~= "0-9"; return true;
        case 'w': cls ~= "a-zA-Z0-9_"; return true;
        case 's': cls ~= " \t\n\r"; return true;
        default: return false;
    }
}
private char escape_char(char e)
{
    switch (e)
    {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        default: return e;
    }
}

/// Compiles the FlexBridge pattern syntax onto DSLtk's CompiledPattern:
/// the DSLtk subset (literals, `.`, `[...]`, `[^...]`, `*`, `+`, `?`, `!`)
/// plus backslash escapes `\n \t \r \d \w \s` and `\X` for a literal X.
Pattern flex_compile(string pat)
{
    Pattern result;
    Pattern bad(string why)
    {
        Pattern p;
        p.valid = false;
        p.error = why;
        return p;
    }
    size_t i = 0;
    while (i < pat.length)
    {
        Pattern.Atom a;
        char c = pat[i];
        if (c == '*' || c == '+' || c == '?' || c == '!')
            return bad("quantifier `" ~ c ~ "` has nothing to repeat");
        if (c == '.')
        {
            a.any = true;
            ++i;
        }
        else if (c == '\\')
        {
            if (i + 1 >= pat.length) return bad("trailing backslash");
            char e = pat[i + 1];
            string cls;
            if (escape_class(e, cls))
            {
                a.is_class = true;
                a.cls = cls;
            }
            else a.ch = escape_char(e);
            i += 2;
        }
        else if (c == '[')
        {
            ++i;
            a.is_class = true;
            if (i < pat.length && pat[i] == '^')
            {
                a.negate = true;
                ++i;
            }
            bool closed = false;
            string dash;
            while (i < pat.length)
            {
                char d = pat[i];
                if (d == ']' && !(a.cls.length == 0 && dash.length == 0))
                {
                    closed = true;
                    ++i;
                    break;
                }
                if (d == '\\' && i + 1 < pat.length)
                {
                    char e = pat[i + 1];
                    if (!escape_class(e, a.cls))
                    {
                        char lit = escape_char(e);
                        if (lit == '-') dash = "-";
                        else a.cls ~= lit;
                    }
                    i += 2;
                    continue;
                }
                if (d == '-' && (a.cls.length == 0 || i + 1 >= pat.length || pat[i + 1] == ']'))
                {
                    dash = "-";
                    ++i;
                    continue;
                }
                a.cls ~= d;
                ++i;
            }
            if (!closed) return bad("unterminated character class");
            a.cls ~= dash;
        }
        else
        {
            a.ch = c;
            ++i;
        }
        if (i < pat.length && (pat[i] == '*' || pat[i] == '+' || pat[i] == '?' || pat[i] == '!'))
            a.quant = pat[i++];
        result.atoms ~= a;
    }
    if (!result.atoms.length) return bad("empty pattern");
    return result;
}

private struct Rule
{
    string name;
    Pattern pat;
}
private struct Tok
{
    string name, text;
    size_t offset;
}

private string lex_rules(Value[] rs, ref Rule[] result)
{
    foreach (r; rs)
    {
        auto x = Rule(str_of(fst(r)), flex_compile(str_of(snd(r))));
        if (!x.pat.valid) return "FlexBridge: invalid pattern for rule `" ~ x.name ~ "`: " ~ x.pat.error;
        result ~= x;
    }
    return "";
}

private string tokenize(Rule[] rules, string text, ref Tok[] result)
{
    size_t pos = 0;
    while (pos < text.length)
    {
        size_t best = 0;
        ptrdiff_t win = -1;
        foreach (k, ref r; rules)
        {
            auto e = r.pat.match_at(text, pos);
            if (!e.isNull && e.get > pos && e.get - pos > best)
            {
                best = e.get - pos;
                win = cast(ptrdiff_t) k;
            }
        }
        if (win < 0)
        {
            size_t line = 1, col = 1;
            foreach (k; 0 .. pos)
                if (text[k] == '\n')
                {
                    ++line;
                    col = 1;
                }
                else ++col;
            return "FlexBridge: no rule matches " ~ quote_string(text[pos .. pos + 1]) ~ " at line "
                ~ to!string(line) ~ ", column " ~ to!string(col);
        }
        auto w = rules[win];
        if (!w.name.length || w.name[0] != '_') result ~= Tok(w.name, text[pos .. pos + best], pos);
        pos += best;
    }
    return "";
}

void installFlexBridge(Session s)
{
    prim(s, "FlexBridge.tokenize", "[(String, String)] -> String -> Result [(String, String)] String", 2,
        delegate Value(C c, V a) {
            Rule[] rules;
            auto e = lex_rules(items(a[0]), rules);
            if (e.length) return err(e);
            Tok[] toks;
            e = tokenize(rules, S(a[1]), toks);
            if (e.length) return err(e);
            Value[] result;
            foreach (t; toks) result ~= pair(str(t.name), str(t.text));
            return ok(list(result));
        });
    prim(s, "FlexBridge.tokenize_spans", "[(String, String)] -> String -> Result [(String, String, Int)] String", 2,
        delegate Value(C c, V a) {
            Rule[] rules;
            auto e = lex_rules(items(a[0]), rules);
            if (e.length) return err(e);
            Tok[] toks;
            e = tokenize(rules, S(a[1]), toks);
            if (e.length) return err(e);
            Value[] result;
            foreach (t; toks) result ~= triple(str(t.name), str(t.text), integer(cast(long) t.offset));
            return ok(list(result));
        });
    prim(s, "FlexBridge.valid_pattern", "String -> Result Unit String", 1, delegate Value(C c, V a) {
        auto p = flex_compile(S(a[0]));
        if (!p.valid) return err(p.error);
        return ok(unit());
    });
    prim(s, "FlexBridge.match_prefix", "String -> String -> Option String", 2, delegate Value(C c, V a) {
        auto p = flex_compile(S(a[0]));
        if (!p.valid) return none();
        auto e = p.match_at(S(a[1]), 0);
        if (e.isNull) return none();
        return some(str(S(a[1])[0 .. e.get]));
    });
    prim(s, "FlexBridge.matches", "String -> String -> Bool", 2, delegate Value(C c, V a) {
        auto p = flex_compile(S(a[0]));
        if (!p.valid) return boolean(false);
        auto e = p.match_at(S(a[1]), 0);
        return boolean(!e.isNull && e.get == S(a[1]).length);
    });
}

