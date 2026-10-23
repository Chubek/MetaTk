/**
 * metalib/d/YACCBridge.d — MetaML host module `YACCBridge` (D dialect): yacc-style grammars parsed with DSLtk Earley `CFG` into `Ast`.
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/YACCBridge.h):
 *   YACCBridge.check : String -> Result [String] String
 *   YACCBridge.literals : String -> Result [String] String
 *   YACCBridge.parse : String -> String -> Result Ast String
 *   YACCBridge.parse_tokens : String -> [(String, String)] -> Result Ast String
 *
 *   Grammar: `rule : sym sym | sym ;` (first rule is the start symbol). A name
 *   with rules is a nonterminal, any other name is a token kind; "x" or 'x' is
 *   a literal. `parse` lexes input into literals, NUM, ID and STR tokens.
 *   Tokens become `Leaf kind text`; nonterminals become `Node rule [...]`.
 */
module metalib.d.YACCBridge;

import MetaML;
import metalib.d.Support;
import metalib.d.ASTBuilder;

import DSLtk : CFG, CFGSymbol, ASTNode;
import std.conv : to;
import std.ascii : isAlpha, isAlphaNum, isDigit;
import std.algorithm.searching : canFind;

private class GrammarFail : Exception
{
    this(string m) { super(m); }
}

/// A yacc-style grammar compiled onto DSLtk's Earley CFG.
private final class Grammar
{
    CFG cfg;
    string[] nonterminals;
    string[] literals;
}

private struct GT
{
    char kind; // i = ident, l = literal, : | ;
    string text;
    size_t line;
}

private GrammarFail gfail(size_t ln, string why)
{
    return new GrammarFail("YACCBridge: grammar line " ~ to!string(ln) ~ ": " ~ why);
}

private Grammar compile(string src)
{
    GT[] ts;
    size_t i = 0, line = 1;
    while (i < src.length)
    {
        char c = src[i];
        if (c == '\n')
        {
            ++line;
            ++i;
        }
        else if (c == ' ' || c == '\t' || c == '\r') ++i;
        else if (c == '#' || (c == '/' && i + 1 < src.length && src[i + 1] == '/'))
        {
            while (i < src.length && src[i] != '\n') ++i;
        }
        else if (c == ':' || c == '|' || c == ';')
        {
            ts ~= GT(c, [c], line);
            ++i;
        }
        else if (c == '"' || c == '\'')
        {
            size_t j = i + 1;
            string text;
            while (j < src.length && src[j] != c && src[j] != '\n')
            {
                if (src[j] == '\\' && j + 1 < src.length) ++j;
                text ~= src[j++];
            }
            if (j >= src.length || src[j] != c) throw gfail(line, "unterminated literal");
            if (!text.length) throw gfail(line, "empty literal");
            ts ~= GT('l', text, line);
            i = j + 1;
        }
        else if (isAlpha(c) || c == '_')
        {
            size_t j = i;
            while (j < src.length && (isAlphaNum(src[j]) || src[j] == '_')) ++j;
            ts ~= GT('i', src[i .. j], line);
            i = j;
        }
        else throw gfail(line, "unexpected character `" ~ c ~ "`");
    }
    struct Raw
    {
        string lhs;
        GT[][] alts;
    }
    Raw[] raws;
    size_t k = 0;
    while (k < ts.length)
    {
        if (ts[k].kind != 'i') throw gfail(ts[k].line, "expected a rule name, found `" ~ ts[k].text ~ "`");
        if (k + 1 >= ts.length || ts[k + 1].kind != ':')
            throw gfail(ts[k].line, "expected `:` after `" ~ ts[k].text ~ "`");
        Raw r = Raw(ts[k].text, [null]);
        k += 2;
        while (true)
        {
            if (k >= ts.length) throw gfail(ts[$ - 1].line, "rule `" ~ r.lhs ~ "` is missing its final `;`");
            if (ts[k].kind == ';')
            {
                ++k;
                break;
            }
            if (ts[k].kind == '|') r.alts ~= cast(GT[]) null;
            else if (ts[k].kind == ':') throw gfail(ts[k].line, "unexpected `:` (missing `;` before this rule?)");
            else r.alts[$ - 1] ~= ts[k];
            ++k;
        }
        raws ~= r;
    }
    if (!raws.length) throw new GrammarFail("YACCBridge: the grammar has no rules");
    auto g = new Grammar;
    bool[string] nts;
    foreach (r; raws)
        if (r.lhs !in nts)
        {
            nts[r.lhs] = true;
            g.nonterminals ~= r.lhs;
        }
    g.cfg.start = raws[0].lhs;
    foreach (r; raws)
        foreach (alt; r.alts)
        {
            CFGSymbol[] rhs;
            foreach (t; alt)
            {
                if (t.kind == 'l')
                {
                    rhs ~= CFGSymbol.token("'" ~ t.text);
                    if (!g.literals.canFind(t.text)) g.literals ~= t.text;
                }
                else if (t.text in nts) rhs ~= CFGSymbol.nonterminal(t.text);
                else rhs ~= CFGSymbol.token("%" ~ t.text);
            }
            g.cfg.add(r.lhs, rhs);
        }
    return g;
}

