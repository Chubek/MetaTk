/**
 * metalib/d/Frontend.d — MetaML host module `Frontend` (D dialect): the MetaML lexer, parser and checker as library functions.
 *
 * Primitives (identical names, types and behaviour in metalib/cpp/Frontend.h):
 *   Frontend.tokens : String -> Result [(String, String)] String
 *   Frontend.parse : String -> Result String String
 *   Frontend.parse_expr : String -> Result String String
 *   Frontend.type_of : String -> Result String String
 *   Frontend.format_type : String -> Result String String
 *   Frontend.eval : String -> Result String String
 *   Frontend.keywords : Unit -> [String]
 *
 *   `eval` runs in a fresh sandbox session (core primitives only, no metalib);
 *   its output is forwarded to the calling session.
 */
module metalib.d.Frontend;

import MetaML;
import metalib.d.Support;

import DSLtk : Diagnostic, ast_to_sexpr;
import std.conv : to;

/// "error[M0201]: message (line 1, column 5)"
string diag_text(Diagnostic d)
{
    return d.headline() ~ " (line " ~ to!string(d.primary_span.line) ~ ", column "
        ~ to!string(d.primary_span.column) ~ ")";
}
string first_diag(Diagnostic[] ds) { return ds.length ? diag_text(ds[0]) : "error: failed"; }

private string tok_name(Tok k)
{
    final switch (k)
    {
        case Tok.Eof: return "eof";
        case Tok.Int: return "int";
        case Tok.Float: return "float";
        case Tok.Str: return "string";
        case Tok.LIdent: return "lident";
        case Tok.UIdent: return "uident";
        case Tok.TyVar: return "tyvar";
        case Tok.Tacit: return "tacit";
        case Tok.Hole: return "hole";
        case Tok.Directive: return "directive";
        case Tok.Op: return "op";
        case Tok.Kw: return "keyword";
        case Tok.LParen: return "lparen";
        case Tok.RParen: return "rparen";
        case Tok.LBracket: return "lbracket";
        case Tok.RBracket: return "rbracket";
        case Tok.LBrace: return "lbrace";
        case Tok.RBrace: return "rbrace";
        case Tok.Comma: return "comma";
        case Tok.Semi: return "semi";
        case Tok.Dot: return "dot";
    }
}

/// Resolves a type expression in the session's global scope and returns it
/// generalized; the unifier is left untouched.
Value format_type(Session s, string src)
{
    auto te = parse_type_expr(src, "<type>");
    if (te.is_err()) return err(first_diag(te.unwrap_err()));
    auto ck = s.checker();
    auto snap = ck.u.snapshot();
    try
    {
        auto m = new Checker.TVarMap;
        ++ck.level;
        auto t = ck.resolve_type(te.unwrap(), ck.global, m);
        --ck.level;
        ck.generalize(t, ck.level);
        auto result = show_type(t);
        ck.u.rollback(snap);
        return ok(make_str(result));
    }
    catch (MetaError e)
    {
        --ck.level;
        ck.u.rollback(snap);
        return err(diag_text(e.diag));
    }
}

void installFrontend(Session s)
{
    prim(s, "Frontend.tokens", "String -> Result [(String, String)] String", 1, delegate Value(C c, V a) {
        auto lx = lex(S(a[0]), "<frontend>");
        if (lx.diagnostics.length) return err(first_diag(lx.diagnostics));
        Value[] result;
        foreach (t; lx.tokens) if (t.kind != Tok.Eof) result ~= pair(str(tok_name(t.kind)), str(t.text));
        return ok(list(result));
    });
    prim(s, "Frontend.parse", "String -> Result String String", 1, delegate Value(C c, V a) {
        auto p = parse(S(a[0]), "<frontend>");
        if (!p.ok()) return err(first_diag(p.diagnostics));
        return ok(str(dump_ast(p.program)));
    });
    prim(s, "Frontend.parse_expr", "String -> Result String String", 1, delegate Value(C c, V a) {
        auto e = parse_expression(S(a[0]), "<frontend>");
        if (e.is_err()) return err(first_diag(e.unwrap_err()));
        return ok(str(ast_to_sexpr(dump_expr(e.unwrap())).dump()));
    });
    prim(s, "Frontend.type_of", "String -> Result String String", 1, delegate Value(C c, V a) {
        auto r = c.session.type_of(S(a[0]));
        if (r.is_err()) return err(first_diag(r.unwrap_err()));
        return ok(str(r.unwrap()));
    });
    prim(s, "Frontend.format_type", "String -> Result String String", 1,
        delegate Value(C c, V a) => format_type(c.session, S(a[0])));
    prim(s, "Frontend.eval", "String -> Result String String", 1, delegate Value(C c, V a) {
        auto sandbox = new Session();
        sandbox.allow_all_primitives();
        sandbox.set_output(delegate void(string t) { c.print(t); });
        auto r = sandbox.eval(S(a[0]), "<frontend>");
        if (r.is_err()) return err(first_diag(r.unwrap_err()));
        return ok(str(show_value(r.unwrap())));
    });
    prim(s, "Frontend.keywords", "Unit -> [String]", 1, delegate Value(C c, V a) {
        return strings(["module", "type", "sig", "struct", "end", "val", "let", "rec", "in", "if",
            "then", "else", "match", "with", "fn", "fun", "import", "include", "open", "do", "true",
            "false", "when", "for", "and", "of"]);
    });
}