private struct Tok
{
    string kind, text;
}

private bool is_word(string s)
{
    foreach (char c; s) if (!(isAlphaNum(c) || c == '_')) return false;
    return true;
}

/// Default lexer: literals of the grammar, NUM, ID, STR; whitespace skipped.
private Tok[] lex(Grammar g, string s)
{
    Tok[] result;
    size_t i = 0;
    while (i < s.length)
    {
        char c = s[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
        {
            ++i;
            continue;
        }
        size_t cls = 0;
        string kind;
        if (isDigit(c))
        {
            size_t j = i;
            while (j < s.length && isDigit(s[j])) ++j;
            if (j + 1 < s.length && s[j] == '.' && isDigit(s[j + 1]))
            {
                ++j;
                while (j < s.length && isDigit(s[j])) ++j;
            }
            cls = j - i;
            kind = "NUM";
        }
        else if (isAlpha(c) || c == '_')
        {
            size_t j = i;
            while (j < s.length && (isAlphaNum(s[j]) || s[j] == '_')) ++j;
            cls = j - i;
            kind = "ID";
        }
        else if (c == '"')
        {
            size_t j = i + 1;
            while (j < s.length && s[j] != '"') j += (s[j] == '\\' && j + 1 < s.length) ? 2 : 1;
            if (j >= s.length) throw new GrammarFail("YACCBridge: unterminated string at offset " ~ to!string(i));
            cls = j + 1 - i;
            kind = "STR";
        }
        size_t lit = 0;
        foreach (l; g.literals)
            if (l.length > lit && s.length - i >= l.length && s[i .. i + l.length] == l)
            {
                if (is_word(l) && i + l.length < s.length && (isAlphaNum(s[i + l.length]) || s[i + l.length] == '_'))
                    continue;
                lit = l.length;
            }
        if (lit && lit >= cls)
        {
            result ~= Tok("LIT", s[i .. i + lit]);
            i += lit;
        }
        else if (cls)
        {
            result ~= Tok(kind, s[i .. i + cls]);
            i += cls;
        }
        else
            throw new GrammarFail("YACCBridge: unexpected character " ~ quote_string(s[i .. i + 1])
                ~ " at offset " ~ to!string(i));
    }
    return result;
}

private string show_key(string k)
{
    if (k == "<end-of-input>") return "end of input";
    if (k[0] == '\'') return quote_string(k[1 .. $]);
    return k[1 .. $];
}

private Value convert(ref const ASTNode n, Tok[] toks, ref size_t next)
{
    if (n.tag() == "token" && n.is_leaf())
    {
        auto t = toks[next++];
        return leaf(t.kind, t.text);
    }
    Value[] ks;
    foreach (i; 0 .. n.size()) ks ~= convert(n.child(i), toks, next);
    return node(n.tag(), ks);
}

private Value run(Grammar g, Tok[] toks)
{
    string[] keys;
    foreach (t; toks) keys ~= g.literals.canFind(t.text) ? "'" ~ t.text : "%" ~ t.kind;
    auto r = g.cfg.parse(keys);
    if (!r.ok())
    {
        string where = r.position < toks.length
            ? "unexpected " ~ quote_string(toks[r.position].text) ~ " at token " ~ to!string(r.position + 1)
            : "unexpected end of input";
        string exp;
        foreach (e; r.expected) exp ~= (exp.length ? ", " : "") ~ show_key(e);
        return err("YACCBridge: " ~ where ~ (exp.length ? "; expected " ~ exp : ""));
    }
    size_t next = 0;
    auto tree = r.tree.get;
    return ok(convert(tree, toks, next));
}

void installYACCBridge(Session s)
{
    declare_ast_type(s);
    prim(s, "YACCBridge.check", "String -> Result [String] String", 1, delegate Value(C c, V a) {
        try return ok(strings(compile(S(a[0])).nonterminals));
        catch (GrammarFail f) return err(f.msg);
    });
    prim(s, "YACCBridge.literals", "String -> Result [String] String", 1, delegate Value(C c, V a) {
        try return ok(strings(compile(S(a[0])).literals));
        catch (GrammarFail f) return err(f.msg);
    });
    prim(s, "YACCBridge.parse", "String -> String -> Result Ast String", 2, delegate Value(C c, V a) {
        try
        {
            auto g = compile(S(a[0]));
            return run(g, lex(g, S(a[1])));
        }
        catch (GrammarFail f) return err(f.msg);
    });
    prim(s, "YACCBridge.parse_tokens", "String -> [(String, String)] -> Result Ast String", 2,
        delegate Value(C c, V a) {
            try
            {
                auto g = compile(S(a[0]));
                Tok[] toks;
                foreach (t; items(a[1])) toks ~= Tok(S(fst(t)), S(snd(t)));
                return run(g, toks);
            }
            catch (GrammarFail f) return err(f.msg);
        });
}

