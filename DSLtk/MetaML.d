/**
 * MetaML — an embeddable dialect of ML (D dialect).
 *
 * MetaML is a functional / tacit / concatenative / modular language built on
 * DSLtk.d. This module is the complete D dialect: lexer, parser, AST,
 * Hindley–Milner type checker (row-polymorphic records, numeric constraints,
 * ML modules), evaluator, core host primitives, the embedding Session and the
 * REPL Language policy for DSLtk.Session.
 *
 * Its twin is MetaML.hpp; both MUST behave identically (AGENTS.md §9). Every
 * algorithm here mirrors MetaML.hpp function for function.
 *
 * ---
 * import MetaML;
 * void main() {
 *     auto s = new Session();
 *     s.loadStdlib();
 *     s.eval("val x := 1 + 2");
 *     auto v = s.eval("x * 10");
 *     writeln(to_string(v));   // 30
 * }
 * ---
 *
 * Exceptions are used internally only; the embedding API never throws
 * (results are returned as DSLtk Result values).
 */
module MetaML;

import DSLtk;

import std.algorithm : min, max, sort, SwapStrategy;
import std.array : array, replicate;
import std.conv : to;
import std.string : indexOf;
import std.typecons : Nullable, nullable, Tuple, tuple;
import core.stdc.stdio : snprintf;
import core.stdc.stdlib : strtod;
import core.stdc.math : c_floor = floor, c_ceil = ceil, c_round = round, c_sqrt = sqrt,
    c_exp = exp, c_log = log, c_sin = sin, c_cos = cos, c_tan = tan, c_fabs = fabs,
    c_pow = pow, c_atan2 = atan2, c_fmod = fmod;

enum string metamlVersion = "1.0.0";
enum string dialect = "d";

// ============================================================
//  Utilities shared by every stage (mirrored in MetaML.hpp)
// ============================================================

private bool isNaNd(double d) { return d != d; }
private bool isInfd(double d) { return d == double.infinity || d == -double.infinity; }
private bool signbitd(double d)
{
    ulong bits = *cast(ulong*) &d;
    return (bits >> 63) != 0;
}

/// Deterministic shortest round-trip float rendering (identical in MetaML.hpp).
string format_float(double d)
{
    if (isNaNd(d)) return "nan";
    if (isInfd(d)) return d < 0 ? "-inf" : "inf";
    if (d == 0.0) return signbitd(d) ? "-0.0" : "0.0";
    char[64] buf;
    int prec = 1;
    for (; prec <= 17; ++prec)
    {
        snprintf(buf.ptr, buf.length, "%.*e", prec - 1, d);
        if (strtod(buf.ptr, null) == d) break;
    }
    size_t n = 0;
    while (n < buf.length && buf[n] != 0) ++n;
    string s = buf[0 .. n].idup;
    bool neg = false;
    if (s.length && s[0] == '-') { neg = true; s = s[1 .. $]; }
    auto epos = s.indexOf('e');
    string mant = s[0 .. epos];
    string es = s[epos + 1 .. $];
    int exp = 0;
    bool eneg = false;
    size_t k = 0;
    if (k < es.length && (es[k] == '+' || es[k] == '-')) { eneg = es[k] == '-'; ++k; }
    for (; k < es.length; ++k) exp = exp * 10 + (es[k] - '0');
    if (eneg) exp = -exp;
    string digits;
    foreach (c; mant) if (c != '.') digits ~= c;
    while (digits.length > 1 && digits[$ - 1] == '0') digits = digits[0 .. $ - 1];
    string result;
    if (exp >= -5 && exp < 17)
    {
        if (exp >= 0)
        {
            size_t intlen = cast(size_t) exp + 1;
            if (digits.length <= intlen)
                result = digits ~ replicate("0", intlen - digits.length) ~ ".0";
            else
                result = digits[0 .. intlen] ~ "." ~ digits[intlen .. $];
        }
        else
            result = "0." ~ replicate("0", cast(size_t)(-exp - 1)) ~ digits;
    }
    else
    {
        result = digits[0 .. 1];
        if (digits.length > 1) result ~= "." ~ digits[1 .. $];
        result ~= "e" ~ to!string(exp);
    }
    return neg ? "-" ~ result : result;
}

/// Quotes a string with MetaML escapes.
string quote_string(string s)
{
    enum hex = "0123456789abcdef";
    string result = "\"";
    foreach (char ch; s)
    {
        ubyte c = cast(ubyte) ch;
        switch (c)
        {
            case '\n': result ~= "\\n"; break;
            case '\t': result ~= "\\t"; break;
            case '\r': result ~= "\\r"; break;
            case '"': result ~= "\\\""; break;
            case '\\': result ~= "\\\\"; break;
            default:
                if (c < 0x20 || c == 0x7f)
                {
                    result ~= "\\u{";
                    result ~= hex[c >> 4];
                    result ~= hex[c & 15];
                    result ~= "}";
                }
                else result ~= ch;
        }
    }
    return result ~ "\"";
}

/// Appends the UTF-8 encoding of a code point.
bool append_utf8(ref string result, uint cp)
{
    if (cp < 0x80) result ~= cast(char) cp;
    else if (cp < 0x800)
    {
        result ~= cast(char)(0xC0 | (cp >> 6));
        result ~= cast(char)(0x80 | (cp & 0x3F));
    }
    else if (cp < 0x10000)
    {
        if (cp >= 0xD800 && cp <= 0xDFFF) return false;
        result ~= cast(char)(0xE0 | (cp >> 12));
        result ~= cast(char)(0x80 | ((cp >> 6) & 0x3F));
        result ~= cast(char)(0x80 | (cp & 0x3F));
    }
    else if (cp <= 0x10FFFF)
    {
        result ~= cast(char)(0xF0 | (cp >> 18));
        result ~= cast(char)(0x80 | ((cp >> 12) & 0x3F));
        result ~= cast(char)(0x80 | ((cp >> 6) & 0x3F));
        result ~= cast(char)(0x80 | (cp & 0x3F));
    }
    else return false;
    return true;
}

long wrap_add(long a, long b) { return cast(long)(cast(ulong) a + cast(ulong) b); }
long wrap_sub(long a, long b) { return cast(long)(cast(ulong) a - cast(ulong) b); }
long wrap_mul(long a, long b) { return cast(long)(cast(ulong) a * cast(ulong) b); }
long wrap_neg(long a) { return cast(long)(0UL - cast(ulong) a); }

/// Saturating float -> int conversion (NaN -> 0).
long float_to_int(double d)
{
    if (isNaNd(d)) return 0;
    if (d >= 9223372036854775807.0) return long.max;
    if (d <= -9223372036854775808.0) return long.min;
    return cast(long) d;
}

/// Parses an Int literal body (decimal, 0x, 0b, underscores, optional sign).
Nullable!long parse_int(string s)
{
    bool neg = false;
    size_t i = 0;
    if (i < s.length && (s[i] == '-' || s[i] == '+' || s[i] == '~')) { neg = s[i] != '+'; ++i; }
    int base = 10;
    if (i + 1 < s.length && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) { base = 16; i += 2; }
    else if (i + 1 < s.length && s[i] == '0' && (s[i + 1] == 'b' || s[i + 1] == 'B')) { base = 2; i += 2; }
    if (i >= s.length) return Nullable!long.init;
    ulong v = 0;
    ulong limit = neg ? 9223372036854775808UL : 9223372036854775807UL;
    bool any = false;
    for (; i < s.length; ++i)
    {
        char c = s[i];
        if (c == '_' && any && i + 1 < s.length && s[i - 1] != '_')
        {
            char next = s[i + 1];
            int d = next >= '0' && next <= '9' ? next - '0'
                : next >= 'a' && next <= 'f' ? next - 'a' + 10
                : next >= 'A' && next <= 'F' ? next - 'A' + 10 : base;
            if (d < base) continue;
        }
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return Nullable!long.init;
        if (d >= base) return Nullable!long.init;
        if (v > (limit - cast(ulong) d) / cast(ulong) base) return Nullable!long.init;
        v = v * cast(ulong) base + cast(ulong) d;
        any = true;
    }
    if (!any) return Nullable!long.init;
    if (neg) return nullable(cast(long)(0UL - v));
    return nullable(cast(long) v);
}

/// Strict float parse: whole string, no leading whitespace.
Nullable!double parse_float(string s)
{
    if (!s.length || s[0] == ' ' || s[0] == '\t' || s[0] == '\n' || s[0] == '\r'
        || s[0] == '\v' || s[0] == '\f')
        return Nullable!double.init;
    char[] t;
    foreach (i, c; s)
        if (c != '_') t ~= c;
        else if (i == 0 || i + 1 == s.length || s[i - 1] < '0' || s[i - 1] > '9'
            || s[i + 1] < '0' || s[i + 1] > '9') return Nullable!double.init;
    if (t.length && t[0] == '~') t[0] = '-';
    t ~= '\0';
    char* end;
    double d = strtod(t.ptr, &end);
    if (end == t.ptr || end != t.ptr + (t.length - 1)) return Nullable!double.init;
    return nullable(d);
}

bool is_lower(char c) { return (c >= 'a' && c <= 'z') || c == '_'; }
bool is_upper(char c) { return c >= 'A' && c <= 'Z'; }
bool is_digit(char c) { return c >= '0' && c <= '9'; }
bool is_ident_char(char c) { return is_lower(c) || is_upper(c) || is_digit(c) || c == '\''; }
bool is_op_char(char c)
{
    switch (c)
    {
        case '!': case '$': case '%': case '&': case '*': case '+': case '-': case '.':
        case '/': case ':': case '<': case '=': case '>': case '?': case '@': case '^':
        case '|': case '~':
            return true;
        default:
            return false;
    }
}

string join_strings(const(string)[] parts, string sep)
{
    string result;
    foreach (i, p; parts) { if (i) result ~= sep; result ~= p; }
    return result;
}

/// Thrown internally; never escapes the embedding API.
class MetaError : Exception
{
    Diagnostic diag;
    this(Diagnostic d) { super(d.message); diag = d; }
}

Diagnostic make_diag(string code, string message, Span span, Severity sev = Severity.Error)
{
    Diagnostic d;
    d.severity = sev;
    d.code = code;
    d.message = message;
    d.primary_span = span;
    return d;
}

noreturn fail(string code, string message, Span span = Span.init)
{
    throw new MetaError(make_diag(code, message, span));
}

// ============================================================
//  Lexer (tokens are DSLtk BasicToken; scanners use DSLtk combinators)
// ============================================================

enum Tok
{
    Eof, Int, Float, Str, LIdent, UIdent, TyVar, Tacit, Hole, Directive, Op, Kw,
    LParen, RParen, LBracket, RBracket, LBrace, RBrace, Comma, Semi, Dot
}

/// A MetaML token: a DSLtk token plus decoded literal payloads.
struct MToken
{
    BasicToken!Tok base;
    alias base this;
    long ival;
    double fval = 0.0;
    string sval;
}

bool is_keyword(string s)
{
    static immutable kws = ["module", "type", "sig", "struct", "end", "val", "let", "rec", "in",
        "if", "then", "else", "match", "with", "fn", "fun", "import", "include", "open", "do",
        "true", "false", "when", "for", "and", "of"];
    foreach (k; kws) if (s == k) return true;
    return false;
}

private bool isHexDigit(char c) { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
private bool isHexDigitU(char c) { return isHexDigit(c) || c == '_'; }
private bool isBinDigit(char c) { return c == '0' || c == '1'; }
private bool isBinDigitU(char c) { return c == '0' || c == '1' || c == '_'; }
private bool isDigitU(char c) { return is_digit(c) || c == '_'; }

/// DSLtk combinator scanners; each returns the end offset of a match.
private struct Scanners
{
    static Nullable!size_t run(P)(P p, string src, size_t pos)
    {
        auto input = ParsecInput(src, pos);
        auto r = p(input);
        if (!r) return Nullable!size_t.init;
        return nullable(input.pos);
    }
    static Nullable!size_t lower_ident(string src, size_t pos)
    {
        return run(satisfy(&is_lower, "lowercase identifier") & satisfy(&is_ident_char, "identifier character").many(), src, pos);
    }
    static Nullable!size_t upper_ident(string src, size_t pos)
    {
        return run(satisfy(&is_upper, "uppercase identifier") & satisfy(&is_ident_char, "identifier character").many(), src, pos);
    }
    static Nullable!size_t op_run(string src, size_t pos)
    {
        return run(satisfy(&is_op_char, "operator character") & satisfy(&is_op_char, "operator character").many(), src, pos);
    }
    static Nullable!size_t hex_lit(string src, size_t pos)
    {
        return run((ch('0') & (ch('x') | ch('X'))) & (satisfy(&isHexDigit, "hex digit") & satisfy(&isHexDigitU, "hex digit").many()), src, pos);
    }
    static Nullable!size_t bin_lit(string src, size_t pos)
    {
        return run((ch('0') & (ch('b') | ch('B'))) & (satisfy(&isBinDigit, "binary digit") & satisfy(&isBinDigitU, "binary digit").many()), src, pos);
    }
    static Nullable!size_t digits(string src, size_t pos)
    {
        return run(satisfy(&is_digit, "digit") & satisfy(&isDigitU, "digit").many(), src, pos);
    }
    static Nullable!size_t fraction(string src, size_t pos)
    {
        return run(try_parse(ch('.') & (satisfy(&is_digit, "digit") & satisfy(&isDigitU, "digit").many())), src, pos);
    }
    static Nullable!size_t exponent(string src, size_t pos)
    {
        return run(try_parse(((ch('e') | ch('E')) & optional(ch('+') | ch('-')))
            & (satisfy(&is_digit, "digit") & satisfy(&isDigitU, "digit").many())), src, pos);
    }
}

bool value_end(ref const MToken t)
{
    switch (t.kind)
    {
        case Tok.Int: case Tok.Float: case Tok.Str: case Tok.LIdent: case Tok.UIdent:
        case Tok.Tacit: case Tok.Hole: case Tok.RParen: case Tok.RBracket: case Tok.RBrace:
            return true;
        case Tok.Kw:
            return t.text == "true" || t.text == "false";
        default:
            return false;
    }
}

struct LexResult
{
    MToken[] tokens;
    Diagnostic[] diagnostics;
    bool incomplete;
}

/// Tokenizes MetaML source. Never throws.
LexResult lex(string src, string file = "<input>")
{
    LexResult res;
    size_t i = 0, line = 1, line_start = 0;
    bool space = false, newline = true;

    Span span_at(size_t b, size_t e)
    {
        Span s;
        s.begin = b; s.end = e; s.line = line; s.column = b - line_start + 1; s.file = file;
        return s;
    }
    void error(string code, string msg, size_t b, size_t e)
    {
        res.diagnostics ~= make_diag(code, msg, span_at(b, e));
    }
    MToken* push(Tok k, size_t b, size_t e)
    {
        MToken t;
        t.kind = k;
        t.text = src[b .. e];
        t.span = span_at(b, e);
        t.trivia.space_before = space;
        t.trivia.newline_before = newline;
        res.tokens ~= t;
        space = false;
        newline = false;
        return &res.tokens[$ - 1];
    }
    void advance_lines(size_t from, size_t to)
    {
        for (size_t k = from; k < to; ++k)
            if (src[k] == '\n') { ++line; line_start = k + 1; }
    }

    while (i < src.length)
    {
        char c = src[i];
        if (c == '\n') { ++i; ++line; line_start = i; space = true; newline = true; continue; }
        if (c == ' ' || c == '\t' || c == '\r') { ++i; space = true; continue; }
        char n1 = i + 1 < src.length ? src[i + 1] : '\0';
        if (c == '-' && n1 == '-')
        {
            while (i < src.length && src[i] != '\n') ++i;
            space = true;
            continue;
        }
        if (c == '{' && n1 == '-')
        {
            size_t start = i;
            size_t start_line = line, start_ls = line_start;
            int depth = 0;
            bool closed = false;
            while (i < src.length)
            {
                if (src[i] == '{' && i + 1 < src.length && src[i + 1] == '-') { ++depth; i += 2; continue; }
                if (src[i] == '-' && i + 1 < src.length && src[i + 1] == '}')
                {
                    --depth;
                    i += 2;
                    if (depth == 0) { closed = true; break; }
                    continue;
                }
                if (src[i] == '\n') { ++line; line_start = i + 1; newline = true; }
                ++i;
            }
            space = true;
            if (!closed)
            {
                size_t save_line = line, save_ls = line_start;
                line = start_line;
                line_start = start_ls;
                error("M0104", "unterminated block comment", start, start + 2);
                line = save_line;
                line_start = save_ls;
                res.incomplete = true;
            }
            continue;
        }
        if (c == '#')
        {
            if (is_digit(n1))
            {
                size_t b = i++;
                while (i < src.length && is_digit(src[i])) ++i;
                auto t = push(Tok.Hole, b, i);
                auto index = parse_int(src[b + 1 .. i]);
                if (index.isNull || index.get > 1024)
                    error("M0105", "placeholder index exceeds the limit of 1024", b, i);
                t.ival = !index.isNull && index.get <= 1024 ? index.get : 0;
                continue;
            }
            if (is_lower(n1) || is_upper(n1))
            {
                size_t b = i++;
                while (i < src.length && is_ident_char(src[i])) ++i;
                size_t name_end = i;
                size_t e = i;
                while (e < src.length && src[e] != '\n') ++e;
                auto t = push(Tok.Directive, b, name_end);
                string arg = src[name_end .. e];
                while (arg.length && (arg[0] == ' ' || arg[0] == '\t')) arg = arg[1 .. $];
                while (arg.length && (arg[$ - 1] == ' ' || arg[$ - 1] == '\t' || arg[$ - 1] == '\r'))
                    arg = arg[0 .. $ - 1];
                t.sval = arg;
                t.text = src[b + 1 .. name_end];
                i = e;
                continue;
            }
            while (i < src.length && src[i] != '\n') ++i;
            space = true;
            continue;
        }
        size_t b = i;
        if (c == '"' || (c == 'r' && n1 == '"'))
        {
            bool raw = c == 'r';
            i += raw ? 2 : 1;
            string val;
            bool closed = false;
            size_t lines_from = i;
            while (i < src.length)
            {
                char d = src[i];
                if (d == '"') { ++i; closed = true; break; }
                if (d == '\\' && !raw)
                {
                    if (i + 1 >= src.length) { ++i; break; }
                    char e = src[i + 1];
                    i += 2;
                    switch (e)
                    {
                        case 'n': val ~= '\n'; break;
                        case 't': val ~= '\t'; break;
                        case 'r': val ~= '\r'; break;
                        case '0': val ~= '\0'; break;
                        case '"': val ~= '"'; break;
                        case '\\': val ~= '\\'; break;
                        case 'u':
                        {
                            uint cp = 0;
                            bool ok = i < src.length && src[i] == '{';
                            size_t j = i + 1;
                            int nd = 0;
                            while (ok && j < src.length && src[j] != '}')
                            {
                                char h = src[j];
                                int dv = is_digit(h) ? h - '0'
                                    : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                                    : (h >= 'A' && h <= 'F') ? h - 'A' + 10 : -1;
                                if (dv < 0 || nd >= 6) { ok = false; break; }
                                cp = cp * 16 + cast(uint) dv;
                                ++nd;
                                ++j;
                            }
                            ok = ok && j < src.length && nd > 0;
                            if (ok && append_utf8(val, cp)) i = j + 1;
                            else error("M0103", "invalid unicode escape; expected `\\u{XXXX}`", i - 2, i);
                            break;
                        }
                        default:
                            error("M0103", "invalid escape sequence `\\" ~ e ~ "`", i - 2, i);
                    }
                    continue;
                }
                val ~= d;
                ++i;
            }
            if (!closed)
            {
                error("M0102", "unterminated string literal", b, b + 1);
                res.incomplete = true;
                advance_lines(lines_from, i);
                break;
            }
            auto t = push(Tok.Str, b, i);
            t.sval = val;
            advance_lines(lines_from, i);
            continue;
        }
        bool neg_lit = (c == '-' || c == '~') && is_digit(n1)
            && (!res.tokens.length || !value_end(res.tokens[$ - 1]) || c == '~');
        if (is_digit(c) || neg_lit)
        {
            size_t start = neg_lit ? i + 1 : i;
            Nullable!size_t e;
            bool is_float = false;
            e = Scanners.hex_lit(src, start);
            if (e.isNull) e = Scanners.bin_lit(src, start);
            if (e.isNull)
            {
                e = Scanners.digits(src, start);
                size_t p = e.get;
                auto f = Scanners.fraction(src, p);
                if (!f.isNull) { p = f.get; is_float = true; }
                auto x = Scanners.exponent(src, p);
                if (!x.isNull) { p = x.get; is_float = true; }
                e = nullable(p);
            }
            i = e.get;
            string body = src[b .. i];
            if (is_float)
            {
                auto v = parse_float(body);
                if (v.isNull) error("M0105", "invalid float literal `" ~ body ~ "`", b, i);
                auto t = push(Tok.Float, b, i);
                t.fval = v.isNull ? 0.0 : v.get;
            }
            else
            {
                auto v = parse_int(body);
                if (v.isNull) error("M0105", "invalid or out-of-range integer literal `" ~ body ~ "`", b, i);
                auto t = push(Tok.Int, b, i);
                t.ival = v.isNull ? 0 : v.get;
            }
            if (i < src.length && is_ident_char(src[i]))
            {
                size_t j = i;
                while (j < src.length && is_ident_char(src[j])) ++j;
                error("M0105", "invalid numeric literal `" ~ src[b .. j] ~ "`", b, j);
                i = j;
            }
            continue;
        }
        if (c == '$' && is_digit(n1))
        {
            ++i;
            while (i < src.length && is_digit(src[i])) ++i;
            auto t = push(Tok.Tacit, b, i);
            auto index = parse_int(src[b + 1 .. i]);
            if (index.isNull || index.get > 1024)
                error("M0105", "placeholder index exceeds the limit of 1024", b, i);
            t.ival = !index.isNull && index.get <= 1024 ? index.get : 0;
            continue;
        }
        if (c == '\'' && is_lower(n1))
        {
            ++i;
            while (i < src.length && is_ident_char(src[i])) ++i;
            push(Tok.TyVar, b, i);
            continue;
        }
        if (is_lower(c))
        {
            i = Scanners.lower_ident(src, i).get;
            push(is_keyword(src[b .. i]) ? Tok.Kw : Tok.LIdent, b, i);
            continue;
        }
        if (is_upper(c))
        {
            i = Scanners.upper_ident(src, i).get;
            push(Tok.UIdent, b, i);
            continue;
        }
        switch (c)
        {
            case '(': push(Tok.LParen, b, ++i); continue;
            case ')': push(Tok.RParen, b, ++i); continue;
            case '[': push(Tok.LBracket, b, ++i); continue;
            case ']': push(Tok.RBracket, b, ++i); continue;
            case '{': push(Tok.LBrace, b, ++i); continue;
            case '}': push(Tok.RBrace, b, ++i); continue;
            case ',': push(Tok.Comma, b, ++i); continue;
            case ';': push(Tok.Semi, b, ++i); continue;
            default: break;
        }
        if (c == '.' && n1 != '.') { push(Tok.Dot, b, ++i); continue; }
        if (cast(ubyte) c == 0xE2 && i + 2 < src.length && cast(ubyte) src[i + 1] == 0x88
            && cast(ubyte) src[i + 2] == 0x98)
        {
            i += 3;
            push(Tok.Op, b, i);
            continue;
        }
        if (is_op_char(c))
        {
            size_t e = Scanners.op_run(src, i).get;
            for (size_t k = i + 1; k < e; ++k)
                if ((src[k] == '$' && k + 1 < src.length && is_digit(src[k + 1]))
                    || (src[k] == '-' && k + 1 < e && src[k + 1] == '-'))
                {
                    e = k;
                    break;
                }
            i = e;
            push(Tok.Op, b, i);
            continue;
        }
        size_t e = i + 1;
        while (e < src.length && (cast(ubyte) src[e] & 0xC0) == 0x80) ++e;
        error("M0101", "unexpected character `" ~ src[i .. e] ~ "`", i, e);
        i = e;
    }
    MToken eof;
    eof.kind = Tok.Eof;
    eof.span = span_at(src.length, src.length);
    eof.trivia.newline_before = true;
    eof.trivia.space_before = true;
    res.tokens ~= eof;
    return res;
}

// ============================================================
//  AST (MetaML extensions over the DSLtk AST; dumped via ASTNode)
// ============================================================

/// Base of every MetaML AST node: a DSLtk span.
abstract class NodeBase
{
    Span span;
}

enum EK
{
    Int, Float, Str, Bool, Unit, Var, Path, Ctor, Tacit, App, Pipe, BinOp, Neg, Lambda, If,
    Match, Let, LetRec, List, Comp, Range, Tuple, Record, Update, Project, OpRef, Annot
}

/// How the type checker resolved a juxtaposition / pipeline (§5.4).
enum Resolution { Apply = 0, ComposeFwd = 1, ComposeBwd = 2 }

struct CompClause
{
    Pat pat;
    Expr expr;
}

final class Expr : NodeBase
{
    EK kind = EK.Unit;
    long ival;
    double fval = 0.0;
    string text;
    string[] path;
    Expr[] kids;
    Pat[] pats;
    Expr[] guards;
    string[] labels;
    CompClause[] clauses;
    TypeExpr annot;
    string name;
    int tacit_count;
    // ---- annotations written by the type checker ----
    Resolution resolution = Resolution.Apply;
    Cell cell;
    Primitive prim;
    int ctor_index = -1;
    int ctor_arity;
}

enum PK { Wild, Var, Int, Float, Str, Bool, Unit, Tuple, List, Cons, Ctor, Record, Annot }

final class Pat : NodeBase
{
    PK kind = PK.Wild;
    long ival;
    double fval = 0.0;
    string text;
    string[] path;
    Pat[] kids;
    string[] labels;
    bool open;
    TypeExpr annot;
    int ctor_index = -1;
    int ctor_arity;
}

enum TEK { Var, Name, Fun, Tuple, List, Record, Unit }

final class TypeExpr : NodeBase
{
    TEK kind = TEK.Unit;
    string name;
    string[] path;
    TypeExpr[] kids;
    string[] labels;
    string row;
    string[] num_vars;
}

enum IK { Sig, Def, Type, Module, Functor, ModType, Import, Include, Open, Expr, Directive }

struct CtorDecl
{
    string name;
    TypeExpr[] args;
    Span span;
}

/// Ascription kind for module definitions.
enum Asc { None = 0, Transparent = 1, Opaque = 2 }

final class Item : NodeBase
{
    IK kind = IK.Expr;
    string name;
    TypeExpr type;
    Pat[] params;
    Expr body;
    Expr value;
    bool tacit;
    string[] tparams;
    int tkind;
    CtorDecl[] ctors;
    Asc asc = Asc.None;
    SigExpr sig;
    ModExpr mexpr;
    string pname;
    SigExpr psig;
    string[] path;
    string text;
    // ---- annotations written by the type checker ----
    bool sealed;
    string[] visible_values;
    string[] visible_modules;
    Cell[] cells;
}

enum MK { Struct, Path, Apply }

final class ModExpr : NodeBase
{
    MK kind = MK.Struct;
    Item[] items;
    string[] path;
    ModExpr arg;
}

final class SigExpr : NodeBase
{
    bool named;
    Item[] items;
    string[] path;
}

struct Program
{
    Item[] items;
    string file;
    string source;
}

// ---- AST dump (S-expressions through DSLtk ASTNode / SExpr) ----

private ASTNode leafn(string tag, string v) { return ASTNode(tag, v); }
private ASTNode nodn(string tag, ASTNode[] kids) { return ASTNode(tag, kids); }

private ASTNode dump_type(TypeExpr t)
{
    if (t is null) return leafn("none", "");
    ASTNode[] k;
    final switch (t.kind)
    {
        case TEK.Var: return leafn("tvar", t.name);
        case TEK.Unit: return leafn("tunit", "");
        case TEK.Name:
        {
            string[] p = t.path.dup;
            p ~= t.name;
            k ~= leafn("name", join_strings(p, "."));
            foreach (a; t.kids) k ~= dump_type(a);
            return nodn("tcon", k);
        }
        case TEK.Fun: return nodn("tfun", [dump_type(t.kids[0]), dump_type(t.kids[1])]);
        case TEK.Tuple:
            foreach (a; t.kids) k ~= dump_type(a);
            return nodn("ttuple", k);
        case TEK.List: return nodn("tlist", [dump_type(t.kids[0])]);
        case TEK.Record:
            foreach (i; 0 .. t.kids.length)
                k ~= nodn("tfield", [leafn("label", t.labels[i]), dump_type(t.kids[i])]);
            if (t.row.length) k ~= leafn("row", t.row);
            return nodn("trecord", k);
    }
}

private ASTNode dump_pat(Pat p)
{
    ASTNode[] k;
    final switch (p.kind)
    {
        case PK.Wild: return leafn("pwild", "");
        case PK.Var: return leafn("pvar", p.text);
        case PK.Int: return leafn("pint", to!string(p.ival));
        case PK.Float: return leafn("pfloat", format_float(p.fval));
        case PK.Str: return leafn("pstr", p.text);
        case PK.Bool: return leafn("pbool", p.ival ? "true" : "false");
        case PK.Unit: return leafn("punit", "");
        case PK.Tuple:
            foreach (x; p.kids) k ~= dump_pat(x);
            return nodn("ptuple", k);
        case PK.List:
            foreach (x; p.kids) k ~= dump_pat(x);
            return nodn("plist", k);
        case PK.Cons: return nodn("pcons", [dump_pat(p.kids[0]), dump_pat(p.kids[1])]);
        case PK.Ctor:
        {
            string[] q = p.path.dup;
            q ~= p.text;
            k ~= leafn("name", join_strings(q, "."));
            foreach (x; p.kids) k ~= dump_pat(x);
            return nodn("pctor", k);
        }
        case PK.Record:
            foreach (i; 0 .. p.kids.length)
                k ~= nodn("pfield", [leafn("label", p.labels[i]), dump_pat(p.kids[i])]);
            if (p.open) k ~= leafn("open", "");
            return nodn("precord", k);
        case PK.Annot: return nodn("pannot", [dump_pat(p.kids[0]), dump_type(p.annot)]);
    }
}

ASTNode dump_expr(Expr e)
{
    ASTNode[] k;
    final switch (e.kind)
    {
        case EK.Int: return leafn("int", to!string(e.ival));
        case EK.Float: return leafn("float", format_float(e.fval));
        case EK.Str: return leafn("str", e.text);
        case EK.Bool: return leafn("bool", e.ival ? "true" : "false");
        case EK.Unit: return leafn("unit", "");
        case EK.Var: return leafn("var", e.text);
        case EK.Path: return leafn("path", join_strings(e.path, "."));
        case EK.Ctor:
        {
            string[] q = e.path.dup;
            q ~= e.text;
            return leafn("ctor", join_strings(q, "."));
        }
        case EK.Tacit: return leafn("tacit", to!string(e.ival));
        case EK.App: return nodn("app", [dump_expr(e.kids[0]), dump_expr(e.kids[1])]);
        case EK.Pipe: return nodn("pipe", [dump_expr(e.kids[0]), dump_expr(e.kids[1])]);
        case EK.BinOp:
            return nodn("binop", [leafn("op", e.text), dump_expr(e.kids[0]), dump_expr(e.kids[1])]);
        case EK.Neg: return nodn("neg", [dump_expr(e.kids[0])]);
        case EK.Lambda:
        {
            ASTNode[] ps;
            foreach (p; e.pats) ps ~= dump_pat(p);
            k ~= nodn("params", ps);
            k ~= dump_expr(e.kids[0]);
            return nodn("fn", k);
        }
        case EK.If:
            return nodn("if", [dump_expr(e.kids[0]), dump_expr(e.kids[1]), dump_expr(e.kids[2])]);
        case EK.Match:
            k ~= dump_expr(e.kids[0]);
            foreach (i; 0 .. e.pats.length)
            {
                ASTNode[] b = [dump_pat(e.pats[i])];
                if (e.guards[i] !is null) b ~= nodn("when", [dump_expr(e.guards[i])]);
                b ~= dump_expr(e.kids[i + 1]);
                k ~= nodn("branch", b);
            }
            return nodn("match", k);
        case EK.Let:
            return nodn("let", [dump_pat(e.pats[0]), dump_expr(e.kids[0]), dump_expr(e.kids[1])]);
        case EK.LetRec:
            foreach (i; 0 .. e.pats.length)
                k ~= nodn("bind", [dump_pat(e.pats[i]), dump_expr(e.kids[i])]);
            k ~= dump_expr(e.kids[$ - 1]);
            return nodn("letrec", k);
        case EK.List:
            foreach (x; e.kids) k ~= dump_expr(x);
            return nodn("list", k);
        case EK.Comp:
            k ~= dump_expr(e.kids[0]);
            foreach (c; e.clauses)
            {
                if (c.pat !is null) k ~= nodn("gen", [dump_pat(c.pat), dump_expr(c.expr)]);
                else k ~= nodn("guard", [dump_expr(c.expr)]);
            }
            return nodn("comp", k);
        case EK.Range: return nodn("range", [dump_expr(e.kids[0]), dump_expr(e.kids[1])]);
        case EK.Tuple:
            foreach (x; e.kids) k ~= dump_expr(x);
            return nodn("tuple", k);
        case EK.Record:
            foreach (i; 0 .. e.kids.length)
                k ~= nodn("field", [leafn("label", e.labels[i]), dump_expr(e.kids[i])]);
            return nodn("record", k);
        case EK.Update:
            k ~= dump_expr(e.kids[0]);
            foreach (i; 0 .. e.labels.length)
                k ~= nodn("field", [leafn("label", e.labels[i]), dump_expr(e.kids[i + 1])]);
            return nodn("update", k);
        case EK.Project: return nodn("proj", [dump_expr(e.kids[0]), leafn("label", e.text)]);
        case EK.OpRef: return leafn("opref", e.text);
        case EK.Annot: return nodn("annot", [dump_expr(e.kids[0]), dump_type(e.annot)]);
    }
}

private ASTNode dump_mexpr(ModExpr m)
{
    ASTNode[] k;
    final switch (m.kind)
    {
        case MK.Struct:
            foreach (i; m.items) k ~= dump_item(i);
            return nodn("struct", k);
        case MK.Path: return leafn("mpath", join_strings(m.path, "."));
        case MK.Apply:
            return nodn("mapply", [leafn("functor", join_strings(m.path, ".")), dump_mexpr(m.arg)]);
    }
}

private ASTNode dump_sig(SigExpr s)
{
    if (s is null) return leafn("none", "");
    if (s.named) return leafn("signame", join_strings(s.path, "."));
    ASTNode[] k;
    foreach (i; s.items) k ~= dump_item(i);
    return nodn("sig", k);
}

private string asc_name(Asc a)
{
    return a == Asc.None ? "none" : a == Asc.Transparent ? ":" : ":>";
}

private ASTNode dump_item(Item it)
{
    ASTNode[] k;
    final switch (it.kind)
    {
        case IK.Sig: return nodn("Signature", [leafn("name", it.name), dump_type(it.type)]);
        case IK.Def:
            k ~= leafn("name", it.name);
            if (it.type !is null) k ~= dump_type(it.type);
            k ~= dump_expr(it.value);
            return nodn("FnDef", k);
        case IK.Type:
        {
            k ~= leafn("name", it.name);
            ASTNode[] ps;
            foreach (p; it.tparams) ps ~= leafn("tvar", p);
            k ~= nodn("tparams", ps);
            if (it.tkind == 1) k ~= dump_type(it.type);
            else if (it.tkind == 2)
                foreach (c; it.ctors)
                {
                    ASTNode[] cs = [leafn("name", c.name)];
                    foreach (a; c.args) cs ~= dump_type(a);
                    k ~= nodn("ctor", cs);
                }
            return nodn("TypeDef", k);
        }
        case IK.Module:
            return nodn("ModuleDef", [leafn("name", it.name), leafn("asc", asc_name(it.asc)),
                dump_sig(it.sig), dump_mexpr(it.mexpr)]);
        case IK.Functor:
            return nodn("FunctorDef", [leafn("name", it.name), leafn("param", it.pname),
                dump_sig(it.psig), leafn("asc", asc_name(it.asc)), dump_sig(it.sig), dump_mexpr(it.mexpr)]);
        case IK.ModType: return nodn("ModuleType", [leafn("name", it.name), dump_sig(it.sig)]);
        case IK.Import: return leafn("Import", it.text);
        case IK.Include: return leafn("Include", join_strings(it.path, "."));
        case IK.Open: return leafn("Open", join_strings(it.path, "."));
        case IK.Expr: return nodn("Expr", [dump_expr(it.body)]);
        case IK.Directive: return nodn("Directive", [leafn("name", it.name), leafn("arg", it.text)]);
    }
}

/// Canonical S-expression dump of a program (identical in MetaML.hpp).
string dump_ast(ref const Program prog)
{
    ASTNode[] items;
    foreach (i; prog.items) items ~= dump_item(cast(Item) i);
    return ast_to_sexpr(ASTNode("Program", items)).dump();
}

// ============================================================
//  Parser (recursive descent over DSLtk tokens; §5.9 grammar)
// ============================================================

struct ParseOutput
{
    Program program;
    Diagnostic[] diagnostics;
    bool incomplete;
    bool ok() const { return diagnostics.length == 0; }
}

class ParseFail : Exception
{
    Diagnostic diag;
    bool at_eof;
    this(Diagnostic d, bool eof) { super(d.message); diag = d; at_eof = eof; }
}

bool is_reserved_op(string op)
{
    return op == ":=" || op == "->" || op == "<-" || op == "=>" || op == "|" || op == ":"
        || op == ".." || op == "~" || op == ":>" || op == ".";
}

/// Binary operator precedence (higher binds tighter); -1 if not binary.
int op_prec(string op, ref bool right)
{
    right = false;
    if (!op.length || is_reserved_op(op)) return -1;
    if (op == "|>") return 0;
    if (op == ">>") return 1;
    if (op == "<<" || op == "\xE2\x88\x98") { right = true; return 1; }
    if (op == "||") { right = true; return 2; }
    if (op == "&&") { right = true; return 3; }
    if (op == "==" || op == "!=" || op == "=" || op == "<>" || op == "<" || op == ">"
        || op == "<=" || op == ">=")
        return 4;
    if (op == "::") { right = true; return 5; }
    if (op == "@" || op == "^") { right = true; return 6; }
    if (op == "+" || op == "-") return 7;
    if (op == "*" || op == "/" || op == "%") return 8;
    switch (op[0])
    {
        case '*': case '/': case '%': return 8;
        case '+': case '-': return 7;
        case '@': case '^': right = true; return 6;
        case ':': right = true; return 5;
        default: return 4;
    }
}

bool is_builtin_op(string op)
{
    static immutable ops = ["+", "-", "*", "/", "%", "==", "!=", "=", "<>", "<", ">", "<=", ">=",
        "&&", "||", "^", "@", "::", ">>", "<<", "\xE2\x88\x98", "|>"];
    foreach (o; ops) if (op == o) return true;
    return false;
}

final class ParserImpl
{
    private MToken[] toks_;
    private size_t p_;
    private string file_;
    private int depth_;
    private int[] holes_;

    this(MToken[] toks, string file) { toks_ = toks; file_ = file; }

    Item[] program()
    {
        Item[] items;
        while (!at(Tok.Eof)) items ~= item(false);
        return items;
    }

    Expr expression_only()
    {
        auto e = expr();
        if (!at(Tok.Eof)) error_here("M0202", "unexpected " ~ describe(peek()) ~ " after expression");
        check_no_tacit(e);
        return e;
    }

    TypeExpr type_only()
    {
        auto t = type();
        if (!at(Tok.Eof)) error_here("M0202", "unexpected " ~ describe(peek()) ~ " after type");
        return t;
    }

private:
    // ---- token helpers ----
    ref const(MToken) peek(size_t k = 0) const
    {
        size_t i = p_ + k;
        return i < toks_.length ? toks_[i] : toks_[$ - 1];
    }
    ref const(MToken) prev() const { return toks_[p_ > 0 ? p_ - 1 : 0]; }
    bool at(Tok k) const { return peek().kind == k; }
    bool at_op(string op, size_t k = 0) const { return peek(k).kind == Tok.Op && peek(k).text == op; }
    bool at_kw(string kw, size_t k = 0) const { return peek(k).kind == Tok.Kw && peek(k).text == kw; }
    ref const(MToken) next()
    {
        size_t i = p_;
        if (p_ + 1 < toks_.length) ++p_;
        return toks_[i];
    }
    static string describe(ref const MToken t)
    {
        switch (t.kind)
        {
            case Tok.Eof: return "end of input";
            case Tok.Str: return "string literal";
            case Tok.Int: case Tok.Float: return "number `" ~ t.text ~ "`";
            case Tok.Directive: return "directive `#" ~ t.text ~ "`";
            case Tok.Kw: return "keyword `" ~ t.text ~ "`";
            default: return "`" ~ t.text ~ "`";
        }
    }
    noreturn error_at(ref const MToken t, string code, string msg)
    {
        throw new ParseFail(make_diag(code, msg, t.span), t.kind == Tok.Eof);
    }
    noreturn error_here(string code, string msg) { error_at(peek(), code, msg); }
    noreturn unexpected(string wanted)
    {
        if (at(Tok.Directive))
            error_here("M0400", "directive `#" ~ peek().text
                ~ "` is only legal at the start of a top-level form");
        error_here("M0202", "expected " ~ wanted ~ " but found " ~ describe(peek()));
    }
    void expect(Tok k, string what) { if (!at(k)) unexpected(what); next(); }
    void expect_op(string op) { if (!at_op(op)) unexpected("`" ~ op ~ "`"); next(); }
    void expect_kw(string kw) { if (!at_kw(kw)) unexpected("`" ~ kw ~ "`"); next(); }
    Span span_from(Span start) const
    {
        Span s = start;
        auto last = prev();
        if (last.span.end > s.end) s.end = last.span.end;
        return s;
    }
    static Span merge_span(Span a, Span b) { return merge_spans(a, b); }
    Expr mk_expr(EK k, Span s)
    {
        auto e = new Expr;
        e.span = s;
        e.kind = k;
        return e;
    }
    Expr mk_var(string name, Span s)
    {
        auto e = mk_expr(EK.Var, s);
        e.text = name;
        return e;
    }
    Pat mk_pvar(string name, Span s)
    {
        auto p = new Pat;
        p.span = s;
        p.kind = PK.Var;
        p.text = name;
        return p;
    }
    Expr mk_app(Expr f, Expr x)
    {
        auto e = mk_expr(EK.App, merge_span(f.span, x.span));
        e.kids = [f, x];
        return e;
    }
    Expr mk_binop(string op, Expr a, Expr b)
    {
        auto e = mk_expr(op == "|>" ? EK.Pipe : EK.BinOp, merge_span(a.span, b.span));
        if (op != "|>") e.text = op;
        e.kids = [a, b];
        return e;
    }
    Expr mk_lambda(Pat[] params, Expr body, Span s)
    {
        auto e = mk_expr(EK.Lambda, s);
        e.pats = params;
        e.kids = [body];
        return e;
    }

    // ---- boundaries ----
    bool starts_atom(ref const MToken t) const
    {
        switch (t.kind)
        {
            case Tok.Int: case Tok.Float: case Tok.Str: case Tok.LIdent: case Tok.UIdent:
            case Tok.Tacit: case Tok.Hole: case Tok.LParen: case Tok.LBracket: case Tok.LBrace:
            case Tok.Directive:
                return true;
            case Tok.Kw:
                // keyword-led forms (fn, if, match, let) need parentheses as arguments
                return t.text == "true" || t.text == "false" || t.text == "do";
            default:
                return false;
        }
    }
    int def_head_at(size_t i, bool allow_eq) const
    {
        if (i >= toks_.length || toks_[i].kind != Tok.LIdent) return 0;
        size_t j = i + 1;
        if (j < toks_.length && toks_[j].kind == Tok.Op && toks_[j].text == "::") return 1;
        int d = 0;
        for (size_t n = 0; j < toks_.length && n < 256; ++j, ++n)
        {
            auto t = &toks_[j];
            if (d == 0 && t.kind == Tok.Op)
            {
                if (t.text == ":=") return 2;
                if (t.text == "=" && allow_eq) return 2;
                return 0;
            }
            switch (t.kind)
            {
                case Tok.LParen: case Tok.LBracket: case Tok.LBrace: ++d; continue;
                case Tok.RParen: case Tok.RBracket: case Tok.RBrace:
                    if (--d < 0) return 0;
                    continue;
                case Tok.Eof: return 0;
                case Tok.Kw:
                    if (d == 0) return 0;
                    continue;
                case Tok.LIdent: case Tok.UIdent: case Tok.Int: case Tok.Float: case Tok.Str:
                case Tok.TyVar: case Tok.Comma:
                    continue;
                case Tok.Op:
                    if (d > 0) continue;
                    return 0;
                default:
                    if (d > 0) continue;
                    return 0;
            }
        }
        return 0;
    }
    /// True when the current token begins a new top-level form on a new line.
    bool at_boundary() const
    {
        auto t = &peek();
        if (depth_ > 0 || !t.trivia.newline_before) return false;
        // a line that starts in column 1 begins a new top-level form
        if (t.kind == Tok.Directive || t.span.column == 1) return true;
        return def_head_at(p_, false) != 0;
    }

    // ---- items ----
    Item new_item(Span s)
    {
        auto it = new Item;
        it.span = s;
        return it;
    }

    Item item(bool in_struct)
    {
        auto t = peek();
        Span start = t.span;
        if (t.kind == Tok.Directive)
        {
            next();
            auto it = new_item(start);
            it.kind = IK.Directive;
            it.name = t.text;
            it.text = t.sval;
            return it;
        }
        if (t.kind == Tok.Kw)
        {
            if (t.text == "module") return module_item();
            if (t.text == "type") return type_decl(false);
            if (t.text == "val") return val_item(false);
            if (t.text == "import")
            {
                next();
                if (!at(Tok.Str)) unexpected("a string path after `import`");
                auto it = new_item(start);
                it.kind = IK.Import;
                it.text = next().sval;
                it.span = span_from(start);
                return it;
            }
            if (t.text == "include" || t.text == "open")
            {
                next();
                auto it = new_item(start);
                it.kind = t.text == "include" ? IK.Include : IK.Open;
                it.path = module_path();
                it.span = span_from(start);
                return it;
            }
            if (t.text == "let") return let_item();
            if (t.text == "end" && !in_struct)
                error_here("M0202", "unexpected `end` outside of a `struct` or `sig`");
        }
        if (t.kind == Tok.LIdent)
        {
            int h = def_head_at(p_, true);
            if (h == 1) return sig_item();
            if (h == 2) return def_item();
        }
        auto it = new_item(start);
        it.kind = IK.Expr;
        it.body = expr();
        check_no_tacit(it.body);
        it.span = span_from(start);
        return it;
    }

    string[] module_path()
    {
        string[] path;
        if (!at(Tok.UIdent)) unexpected("a module name");
        path ~= next().text;
        while (at(Tok.Dot) && peek(1).kind == Tok.UIdent)
        {
            next();
            path ~= next().text;
        }
        return path;
    }

    Item sig_item()
    {
        Span start = peek().span;
        auto it = new_item(start);
        it.kind = IK.Sig;
        it.name = next().text;
        expect_op("::");
        it.type = type();
        it.span = span_from(start);
        return it;
    }

    string binder_name()
    {
        if (at(Tok.LIdent)) return next().text;
        if (at(Tok.LParen) && peek(1).kind == Tok.Op && peek(2).kind == Tok.RParen)
        {
            next();
            string op = next().text;
            next();
            return op;
        }
        unexpected("a value name");
    }

    Item def_item()
    {
        Span start = peek().span;
        string name = binder_name();
        Pat[] params;
        while (!at_op(":=") && !at_op("=")) params ~= patom();
        next();
        auto body = expr();
        return make_def(name, params, null, body, span_from(start));
    }

    Item val_item(bool in_sig)
    {
        Span start = peek().span;
        expect_kw("val");
        string name = binder_name();
        if (at_op(":"))
        {
            next();
            auto ty = type();
            if (!in_sig && (at_op(":=") || at_op("=")))
            {
                next();
                auto body = expr();
                return make_def(name, null, ty, body, span_from(start));
            }
            auto it = new_item(start);
            it.kind = IK.Sig;
            it.name = name;
            it.type = ty;
            it.span = span_from(start);
            return it;
        }
        if (in_sig) unexpected("`:` and a type in a signature");
        Pat[] params;
        while (!at_op(":=") && !at_op("="))
        {
            if (at(Tok.Eof)) unexpected("`:=`");
            params ~= patom();
        }
        next();
        auto body = expr();
        return make_def(name, params, null, body, span_from(start));
    }

    Item let_item()
    {
        Span start = peek().span;
        size_t save = p_;
        next();
        if (at_kw("rec")) next();
        if (at(Tok.LIdent))
        {
            string name = next().text;
            Pat[] params;
            bool ok = true;
            while (!at_op("=") && !at_op(":="))
            {
                if (at(Tok.Eof) || at_kw("in")) { ok = false; break; }
                params ~= patom();
            }
            if (ok)
            {
                next();
                auto body = expr();
                if (!at_kw("in") && !at_kw("and"))
                    return make_def(name, params, null, body, span_from(start));
            }
        }
        p_ = save;
        auto it = new_item(start);
        it.kind = IK.Expr;
        it.body = expr();
        check_no_tacit(it.body);
        it.span = span_from(start);
        return it;
    }

    Item make_def(string name, Pat[] params, TypeExpr annot, Expr body, Span span)
    {
        auto it = new_item(span);
        it.kind = IK.Def;
        it.name = name;
        it.type = annot;
        it.body = body;
        int tmax = max_tacit(body);
        bool[string] names;
        foreach (pp; params) pat_names(pp, names);
        if (tmax > 0)
        {
            bool named_used = false;
            if (params.length)
            {
                bool[string] fv;
                free_vars(body, null, fv);
                foreach (n; sortedKeys(names)) if (n in fv) named_used = true;
            }
            if (named_used || (params.length && tmax > cast(int) params.length))
            {
                Diagnostic d = make_diag("M0201", "mixed parameter styles: `" ~ name
                    ~ "` uses both named parameters and `$n` placeholders", span);
                d.suggestions ~= "use either named parameters or `$1`, `$2`, ... but not both";
                throw new ParseFail(d, false);
            }
            it.tacit = true;
            Pat[] ps = params.dup;
            if (!ps.length)
                foreach (k; 1 .. tmax + 1) ps ~= mk_pvar("$" ~ to!string(k), body.span);
            auto lam = mk_lambda(ps, body, span);
            lam.tacit_count = tmax;
            lam.name = name;
            it.params = params;
            it.value = lam;
            return it;
        }
        it.params = params;
        if (!params.length)
        {
            it.value = body;
            if (body.kind == EK.Lambda && !body.name.length) body.name = name;
        }
        else
        {
            auto lam = mk_lambda(params, body, span);
            lam.name = name;
            it.value = lam;
        }
        return it;
    }

    Item type_decl(bool in_sig)
    {
        Span start = peek().span;
        expect_kw("type");
        auto it = new_item(start);
        it.kind = IK.Type;
        if (!at(Tok.UIdent) && !at(Tok.LIdent)) unexpected("a type name");
        it.name = next().text;
        while ((at(Tok.TyVar) || at(Tok.LIdent)) && !peek().trivia.newline_before)
            it.tparams ~= next().text;
        if (at_op("=") || at_op(":="))
        {
            next();
            if (at_op("|") || adt_ahead())
            {
                it.tkind = 2;
                if (at_op("|")) next();
                it.ctors ~= ctor_decl();
                while (at_op("|"))
                {
                    next();
                    it.ctors ~= ctor_decl();
                }
            }
            else
            {
                it.tkind = 1;
                it.type = type();
            }
        }
        it.span = span_from(start);
        return it;
    }

    bool adt_ahead() const
    {
        if (!at(Tok.UIdent)) return false;
        int d = 0;
        for (size_t j = p_; j < toks_.length; ++j)
        {
            auto t = &toks_[j];
            if (j > p_ && d == 0 && t.trivia.newline_before
                && (t.kind == Tok.Kw || t.kind == Tok.Directive || t.kind == Tok.Eof
                    || def_head_at(j, true)))
                return false;
            if (t.kind == Tok.Eof) return false;
            if (t.kind == Tok.LParen || t.kind == Tok.LBracket || t.kind == Tok.LBrace) ++d;
            else if (t.kind == Tok.RParen || t.kind == Tok.RBracket || t.kind == Tok.RBrace)
            {
                if (--d < 0) return false;
            }
            else if (d == 0 && t.kind == Tok.Op && t.text == "|") return true;
            else if (d == 0 && t.kind == Tok.Op && t.text == "->") return false;
        }
        return false;
    }

    CtorDecl ctor_decl()
    {
        CtorDecl c;
        c.span = peek().span;
        if (!at(Tok.UIdent)) unexpected("a constructor name");
        c.name = next().text;
        if (at_kw("of"))
        {
            next();
            c.args ~= type();
            return c;
        }
        while (starts_tatom(peek()) && !peek().trivia.newline_before) c.args ~= tatom();
        return c;
    }

    SigExpr sig_expr()
    {
        Span start = peek().span;
        auto s = new SigExpr;
        s.span = start;
        if (at_kw("sig"))
        {
            next();
            while (!at_kw("end"))
            {
                if (at(Tok.Eof)) unexpected("`end` closing `sig`");
                if (at_kw("type")) s.items ~= type_decl(true);
                else if (at_kw("val")) s.items ~= val_item(true);
                else if (at(Tok.LIdent) && peek(1).kind == Tok.Op && peek(1).text == "::")
                    s.items ~= sig_item();
                else unexpected("`type`, `val` or `end` in a signature");
            }
            next();
            s.span = span_from(start);
            return s;
        }
        if (at(Tok.UIdent))
        {
            s.named = true;
            s.path = module_path();
            s.span = span_from(start);
            return s;
        }
        unexpected("`sig` or a signature name");
    }

    ModExpr mod_expr()
    {
        Span start = peek().span;
        auto m = new ModExpr;
        m.span = start;
        if (at_kw("struct"))
        {
            next();
            m.kind = MK.Struct;
            while (!at_kw("end"))
            {
                if (at(Tok.Eof)) unexpected("`end` closing `struct`");
                m.items ~= item(true);
            }
            next();
            m.span = span_from(start);
            return m;
        }
        if (at(Tok.UIdent))
        {
            m.path = module_path();
            if (at(Tok.LParen))
            {
                next();
                m.kind = MK.Apply;
                m.arg = mod_expr();
                expect(Tok.RParen, "`)`");
            }
            else m.kind = MK.Path;
            m.span = span_from(start);
            return m;
        }
        unexpected("`struct` or a module path");
    }

    Item module_item()
    {
        Span start = peek().span;
        expect_kw("module");
        auto it = new_item(start);
        if (at_kw("type"))
        {
            next();
            it.kind = IK.ModType;
            if (!at(Tok.UIdent)) unexpected("a signature name");
            it.name = next().text;
            if (!at_op(":=") && !at_op("=")) unexpected("`:=`");
            next();
            it.sig = sig_expr();
            it.span = span_from(start);
            return it;
        }
        if (!at(Tok.UIdent)) unexpected("a module name");
        it.name = next().text;
        it.kind = IK.Module;
        if (at(Tok.LParen))
        {
            next();
            it.kind = IK.Functor;
            if (!at(Tok.UIdent)) unexpected("a functor parameter name");
            it.pname = next().text;
            expect_op(":");
            it.psig = sig_expr();
            expect(Tok.RParen, "`)`");
        }
        if (at_op(":") || at_op(":>"))
        {
            it.asc = next().text == ":" ? Asc.Transparent : Asc.Opaque;
            it.sig = sig_expr();
        }
        if (!at_op(":=") && !at_op("=")) unexpected("`:=`");
        next();
        it.mexpr = mod_expr();
        if (it.kind == IK.Functor && it.mexpr.kind != MK.Struct)
            error_at(prev(), "M0202", "a functor body must be `struct ... end`");
        it.span = span_from(start);
        return it;
    }

    // ---- expressions ----
    Expr expr() { return binary(0); }

    Expr binary(int min_prec)
    {
        auto lhs = prefix();
        while (true)
        {
            auto t = &peek();
            if (t.kind != Tok.Op || at_boundary()) break;
            if (peek(1).kind == Tok.RParen) break; // `(e op)` is a left section
            bool right = false;
            int prec = op_prec(t.text, right);
            if (prec < 0 || prec < min_prec) break;
            string op = next().text;
            if (op == "|>")
            {
                Expr rhs;
                auto u = &peek();
                bool r2 = false;
                if (u.kind == Tok.Op && op_prec(u.text, r2) >= 0) rhs = bare_section();
                else rhs = binary(1);
                lhs = mk_binop(op, lhs, rhs);
                continue;
            }
            auto rhs = binary(right ? prec : prec + 1);
            lhs = mk_binop(op, lhs, rhs);
        }
        return lhs;
    }

    /// `|> + 1` — a right section without parentheses.
    Expr bare_section()
    {
        auto t = next();
        string op = t.text;
        bool right = false;
        int prec = op_prec(op, right);
        auto operand = binary(prec + 1);
        string pn = "#0";
        auto body = mk_binop(op, mk_var(pn, t.span), operand);
        return mk_lambda([mk_pvar(pn, t.span)], body, span_from(t.span));
    }

    Expr prefix()
    {
        if (at_op("-") || at_op("~"))
        {
            Span s = next().span;
            auto operand = app();
            auto e = mk_expr(EK.Neg, merge_span(s, operand.span));
            e.kids = [operand];
            return e;
        }
        return app();
    }

    Expr app()
    {
        bool head_greedy = at_kw("fn") || at_kw("fun") || at_kw("if") || at_kw("match") || at_kw("let");
        auto f = postfix();
        if (head_greedy) return f;
        while (starts_atom(peek()) && !at_boundary())
        {
            auto arg = postfix();
            f = mk_app(f, arg);
        }
        return f;
    }

    Expr postfix()
    {
        auto a = atom();
        while (true)
        {
            if (at(Tok.Dot) && peek(1).kind == Tok.LIdent)
            {
                next();
                auto f = next();
                auto e = mk_expr(EK.Project, merge_span(a.span, f.span));
                e.kids = [a];
                e.text = f.text;
                a = e;
                continue;
            }
            if (at(Tok.LParen) && !peek().trivia.space_before && value_end(prev()))
            {
                Span ps = next().span;
                ++depth_;
                if (at(Tok.RParen))
                {
                    next();
                    --depth_;
                    auto u = mk_expr(EK.Unit, span_from(ps));
                    a = mk_app(a, u);
                    continue;
                }
                Expr[] args = [expr()];
                while (at(Tok.Comma))
                {
                    next();
                    args ~= expr();
                }
                expect(Tok.RParen, "`)` closing the argument list");
                --depth_;
                foreach (x; args)
                {
                    a = mk_app(a, x);
                    a.span = span_from(a.kids[0].span);
                }
                continue;
            }
            break;
        }
        return a;
    }

    Expr atom()
    {
        auto t = peek();
        Span s = t.span;
        switch (t.kind)
        {
            case Tok.Int:
            {
                next();
                auto e = mk_expr(EK.Int, s);
                e.ival = t.ival;
                return e;
            }
            case Tok.Float:
            {
                next();
                auto e = mk_expr(EK.Float, s);
                e.fval = t.fval;
                return e;
            }
            case Tok.Str:
            {
                next();
                auto e = mk_expr(EK.Str, s);
                e.text = t.sval;
                return e;
            }
            case Tok.LIdent:
                next();
                return mk_var(t.text, s);
            case Tok.UIdent:
                return upper_path();
            case Tok.Tacit:
            {
                next();
                if (t.ival < 1) error_at(t, "M0201", "tacit parameters are numbered from `$1`");
                auto e = mk_expr(EK.Tacit, s);
                e.ival = t.ival;
                return e;
            }
            case Tok.Hole:
            {
                next();
                if (!holes_.length)
                    error_at(t, "M0207", "section hole `#" ~ to!string(t.ival)
                        ~ "` is only legal inside parentheses");
                if (t.ival < 1) error_at(t, "M0207", "section holes are numbered from `#1`");
                if (t.ival > holes_[$ - 1]) holes_[$ - 1] = cast(int) t.ival;
                return mk_var("#" ~ to!string(t.ival), s);
            }
            case Tok.LParen: return paren();
            case Tok.LBracket: return list();
            case Tok.LBrace: return record();
            case Tok.Kw:
                if (t.text == "true" || t.text == "false")
                {
                    next();
                    auto e = mk_expr(EK.Bool, s);
                    e.ival = t.text == "true";
                    return e;
                }
                if (t.text == "fn" || t.text == "fun")
                {
                    next();
                    Pat[] ps;
                    while (!at_op("->"))
                    {
                        if (at(Tok.Eof)) unexpected("`->`");
                        ps ~= patom();
                    }
                    if (!ps.length) unexpected("a parameter");
                    next();
                    auto body = expr();
                    return mk_lambda(ps, body, span_from(s));
                }
                if (t.text == "if")
                {
                    next();
                    auto c = expr();
                    expect_kw("then");
                    auto a = expr();
                    expect_kw("else");
                    auto b = expr();
                    auto e = mk_expr(EK.If, span_from(s));
                    e.kids = [c, a, b];
                    return e;
                }
                if (t.text == "match") return match_expr();
                if (t.text == "let") return let_expr();
                if (t.text == "do") return do_block();
                break;
            default:
                break;
        }
        unexpected("an expression");
    }

    Expr upper_path()
    {
        Span s = peek().span;
        string[] segs = [next().text];
        while (at(Tok.Dot))
        {
            if (peek(1).kind == Tok.UIdent)
            {
                next();
                segs ~= next().text;
                continue;
            }
            if (peek(1).kind == Tok.LIdent)
            {
                next();
                segs ~= next().text;
                auto e = mk_expr(EK.Path, span_from(s));
                e.path = segs;
                return e;
            }
            if (peek(1).kind == Tok.LParen && peek(2).kind == Tok.Op && peek(3).kind == Tok.RParen)
            {
                next();
                next();
                segs ~= next().text;
                next();
                auto e = mk_expr(EK.Path, span_from(s));
                e.path = segs;
                return e;
            }
            break;
        }
        auto e = mk_expr(EK.Ctor, span_from(s));
        e.text = segs[$ - 1];
        e.path = segs[0 .. $ - 1];
        return e;
    }

    Expr paren()
    {
        Span s = next().span;
        ++depth_;
        if (at(Tok.RParen))
        {
            next();
            --depth_;
            return mk_expr(EK.Unit, span_from(s));
        }
        bool r = false;
        if (at(Tok.Op) && op_prec(peek().text, r) >= 0)
        {
            auto opt = next();
            string op = opt.text;
            if (at(Tok.RParen))
            {
                next();
                --depth_;
                auto e = mk_expr(EK.OpRef, span_from(s));
                e.text = op;
                return e;
            }
            holes_ ~= 0;
            auto operand = expr();
            int nh = holes_[$ - 1];
            holes_ = holes_[0 .. $ - 1];
            expect(Tok.RParen, "`)` closing the section");
            --depth_;
            string pn = "#0";
            auto body = mk_binop(op, mk_var(pn, opt.span), operand);
            auto lam = mk_lambda([mk_pvar(pn, opt.span)], body, span_from(s));
            return wrap_holes(lam, nh, s);
        }
        holes_ ~= 0;
        auto e = expr();
        if (at(Tok.Op) && op_prec(peek().text, r) >= 0 && peek(1).kind == Tok.RParen)
        {
            auto opt = next();
            next();
            --depth_;
            int nh = holes_[$ - 1];
            holes_ = holes_[0 .. $ - 1];
            string pn = "#0";
            auto body = mk_binop(opt.text, e, mk_var(pn, opt.span));
            return wrap_holes(mk_lambda([mk_pvar(pn, opt.span)], body, span_from(s)), nh, s);
        }
        if (at(Tok.Comma))
        {
            auto tup = mk_expr(EK.Tuple, s);
            tup.kids ~= e;
            while (at(Tok.Comma))
            {
                next();
                tup.kids ~= expr();
            }
            e = tup;
        }
        else if (at_op(":"))
        {
            next();
            auto an = mk_expr(EK.Annot, s);
            an.kids = [e];
            an.annot = type();
            e = an;
        }
        expect(Tok.RParen, "`)`");
        --depth_;
        int nh = holes_[$ - 1];
        holes_ = holes_[0 .. $ - 1];
        e.span = span_from(s);
        return wrap_holes(e, nh, s);
    }

    Expr wrap_holes(Expr e, int n, Span s)
    {
        if (n == 0) return e;
        Pat[] ps;
        foreach (k; 1 .. n + 1) ps ~= mk_pvar("#" ~ to!string(k), s);
        return mk_lambda(ps, e, span_from(s));
    }

    Expr list()
    {
        Span s = next().span;
        ++depth_;
        if (at(Tok.RBracket))
        {
            next();
            --depth_;
            return mk_expr(EK.List, span_from(s));
        }
        auto first = expr();
        if (at_op(".."))
        {
            next();
            auto last = expr();
            expect(Tok.RBracket, "`]`");
            --depth_;
            auto e = mk_expr(EK.Range, span_from(s));
            e.kids = [first, last];
            return e;
        }
        if (at_kw("for"))
        {
            next();
            auto e = mk_expr(EK.Comp, s);
            e.kids = [first];
            while (true)
            {
                if (at_kw("if"))
                {
                    next();
                    e.clauses ~= CompClause(null, expr());
                }
                else
                {
                    auto pat = pattern();
                    expect_op("<-");
                    e.clauses ~= CompClause(pat, expr());
                }
                if (at(Tok.Comma)) { next(); continue; }
                if (at_kw("if")) continue;
                break;
            }
            expect(Tok.RBracket, "`]`");
            --depth_;
            e.span = span_from(s);
            return e;
        }
        auto e = mk_expr(EK.List, s);
        e.kids ~= first;
        while (at(Tok.Comma))
        {
            next();
            if (at(Tok.RBracket)) break;
            e.kids ~= expr();
        }
        expect(Tok.RBracket, "`]`");
        --depth_;
        e.span = span_from(s);
        return e;
    }

    void record_fields(Expr e)
    {
        while (!at(Tok.RBrace))
        {
            if (!at(Tok.LIdent)) unexpected("a field name");
            e.labels ~= next().text;
            if (!at_op("=") && !at_op(":=")) unexpected("`=` after the field name");
            next();
            e.kids ~= expr();
            if (at(Tok.Comma) || at(Tok.Semi)) next();
            else break;
        }
        expect(Tok.RBrace, "`}`");
    }

    Expr record()
    {
        Span s = next().span;
        ++depth_;
        if (at(Tok.LIdent) && (at_op("=", 1) || at_op(":=", 1)))
        {
            auto e = mk_expr(EK.Record, s);
            record_fields(e);
            --depth_;
            e.span = span_from(s);
            check_dup_labels(e.labels, s);
            return e;
        }
        if (at(Tok.RBrace))
        {
            next();
            --depth_;
            return mk_expr(EK.Record, span_from(s));
        }
        auto base = expr();
        expect_kw("with");
        auto e = mk_expr(EK.Update, s);
        e.kids ~= base;
        record_fields(e);
        --depth_;
        e.span = span_from(s);
        return e;
    }

    void check_dup_labels(string[] labels, Span s)
    {
        bool[string] seen;
        foreach (l; labels)
        {
            if (l in seen)
                throw new ParseFail(make_diag("M0208", "duplicate field `" ~ l ~ "` in record", s), false);
            seen[l] = true;
        }
    }

    Expr match_expr()
    {
        Span s = next().span;
        auto scrut = expr();
        expect_kw("with");
        auto e = mk_expr(EK.Match, s);
        e.kids ~= scrut;
        if (at_op("|")) next();
        while (true)
        {
            auto pat = pattern();
            Expr guard;
            if (at_kw("when"))
            {
                next();
                guard = expr();
            }
            expect_op("->");
            auto body = expr();
            e.pats ~= pat;
            e.guards ~= guard;
            e.kids ~= body;
            if (at_op("|")) { next(); continue; }
            break;
        }
        e.span = span_from(s);
        return e;
    }

    struct Binding
    {
        Pat pat;
        Expr rhs;
    }

    Binding binding(bool allow_colon_eq)
    {
        if (at(Tok.LIdent) && !(at_op("=", 1) || at_op(":=", 1)) && peek(1).kind != Tok.Comma)
        {
            Span s = peek().span;
            string name = next().text;
            Pat[] params;
            while (!at_op("=") && !(allow_colon_eq && at_op(":=")))
            {
                if (at(Tok.Eof)) unexpected("`=`");
                params ~= patom();
            }
            next();
            auto body = expr();
            auto lam = mk_lambda(params, body, span_from(s));
            lam.name = name;
            return Binding(mk_pvar(name, s), lam);
        }
        auto pat = pattern();
        if (!at_op("=") && !(allow_colon_eq && at_op(":="))) unexpected("`=`");
        next();
        auto rhs = expr();
        if (pat.kind == PK.Var && rhs.kind == EK.Lambda && !rhs.name.length) rhs.name = pat.text;
        return Binding(pat, rhs);
    }

    Expr let_expr()
    {
        Span s = next().span;
        bool rec = false;
        if (at_kw("rec")) { next(); rec = true; }
        Binding[] bs = [binding(true)];
        while (at_kw("and"))
        {
            next();
            bs ~= binding(true);
        }
        expect_kw("in");
        auto body = expr();
        if (rec)
        {
            auto e = mk_expr(EK.LetRec, span_from(s));
            foreach (b; bs)
            {
                if (b.pat.kind != PK.Var)
                    throw new ParseFail(make_diag("M0205", "`let rec` can only bind names", b.pat.span), false);
                e.pats ~= b.pat;
                e.kids ~= b.rhs;
            }
            e.kids ~= body;
            return e;
        }
        for (size_t i = bs.length; i-- > 0;)
        {
            auto e = mk_expr(EK.Let, span_from(s));
            e.pats = [bs[i].pat];
            e.kids = [bs[i].rhs, body];
            body = e;
        }
        return body;
    }

    Expr do_block()
    {
        Span s = next().span;
        expect(Tok.LBrace, "`{` after `do`");
        ++depth_;
        struct Stmt
        {
            bool is_let;
            bool rec;
            Binding b;
            Expr e;
        }
        Stmt[] stmts;
        while (!at(Tok.RBrace))
        {
            Stmt st;
            if (at_kw("let"))
            {
                next();
                if (at_kw("rec")) { next(); st.rec = true; }
                st.is_let = true;
                st.b = binding(true);
            }
            else st.e = expr();
            stmts ~= st;
            if (at(Tok.Semi))
            {
                next();
                if (at(Tok.RBrace))
                {
                    Stmt u;
                    u.e = mk_expr(EK.Unit, peek().span);
                    stmts ~= u;
                }
                continue;
            }
            break;
        }
        expect(Tok.RBrace, "`}` closing `do`");
        --depth_;
        Span whole = span_from(s);
        Expr acc;
        if (!stmts.length || stmts[$ - 1].is_let) acc = mk_expr(EK.Unit, whole);
        for (size_t i = stmts.length; i-- > 0;)
        {
            auto st = stmts[i];
            if (acc is null) { acc = st.e; continue; }
            if (st.is_let && st.rec)
            {
                auto e = mk_expr(EK.LetRec, whole);
                e.pats = [st.b.pat];
                e.kids = [st.b.rhs, acc];
                acc = e;
                continue;
            }
            auto e = mk_expr(EK.Let, whole);
            if (st.is_let)
            {
                e.pats = [st.b.pat];
                e.kids = [st.b.rhs, acc];
            }
            else
            {
                auto w = new Pat;
                w.span = st.e.span;
                w.kind = PK.Wild;
                e.pats = [w];
                e.kids = [st.e, acc];
            }
            acc = e;
        }
        return acc;
    }

    // ---- patterns ----
    bool starts_patom(ref const MToken t) const
    {
        switch (t.kind)
        {
            case Tok.LIdent: case Tok.UIdent: case Tok.Int: case Tok.Float: case Tok.Str:
            case Tok.LParen: case Tok.LBracket: case Tok.LBrace:
                return true;
            case Tok.Kw:
                return t.text == "true" || t.text == "false";
            default:
                return false;
        }
    }

    Pat pattern()
    {
        auto p = pat_app();
        if (at_op("::"))
        {
            next();
            auto rest = pattern();
            auto c = new Pat;
            c.span = merge_span(p.span, rest.span);
            c.kind = PK.Cons;
            c.kids = [p, rest];
            return c;
        }
        return p;
    }

    Pat ctor_pat_head()
    {
        Span s = peek().span;
        string[] segs = [next().text];
        while (at(Tok.Dot) && peek(1).kind == Tok.UIdent)
        {
            next();
            segs ~= next().text;
        }
        auto c = new Pat;
        c.span = s;
        c.kind = PK.Ctor;
        c.text = segs[$ - 1];
        c.path = segs[0 .. $ - 1];
        if (at(Tok.LParen) && !peek().trivia.space_before)
        {
            next();
            ++depth_;
            if (!at(Tok.RParen))
            {
                c.kids ~= pattern();
                while (at(Tok.Comma))
                {
                    next();
                    c.kids ~= pattern();
                }
            }
            expect(Tok.RParen, "`)`");
            --depth_;
        }
        c.span = span_from(s);
        return c;
    }

    Pat pat_app()
    {
        if (at(Tok.UIdent))
        {
            auto c = ctor_pat_head();
            if (!c.kids.length)
                while (starts_patom(peek()) && !at_boundary()) c.kids ~= patom();
            c.span = span_from(c.span);
            return c;
        }
        return patom();
    }

    Pat patom()
    {
        auto t = peek();
        Span s = t.span;
        auto p = new Pat;
        p.span = s;
        switch (t.kind)
        {
            case Tok.LIdent:
                next();
                if (t.text == "_") p.kind = PK.Wild;
                else { p.kind = PK.Var; p.text = t.text; }
                return p;
            case Tok.UIdent: return ctor_pat_head();
            case Tok.Int:
                next();
                p.kind = PK.Int;
                p.ival = t.ival;
                return p;
            case Tok.Float:
                next();
                p.kind = PK.Float;
                p.fval = t.fval;
                return p;
            case Tok.Str:
                next();
                p.kind = PK.Str;
                p.text = t.sval;
                return p;
            case Tok.Kw:
                if (t.text == "true" || t.text == "false")
                {
                    next();
                    p.kind = PK.Bool;
                    p.ival = t.text == "true";
                    return p;
                }
                break;
            case Tok.LParen:
            {
                next();
                ++depth_;
                if (at(Tok.RParen))
                {
                    next();
                    --depth_;
                    p.kind = PK.Unit;
                    p.span = span_from(s);
                    return p;
                }
                auto first = pattern();
                if (at(Tok.Comma))
                {
                    p.kind = PK.Tuple;
                    p.kids ~= first;
                    while (at(Tok.Comma))
                    {
                        next();
                        p.kids ~= pattern();
                    }
                }
                else if (at_op(":"))
                {
                    next();
                    p.kind = PK.Annot;
                    p.kids ~= first;
                    p.annot = type();
                }
                else p = first;
                expect(Tok.RParen, "`)`");
                --depth_;
                p.span = span_from(s);
                return p;
            }
            case Tok.LBracket:
            {
                next();
                ++depth_;
                p.kind = PK.List;
                while (!at(Tok.RBracket))
                {
                    p.kids ~= pattern();
                    if (at(Tok.Comma)) next();
                    else break;
                }
                expect(Tok.RBracket, "`]`");
                --depth_;
                p.span = span_from(s);
                return p;
            }
            case Tok.LBrace:
            {
                next();
                ++depth_;
                p.kind = PK.Record;
                while (!at(Tok.RBrace))
                {
                    if (at_op(".."))
                    {
                        next();
                        p.open = true;
                        break;
                    }
                    if (!at(Tok.LIdent)) unexpected("a field name");
                    auto f = next();
                    p.labels ~= f.text;
                    if (at_op("="))
                    {
                        next();
                        p.kids ~= pattern();
                    }
                    else p.kids ~= mk_pvar(f.text, f.span);
                    if (at(Tok.Comma) || at(Tok.Semi)) next();
                    else break;
                }
                expect(Tok.RBrace, "`}`");
                --depth_;
                p.span = span_from(s);
                return p;
            }
            default:
                break;
        }
        unexpected("a pattern");
    }

    // ---- types ----
    bool starts_tatom(ref const MToken t) const
    {
        return t.kind == Tok.TyVar || t.kind == Tok.LIdent || t.kind == Tok.UIdent
            || t.kind == Tok.LParen || t.kind == Tok.LBracket || t.kind == Tok.LBrace;
    }

    TypeExpr new_type(Span s)
    {
        auto t = new TypeExpr;
        t.span = s;
        return t;
    }

    TypeExpr type()
    {
        string[] nums;
        while (at(Tok.UIdent) && peek().text == "Num"
            && (peek(1).kind == Tok.TyVar || peek(1).kind == Tok.LIdent)
            && peek(2).kind == Tok.Op && peek(2).text == "=>")
        {
            next();
            nums ~= next().text;
            next();
        }
        auto t = type_arrow();
        t.num_vars = nums;
        return t;
    }

    TypeExpr type_arrow()
    {
        auto a = type_app();
        if (at_op("->"))
        {
            next();
            auto b = type_arrow();
            auto f = new_type(merge_span(a.span, b.span));
            f.kind = TEK.Fun;
            f.kids = [a, b];
            return f;
        }
        return a;
    }

    bool type_arg_ok() const
    {
        auto t = &peek();
        if (!starts_tatom(*t)) return false;
        if (depth_ == 0 && t.trivia.newline_before) return false;
        if (t.kind == Tok.LIdent && (t.text == "list" || t.text == "option")) return false;
        return true;
    }

    TypeExpr type_app()
    {
        TypeExpr t;
        if (at(Tok.UIdent))
        {
            t = tname();
            if (!(at(Tok.LParen) && !peek().trivia.space_before))
                while (type_arg_ok()) t.kids ~= tatom();
        }
        else t = tatom();
        while (at(Tok.LIdent) && (peek().text == "list" || peek().text == "option")
            && !(depth_ == 0 && peek().trivia.newline_before))
        {
            auto w = next();
            auto n = new_type(merge_span(t.span, w.span));
            if (w.text == "list")
            {
                n.kind = TEK.List;
                n.kids = [t];
            }
            else
            {
                n.kind = TEK.Name;
                n.name = "Option";
                n.kids = [t];
            }
            t = n;
        }
        return t;
    }

    TypeExpr tname()
    {
        Span s = peek().span;
        auto t = new_type(s);
        t.kind = TEK.Name;
        string[] segs = [next().text];
        while (at(Tok.Dot) && (peek(1).kind == Tok.UIdent || peek(1).kind == Tok.LIdent))
        {
            next();
            segs ~= next().text;
            if (prev().kind == Tok.LIdent) break;
        }
        t.name = segs[$ - 1];
        t.path = segs[0 .. $ - 1];
        if (at(Tok.LParen) && !peek().trivia.space_before)
        {
            next();
            ++depth_;
            t.kids ~= type();
            while (at(Tok.Comma))
            {
                next();
                t.kids ~= type();
            }
            expect(Tok.RParen, "`)`");
            --depth_;
        }
        t.span = span_from(s);
        return t;
    }

    TypeExpr tatom()
    {
        auto tk = peek();
        Span s = tk.span;
        auto t = new_type(s);
        switch (tk.kind)
        {
            case Tok.TyVar:
                next();
                t.kind = TEK.Var;
                t.name = tk.text;
                return t;
            case Tok.LIdent:
                next();
                t.kind = TEK.Name;
                t.name = tk.text;
                return t;
            case Tok.UIdent: return tname();
            case Tok.LParen:
            {
                next();
                ++depth_;
                if (at(Tok.RParen))
                {
                    next();
                    --depth_;
                    t.kind = TEK.Unit;
                    return t;
                }
                auto first = type();
                if (at(Tok.Comma))
                {
                    t.kind = TEK.Tuple;
                    t.kids ~= first;
                    while (at(Tok.Comma))
                    {
                        next();
                        t.kids ~= type();
                    }
                }
                else t = first;
                expect(Tok.RParen, "`)`");
                --depth_;
                return t;
            }
            case Tok.LBracket:
            {
                next();
                ++depth_;
                t.kind = TEK.List;
                t.kids ~= type();
                expect(Tok.RBracket, "`]`");
                --depth_;
                t.span = span_from(s);
                return t;
            }
            case Tok.LBrace:
            {
                next();
                ++depth_;
                t.kind = TEK.Record;
                while (!at(Tok.RBrace) && !at_op("|"))
                {
                    if (!at(Tok.LIdent)) unexpected("a field name");
                    t.labels ~= next().text;
                    expect_op(":");
                    t.kids ~= type();
                    if (at(Tok.Comma) || at(Tok.Semi)) next();
                    else break;
                }
                if (at_op("|"))
                {
                    next();
                    if (!at(Tok.LIdent) && !at(Tok.TyVar)) unexpected("a row variable");
                    t.row = next().text;
                }
                expect(Tok.RBrace, "`}`");
                --depth_;
                t.span = span_from(s);
                return t;
            }
            default:
                break;
        }
        unexpected("a type");
    }

    // ---- analyses ----
    static int max_tacit(Expr e)
    {
        if (e is null) return 0;
        int m = e.kind == EK.Tacit ? cast(int) e.ival : 0;
        foreach (k; e.kids) m = max(m, max_tacit(k));
        foreach (g; e.guards) m = max(m, max_tacit(g));
        foreach (c; e.clauses) m = max(m, max_tacit(c.expr));
        return m;
    }

    void check_no_tacit(Expr e)
    {
        if (max_tacit(e) > 0)
        {
            auto t = find_tacit(e);
            Diagnostic d = make_diag("M0206", "tacit parameter `$" ~ to!string(t.ival)
                ~ "` used outside of a definition", t.span);
            d.suggestions ~= "`$n` refers to the n-th parameter of the enclosing `name := ...` definition";
            throw new ParseFail(d, false);
        }
    }
    static Expr find_tacit(Expr e)
    {
        if (e.kind == EK.Tacit) return e;
        foreach (k; e.kids) if (max_tacit(k) > 0) return find_tacit(k);
        foreach (g; e.guards) if (g !is null && max_tacit(g) > 0) return find_tacit(g);
        foreach (c; e.clauses) if (max_tacit(c.expr) > 0) return find_tacit(c.expr);
        return e;
    }

public:
    static void pat_names(Pat p, ref bool[string] result)
    {
        if (p is null) return;
        if (p.kind == PK.Var) result[p.text] = true;
        foreach (k; p.kids) pat_names(k, result);
    }

    /// Free variables of an expression (honouring binders).
    static void free_vars(Expr e, bool[string] bound, ref bool[string] result)
    {
        if (e is null) return;
        switch (e.kind)
        {
            case EK.Var:
                if (e.text !in bound) result[e.text] = true;
                return;
            case EK.Lambda:
            {
                auto b = bound.dup;
                foreach (p; e.pats) pat_names(p, b);
                free_vars(e.kids[0], b, result);
                return;
            }
            case EK.Let:
            {
                free_vars(e.kids[0], bound, result);
                auto b = bound.dup;
                pat_names(e.pats[0], b);
                free_vars(e.kids[1], b, result);
                return;
            }
            case EK.LetRec:
            {
                auto b = bound.dup;
                foreach (p; e.pats) pat_names(p, b);
                foreach (k; e.kids) free_vars(k, b, result);
                return;
            }
            case EK.Match:
            {
                free_vars(e.kids[0], bound, result);
                foreach (i; 0 .. e.pats.length)
                {
                    auto b = bound.dup;
                    pat_names(e.pats[i], b);
                    if (e.guards[i] !is null) free_vars(e.guards[i], b, result);
                    free_vars(e.kids[i + 1], b, result);
                }
                return;
            }
            case EK.Comp:
            {
                auto b = bound.dup;
                foreach (c; e.clauses)
                {
                    free_vars(c.expr, b, result);
                    if (c.pat !is null) pat_names(c.pat, b);
                }
                free_vars(e.kids[0], b, result);
                return;
            }
            case EK.BinOp:
                if (!is_builtin_op(e.text) && e.text !in bound) result[e.text] = true;
                break;
            case EK.OpRef:
                if (!is_builtin_op(e.text) && e.text !in bound) result[e.text] = true;
                return;
            default:
                break;
        }
        foreach (k; e.kids) free_vars(k, bound, result);
    }
}

string[] sortedKeys(V)(V[string] aa)
{
    auto ks = aa.keys;
    ks.sort();
    return ks;
}

/// Parses a whole program. Never throws.
ParseOutput parse(string source, string file = "<input>")
{
    ParseOutput result;
    result.program.file = file;
    result.program.source = source;
    auto lx = lex(source, file);
    if (lx.diagnostics.length)
    {
        result.diagnostics = lx.diagnostics;
        result.incomplete = lx.incomplete;
        return result;
    }
    auto p = new ParserImpl(lx.tokens, file);
    try result.program.items = p.program();
    catch (ParseFail f)
    {
        result.diagnostics ~= f.diag;
        result.incomplete = f.at_eof;
    }
    catch (MetaError e) result.diagnostics ~= e.diag;
    return result;
}

/// Parses a single expression (used by #type / #trace).
Result!(Expr, Diagnostic[]) parse_expression(string source, string file = "<input>")
{
    alias R = Result!(Expr, Diagnostic[]);
    auto lx = lex(source, file);
    if (lx.diagnostics.length) return R.from_err(lx.diagnostics);
    auto p = new ParserImpl(lx.tokens, file);
    try return R.from_ok(p.expression_only());
    catch (ParseFail f) return R.from_err([f.diag]);
}

/// Parses a type expression (used by primitives and Type.*).
Result!(TypeExpr, Diagnostic[]) parse_type_expr(string source, string file = "<type>")
{
    alias R = Result!(TypeExpr, Diagnostic[]);
    auto lx = lex(source, file);
    if (lx.diagnostics.length) return R.from_err(lx.diagnostics);
    auto p = new ParserImpl(lx.tokens, file);
    try return R.from_ok(p.type_only());
    catch (ParseFail f) return R.from_err([f.diag]);
}

// ============================================================
//  Types: union-find type graph with a trail (Algorithm W + Rémy rows)
// ============================================================

enum TK { Var, Con, RowEmpty, RowExt }

enum int GENERIC = 1 << 30;

final class TypeNode
{
    TK kind = TK.Var;
    // Var
    int id;
    int level;
    bool num;
    bool rigid;
    TypeNode link;
    string hint;
    // Con
    string name;
    int stamp;
    TypeNode[] args;
    // RowExt
    string label;
    TypeNode field;
    TypeNode rest;
}

/// Builtin constructor stamps (shared by both dialects).
struct stamps
{
    enum int Int = 1, Float = 2, String = 3, Bool = 4, Unit = 5, List = 6, Fun = 7, Tuple = 8,
        Record = 9, First = 100;
}

TypeNode prune(TypeNode t)
{
    while (t.kind == TK.Var && t.link !is null) t = t.link;
    return t;
}

TypeNode t_con(string name, int stamp, TypeNode[] args = null)
{
    auto t = new TypeNode;
    t.kind = TK.Con;
    t.name = name;
    t.stamp = stamp;
    t.args = args;
    return t;
}
TypeNode t_int() { return t_con("Int", stamps.Int); }
TypeNode t_float() { return t_con("Float", stamps.Float); }
TypeNode t_string() { return t_con("String", stamps.String); }
TypeNode t_bool() { return t_con("Bool", stamps.Bool); }
TypeNode t_unit() { return t_con("Unit", stamps.Unit); }
TypeNode t_list(TypeNode a) { return t_con("List", stamps.List, [a]); }
TypeNode t_fun(TypeNode a, TypeNode b) { return t_con("->", stamps.Fun, [a, b]); }
TypeNode t_tuple(TypeNode[] items) { return t_con("*", stamps.Tuple, items); }
TypeNode t_record(TypeNode row) { return t_con("Record", stamps.Record, [row]); }
TypeNode row_empty()
{
    auto t = new TypeNode;
    t.kind = TK.RowEmpty;
    return t;
}
TypeNode row_ext(string label, TypeNode field, TypeNode rest)
{
    auto t = new TypeNode;
    t.kind = TK.RowExt;
    t.label = label;
    t.field = field;
    t.rest = rest;
    return t;
}
bool is_fun(TypeNode t)
{
    auto p = prune(t);
    return p.kind == TK.Con && p.stamp == stamps.Fun;
}

/// Deterministic type printer: variables named 'a, 'b, ... by first use.
final class TypePrinter
{
    private string[TypeNode] names_;
    private string[] num_order_;
    private int next_;

    string print(TypeNode t)
    {
        string body = go(t, 0);
        string pre;
        foreach (n; num_order_) pre ~= (pre.length ? ", " : "") ~ "Num " ~ n;
        return pre.length ? pre ~ " => " ~ body : body;
    }
    string print_bare(TypeNode t) { return go(t, 0); }

    private string var_name(TypeNode v)
    {
        if (auto p = v in names_) return *p;
        string n;
        if (v.rigid && v.hint.length) n = v.hint[0] == '\'' ? v.hint : "'" ~ v.hint;
        else
        {
            int k = next_++;
            n = "'";
            n ~= cast(char)('a' + k % 26);
            if (k >= 26) n ~= to!string(k / 26);
        }
        foreach (existing; names_.byValue) if (existing == n) { n ~= "_" ~ to!string(v.id); break; }
        names_[v] = n;
        if (v.num) num_order_ ~= n;
        return n;
    }

    private string go(TypeNode t0, int prec)
    {
        auto t = prune(t0);
        final switch (t.kind)
        {
            case TK.Var: return var_name(t);
            case TK.RowEmpty: return "{}";
            case TK.RowExt: return "{ " ~ row_text(t) ~ " }";
            case TK.Con: break;
        }
        if (t.stamp == stamps.Fun)
        {
            string lhs = go(t.args[0], 1);
            string s = lhs ~ " -> " ~ go(t.args[1], 0);
            return prec > 0 ? "(" ~ s ~ ")" : s;
        }
        if (t.stamp == stamps.List) return "[" ~ go(t.args[0], 0) ~ "]";
        if (t.stamp == stamps.Tuple)
        {
            string s = "(";
            foreach (i, a; t.args) s ~= (i ? ", " : "") ~ go(a, 0);
            return s ~ ")";
        }
        if (t.stamp == stamps.Record)
        {
            auto row = prune(t.args[0]);
            if (row.kind == TK.RowEmpty) return "{}";
            return "{ " ~ row_text(row) ~ " }";
        }
        if (!t.args.length) return t.name;
        string s = t.name;
        foreach (a; t.args) s ~= " " ~ go(a, 2);
        return prec > 1 ? "(" ~ s ~ ")" : s;
    }

    private string row_text(TypeNode row)
    {
        Tuple!(string, TypeNode)[] fields;
        row = prune(row);
        while (row.kind == TK.RowExt)
        {
            fields ~= tuple(row.label, row.field);
            row = prune(row.rest);
        }
        fields.sort!((a, b) => a[0] < b[0], SwapStrategy.stable);
        string s;
        foreach (i, f; fields) s ~= (i ? ", " : "") ~ f[0] ~ " : " ~ go(f[1], 0);
        if (row.kind == TK.Var) s ~= " | " ~ var_name(row);
        return s;
    }
}

string show_type(TypeNode t)
{
    auto p = new TypePrinter;
    return p.print(t);
}

/// Unifier with an undo trail (needed for type-directed resolution).
final class Unifier
{
    string reason;
    string reason_code;
    TypeNode delegate(int level) fresh_var;

    private struct Entry
    {
        TypeNode v;
        TypeNode link;
        int level;
        bool num;
    }
    private Entry[] trail_;

    size_t snapshot() const { return trail_.length; }
    void rollback(size_t s)
    {
        while (trail_.length > s)
        {
            auto e = trail_[$ - 1];
            e.v.link = e.link;
            e.v.level = e.level;
            e.v.num = e.num;
            trail_ = trail_[0 .. $ - 1];
        }
    }
    void commit() { trail_ = null; }
    void set_level(TypeNode v, int level)
    {
        record(v);
        v.level = level;
    }

    bool unify(TypeNode a, TypeNode b)
    {
        a = prune(a);
        b = prune(b);
        if (a is b) return true;
        if (a.kind == TK.Var && b.kind == TK.Var)
        {
            if (a.rigid && b.rigid)
                return fail("M0309", "the rigid type variables `" ~ rigid_name(a) ~ "` and `"
                    ~ rigid_name(b) ~ "` are distinct");
            if (a.rigid) return bind(b, a);
            return bind(a, b);
        }
        if (a.kind == TK.Var) return bind(a, b);
        if (b.kind == TK.Var) return bind(b, a);
        if (a.kind == TK.Con && b.kind == TK.Con)
        {
            if (a.stamp != b.stamp || a.args.length != b.args.length) return fail("M0301", "");
            foreach (i; 0 .. a.args.length) if (!unify(a.args[i], b.args[i])) return false;
            return true;
        }
        if (a.kind == TK.RowEmpty && b.kind == TK.RowEmpty) return true;
        if (a.kind == TK.RowEmpty && b.kind == TK.RowExt) { auto tmp = a; a = b; b = tmp; }
        if (a.kind == TK.RowExt && (b.kind == TK.RowExt || b.kind == TK.RowEmpty))
        {
            TypeNode tail = row_tail(a.rest);
            TypeNode f, r;
            if (!rewrite(b, a.label, f, r)) return false;
            if (tail.kind == TK.Var && tail.link !is null) return fail("M0303", "recursive row type");
            if (!unify(a.field, f)) return false;
            return unify(a.rest, r);
        }
        return fail("M0301", "");
    }

private:
    void record(TypeNode v) { trail_ ~= Entry(v, v.link, v.level, v.num); }
    bool fail(string code, string why)
    {
        reason_code = code;
        reason = why;
        return false;
    }
    static string rigid_name(TypeNode v)
    {
        if (!v.hint.length) return "'?";
        return v.hint[0] == '\'' ? v.hint : "'" ~ v.hint;
    }
    static TypeNode row_tail(TypeNode r)
    {
        r = prune(r);
        while (r.kind == TK.RowExt) r = prune(r.rest);
        return r;
    }
    bool occurs_adjust(TypeNode v, TypeNode t0)
    {
        auto t = prune(t0);
        if (t is v) return true;
        final switch (t.kind)
        {
            case TK.Var:
                if (t.level > v.level) set_level(t, v.level);
                return false;
            case TK.Con:
                foreach (a; t.args) if (occurs_adjust(v, a)) return true;
                return false;
            case TK.RowExt:
                return occurs_adjust(v, t.field) || occurs_adjust(v, t.rest);
            case TK.RowEmpty:
                return false;
        }
    }
    bool bind(TypeNode v, TypeNode t)
    {
        if (v.rigid)
        {
            if (t.kind == TK.Con)
                return fail("M0309", "the type variable `" ~ rigid_name(v)
                    ~ "` comes from a signature and cannot be `" ~ show_type(t) ~ "`");
            return fail("M0309", "rigid type variable `" ~ rigid_name(v) ~ "`");
        }
        if (occurs_adjust(v, t))
        {
            auto p = new TypePrinter;
            string vs = p.print_bare(v);
            string ts = p.print_bare(t);
            return fail("M0302", "cannot construct the infinite type `" ~ vs ~ " = " ~ ts ~ "`");
        }
        if (v.num)
        {
            if (t.kind == TK.Var)
            {
                if (!t.num)
                {
                    if (t.rigid)
                        return fail("M0306", "the type variable `" ~ rigid_name(t)
                            ~ "` from the signature is not numeric; declare it as `Num "
                            ~ rigid_name(t) ~ " =>`");
                    record(t);
                    t.num = true;
                }
            }
            else if (t.kind != TK.Con || (t.stamp != stamps.Int && t.stamp != stamps.Float))
                return fail("M0306", "expected a numeric type (`Int` or `Float`) but found `"
                    ~ show_type(t) ~ "`");
        }
        record(v);
        v.link = t;
        return true;
    }
    bool rewrite(TypeNode row, string label, ref TypeNode field, ref TypeNode rest)
    {
        row = prune(row);
        if (row.kind == TK.RowExt)
        {
            if (row.label == label)
            {
                field = row.field;
                rest = row.rest;
                return true;
            }
            TypeNode f, r;
            if (!rewrite(row.rest, label, f, r)) return false;
            field = f;
            rest = row_ext(row.label, row.field, r);
            return true;
        }
        if (row.kind == TK.Var)
        {
            if (row.rigid)
                return fail("M0303", "the record type is closed by a signature and has no field `"
                    ~ label ~ "`");
            field = fresh_var(row.level);
            rest = fresh_var(row.level);
            return bind(row, row_ext(label, field, rest));
        }
        return fail("M0303", "the record has no field `" ~ label ~ "`");
    }
}

// ============================================================
//  Static environment: scopes, type definitions, module signatures
// ============================================================

/// Insertion-ordered map (deterministic iteration; mirrors MetaML.hpp).
struct OrderedMap(T)
{
    private string[] order_;
    private T[string] map_;

    inout(T)* find(string k) inout { return k in map_; }
    void set(string k, T v)
    {
        if (k !in map_) order_ ~= k;
        map_[k] = v;
    }
    void erase(string k)
    {
        if (k in map_)
        {
            map_.remove(k);
            string[] o;
            foreach (x; order_) if (x != k) o ~= x;
            order_ = o;
        }
    }
    const(string)[] keys() const { return order_; }
    size_t size() const { return order_.length; }
}

final class TypeDef
{
    string name;
    string display;
    int stamp;
    bool alias_;
    TypeNode[] params;
    TypeNode body;
    CtorInfo[] ctors;
}

final class CtorInfo
{
    string name;
    int index;
    int arity;
    TypeNode scheme;
    TypeDef owner;
}

struct VarInfo
{
    TypeNode scheme;
    Cell cell;
}

final class SigDef
{
    string name;
    SigExpr expr;
    Scope env;
}

final class FunctorInfo
{
    string name;
    Item item;
    Scope env;
}

/// A lexical scope; a module signature is a Scope whose own entries are its exports.
final class Scope
{
    Scope parent;
    string prefix;
    OrderedMap!VarInfo vars;
    OrderedMap!TypeDef types;
    OrderedMap!CtorInfo ctors;
    OrderedMap!Scope modules;
    OrderedMap!FunctorInfo functors;
    OrderedMap!SigDef sigs;
    OrderedMap!Item pending;

    static Scope child(Scope parent, string prefix = null)
    {
        auto s = new Scope;
        s.parent = parent;
        s.prefix = (!prefix.length && parent !is null) ? parent.prefix : prefix;
        return s;
    }

    VarInfo* var(string k)
    {
        for (Scope s = this; s !is null; s = s.parent) if (auto r = s.vars.find(k)) return r;
        return null;
    }
    TypeDef* type(string k)
    {
        for (Scope s = this; s !is null; s = s.parent) if (auto r = s.types.find(k)) return r;
        return null;
    }
    CtorInfo* ctor(string k)
    {
        for (Scope s = this; s !is null; s = s.parent) if (auto r = s.ctors.find(k)) return r;
        return null;
    }
    Scope* module_(string k)
    {
        for (Scope s = this; s !is null; s = s.parent) if (auto r = s.modules.find(k)) return r;
        return null;
    }
    FunctorInfo* functor(string k)
    {
        for (Scope s = this; s !is null; s = s.parent) if (auto r = s.functors.find(k)) return r;
        return null;
    }
    SigDef* sig(string k)
    {
        for (Scope s = this; s !is null; s = s.parent) if (auto r = s.sigs.find(k)) return r;
        return null;
    }
}

// ============================================================
//  Type checker (Hindley–Milner; type-directed concat resolution; modules)
// ============================================================

/// What the REPL reports for a checked unit.
struct UnitInfo
{
    Tuple!(string, TypeNode)[] defs;
    TypeNode expr_type;
}

private struct UnitRange
{
    size_t b, e;
}

final class Checker
{
    Unifier u;
    int level = 0;
    int next_id = 0;
    int next_stamp = stamps.First;
    Scope global;
    Diagnostic[] warnings;
    string[] disabled;
    bool quiet;
    Primitive delegate(string) find_prim;
    TypeNode delegate(Primitive, Span) prim_type;

    this()
    {
        u = new Unifier;
        u.fresh_var = (int lvl) => fresh_at(lvl);
        global = new Scope;
        void builtin(string n, int stamp, int arity)
        {
            auto d = new TypeDef;
            d.name = n;
            d.display = n;
            d.stamp = stamp;
            foreach (i; 0 .. arity) d.params ~= generic_var();
            global.types.set(n, d);
        }
        builtin("Int", stamps.Int, 0);
        builtin("Float", stamps.Float, 0);
        builtin("String", stamps.String, 0);
        builtin("Bool", stamps.Bool, 0);
        builtin("Unit", stamps.Unit, 0);
        builtin("List", stamps.List, 1);
    }

    // ---- variables & schemes ----
    TypeNode fresh_at(int lvl)
    {
        auto t = new TypeNode;
        t.kind = TK.Var;
        t.id = ++next_id;
        t.level = lvl;
        return t;
    }
    TypeNode fresh() { return fresh_at(level); }
    TypeNode fresh_num()
    {
        auto t = fresh();
        t.num = true;
        return t;
    }
    TypeNode generic_var() { return fresh_at(GENERIC); }

    void generalize(TypeNode t0, int lvl)
    {
        auto t = prune(t0);
        final switch (t.kind)
        {
            case TK.Var:
                if (t.level > lvl && t.level != GENERIC) u.set_level(t, GENERIC);
                return;
            case TK.Con:
                foreach (a; t.args) generalize(a, lvl);
                return;
            case TK.RowExt:
                generalize(t.field, lvl);
                generalize(t.rest, lvl);
                return;
            case TK.RowEmpty:
                return;
        }
    }

    TypeNode copy_generic(TypeNode t0, ref TypeNode[TypeNode] m, bool rigid)
    {
        auto t = prune(t0);
        final switch (t.kind)
        {
            case TK.Var:
            {
                if (t.level != GENERIC) return t;
                if (auto p = t in m) return *p;
                auto v = fresh();
                v.num = t.num;
                v.rigid = rigid;
                v.hint = t.hint;
                m[t] = v;
                return v;
            }
            case TK.Con:
            {
                TypeNode[] args;
                foreach (a; t.args) args ~= copy_generic(a, m, rigid);
                return t_con(t.name, t.stamp, args);
            }
            case TK.RowExt:
            {
                auto f = copy_generic(t.field, m, rigid);
                return row_ext(t.label, f, copy_generic(t.rest, m, rigid));
            }
            case TK.RowEmpty:
                return t;
        }
    }
    TypeNode instantiate(TypeNode t)
    {
        TypeNode[TypeNode] m;
        return copy_generic(t, m, false);
    }
    TypeNode skolemize(TypeNode t)
    {
        TypeNode[TypeNode] m;
        return copy_generic(t, m, true);
    }
    /// Substitutes generic parameters by arguments (alias expansion).
    TypeNode subst(TypeNode t0, TypeNode[TypeNode] m)
    {
        auto t = prune(t0);
        final switch (t.kind)
        {
            case TK.Var:
                if (auto p = t in m) return *p;
                return t;
            case TK.Con:
            {
                TypeNode[] args;
                foreach (a; t.args) args ~= subst(a, m);
                return t_con(t.name, t.stamp, args);
            }
            case TK.RowExt:
            {
                auto f = subst(t.field, m);
                return row_ext(t.label, f, subst(t.rest, m));
            }
            case TK.RowEmpty:
                return t;
        }
    }

    // ---- errors ----
    noreturn error(string code, string msg, Span sp, string[] suggestions = null)
    {
        Diagnostic d = make_diag(code, msg, sp);
        d.suggestions = suggestions;
        throw new MetaError(d);
    }
    void warn(string code, string msg, Span sp)
    {
        if (quiet) return;
        foreach (c; disabled) if (c == code) return;
        warnings ~= make_diag(code, msg, sp, Severity.Warning);
    }

    static string conversion_hint(string expected, string found)
    {
        if (expected == "Float" && found == "Int") return "consider calling `Float.of_int`";
        if (expected == "Int" && found == "Float") return "consider calling `Float.truncate`";
        if (expected == "String" && found == "Int") return "consider calling `Int.to_string`";
        if (expected == "String" && found == "Float") return "consider calling `Float.to_string`";
        if (expected == "Unit" && found != "Unit") return "use `let _ := ...;` to discard a value";
        return "";
    }

    void unify_or_error(TypeNode expected, TypeNode found, Span sp, string context = "")
    {
        auto snap = u.snapshot();
        if (u.unify(expected, found)) return;
        string code = u.reason_code, reason = u.reason;
        u.rollback(snap);
        auto p = new TypePrinter;
        string e = p.print_bare(expected);
        string f = p.print_bare(found);
        string msg = "expected `" ~ e ~ "` but found `" ~ f ~ "`";
        if (code == "M0306" && reason.length) msg = reason;
        else if ((code == "M0302" || code == "M0303" || code == "M0309") && reason.length)
            msg ~= "; " ~ reason;
        else code = "M0301";
        if (context.length) msg = context ~ ": " ~ msg;
        auto hint = conversion_hint(e, f);
        if (hint.length) msg ~= "; " ~ hint;
        error(code, msg, sp);
    }

    // ---- names & suggestions ----
    static size_t edit_distance(string a, string b)
    {
        auto prev = new size_t[b.length + 1];
        auto cur = new size_t[b.length + 1];
        foreach (j; 0 .. b.length + 1) prev[j] = j;
        foreach (i; 1 .. a.length + 1)
        {
            cur[0] = i;
            foreach (j; 1 .. b.length + 1)
                cur[j] = min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1));
            auto tmp = prev; prev = cur; cur = tmp;
        }
        return prev[b.length];
    }
    static string closest(string name, const(string)[] cands)
    {
        string best;
        size_t bd = 3;
        foreach (c; cands)
        {
            if (!c.length || c[0] == '$' || c[0] == '#' || c == name) continue;
            auto d = edit_distance(name, c);
            if (d < bd || (d == bd && best.length && c < best))
            {
                bd = d;
                best = c;
            }
        }
        if (bd > 2 || bd >= name.length) return "";
        return best;
    }
    string[] visible_vars(Scope scope_)
    {
        string[] result;
        for (Scope s = scope_; s !is null; s = s.parent) foreach (k; s.vars.keys) result ~= k;
        return result;
    }

    // ---- module paths ----
    Scope resolve_mpath(const(string)[] path, Scope scope_, Span sp)
    {
        auto m = scope_.module_(path[0]);
        if (m is null) error("M0501", "unbound module `" ~ path[0] ~ "`", sp);
        Scope cur = *m;
        foreach (i; 1 .. path.length)
        {
            auto n = cur.modules.find(path[i]);
            if (n is null)
                error("M0501", "module `" ~ join_strings(path[0 .. i], ".") ~ "` has no submodule `"
                    ~ path[i] ~ "`", sp);
            cur = *n;
        }
        return cur;
    }

    // ---- type expressions ----
    static class TVarMap
    {
        TypeNode[string] vars;
        bool allow_new = true;
    }

    TypeNode apply_typedef(TypeDef d, TypeNode[] args, Span sp)
    {
        if (args.length != d.params.length)
            error("M0210", "type `" ~ d.display ~ "` expects " ~ to!string(d.params.length)
                ~ " argument(s) but was given " ~ to!string(args.length), sp);
        if (d.alias_)
        {
            if (d.body is null) error("M0210", "type alias `" ~ d.display ~ "` refers to itself", sp);
            TypeNode[TypeNode] m;
            foreach (i; 0 .. args.length) m[d.params[i]] = args[i];
            return subst(d.body, m);
        }
        return t_con(d.display, d.stamp, args);
    }

    TypeNode tvar_named(string name, TVarMap m, Span sp)
    {
        if (auto p = name in m.vars) return *p;
        if (!m.allow_new) error("M0210", "unbound type variable `" ~ name ~ "`", sp);
        auto v = fresh();
        v.hint = name;
        m.vars[name] = v;
        return v;
    }

    TypeNode resolve_type(TypeExpr te, Scope scope_, TVarMap m)
    {
        foreach (n; te.num_vars) tvar_named(n, m, te.span).num = true;
        final switch (te.kind)
        {
            case TEK.Var: return tvar_named(te.name, m, te.span);
            case TEK.Unit: return t_unit();
            case TEK.Fun:
            {
                auto a = resolve_type(te.kids[0], scope_, m);
                return t_fun(a, resolve_type(te.kids[1], scope_, m));
            }
            case TEK.Tuple:
            {
                TypeNode[] items;
                foreach (k; te.kids) items ~= resolve_type(k, scope_, m);
                return t_tuple(items);
            }
            case TEK.List: return t_list(resolve_type(te.kids[0], scope_, m));
            case TEK.Record:
            {
                TypeNode row = !te.row.length ? row_empty() : tvar_named(te.row, m, te.span);
                bool[string] seen;
                for (size_t i = te.kids.length; i-- > 0;)
                {
                    if (te.labels[i] in seen)
                        error("M0208", "duplicate field `" ~ te.labels[i] ~ "` in record type", te.span);
                    seen[te.labels[i]] = true;
                    auto ft = resolve_type(te.kids[i], scope_, m);
                    row = row_ext(te.labels[i], ft, row);
                }
                return t_record(row);
            }
            case TEK.Name:
            {
                TypeNode[] args;
                foreach (k; te.kids) args ~= resolve_type(k, scope_, m);
                if (te.path.length)
                {
                    auto ms = resolve_mpath(te.path, scope_, te.span);
                    auto d = ms.types.find(te.name);
                    if (d is null)
                        error("M0210", "module `" ~ join_strings(te.path, ".") ~ "` has no type `"
                            ~ te.name ~ "`", te.span);
                    return apply_typedef(*d, args, te.span);
                }
                if (auto d = scope_.type(te.name)) return apply_typedef(*d, args, te.span);
                if (is_lower(te.name[0]) && !args.length) return tvar_named(te.name, m, te.span);
                string[] cands;
                for (Scope s = scope_; s !is null; s = s.parent) foreach (k; s.types.keys) cands ~= k;
                auto near = closest(te.name, cands);
                error("M0210", "unbound type `" ~ te.name ~ "`", te.span,
                    near.length ? ["did you mean `" ~ near ~ "`?"] : null);
            }
        }
    }

    /// Resolves a signature type into a generic scheme.
    TypeNode sig_scheme(TypeExpr te, Scope scope_)
    {
        auto m = new TVarMap;
        ++level;
        auto t = resolve_type(te, scope_, m);
        --level;
        generalize(t, level);
        return t;
    }

    // ---- builtin operators ----
    TypeNode builtin_op_type(string op)
    {
        if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%")
        {
            auto a = fresh_num();
            return t_fun(a, t_fun(a, a));
        }
        if (op == "==" || op == "!=" || op == "=" || op == "<>" || op == "<" || op == ">"
            || op == "<=" || op == ">=")
        {
            auto a = fresh();
            return t_fun(a, t_fun(a, t_bool()));
        }
        if (op == "&&" || op == "||") return t_fun(t_bool(), t_fun(t_bool(), t_bool()));
        if (op == "^") return t_fun(t_string(), t_fun(t_string(), t_string()));
        if (op == "@")
        {
            auto a = t_list(fresh());
            return t_fun(a, t_fun(a, a));
        }
        if (op == "::")
        {
            auto a = fresh();
            return t_fun(a, t_fun(t_list(a), t_list(a)));
        }
        auto a = fresh(), b = fresh(), c = fresh();
        if (op == ">>") return t_fun(t_fun(a, b), t_fun(t_fun(b, c), t_fun(a, c)));
        if (op == "|>") return t_fun(a, t_fun(t_fun(a, b), b));
        return t_fun(t_fun(b, c), t_fun(t_fun(a, b), t_fun(a, c)));
    }

    // ---- patterns ----
    void bind_local(Scope scope_, string name, TypeNode t) { scope_.vars.set(name, VarInfo(t, null)); }

    void check_pat(Pat p, TypeNode t, Scope scope_, ref bool[string] names)
    {
        final switch (p.kind)
        {
            case PK.Wild: return;
            case PK.Var:
                if (p.text in names)
                    error("M0208", "the name `" ~ p.text ~ "` is bound twice in this pattern", p.span);
                names[p.text] = true;
                bind_local(scope_, p.text, t);
                return;
            case PK.Int: unify_or_error(t, t_int(), p.span, "in this pattern"); return;
            case PK.Float: unify_or_error(t, t_float(), p.span, "in this pattern"); return;
            case PK.Str: unify_or_error(t, t_string(), p.span, "in this pattern"); return;
            case PK.Bool: unify_or_error(t, t_bool(), p.span, "in this pattern"); return;
            case PK.Unit: unify_or_error(t, t_unit(), p.span, "in this pattern"); return;
            case PK.Tuple:
            {
                TypeNode[] items;
                foreach (i; 0 .. p.kids.length) items ~= fresh();
                unify_or_error(t, t_tuple(items), p.span, "in this pattern");
                foreach (i; 0 .. p.kids.length) check_pat(p.kids[i], items[i], scope_, names);
                return;
            }
            case PK.List:
            {
                auto el = fresh();
                unify_or_error(t, t_list(el), p.span, "in this pattern");
                foreach (k; p.kids) check_pat(k, el, scope_, names);
                return;
            }
            case PK.Cons:
            {
                auto el = fresh();
                unify_or_error(t, t_list(el), p.span, "in this pattern");
                check_pat(p.kids[0], el, scope_, names);
                check_pat(p.kids[1], t_list(el), scope_, names);
                return;
            }
            case PK.Ctor:
            {
                auto ci = lookup_ctor(p.text, p.path, scope_, p.span);
                p.ctor_index = ci.index;
                p.ctor_arity = ci.arity;
                auto ty = instantiate(ci.scheme);
                TypeNode[] args;
                foreach (i; 0 .. ci.arity)
                {
                    auto f = prune(ty);
                    args ~= f.args[0];
                    ty = f.args[1];
                }
                Pat[] kids = p.kids;
                if (ci.arity == 1 && kids.length > 1)
                {
                    auto tup = new Pat;
                    tup.kind = PK.Tuple;
                    tup.span = p.span;
                    tup.kids = kids;
                    kids = [tup];
                    p.kids = kids;
                }
                if (cast(int) kids.length != ci.arity)
                    error("M0304", "constructor `" ~ p.text ~ "` expects " ~ to!string(ci.arity)
                        ~ " argument(s) but the pattern has " ~ to!string(kids.length), p.span);
                unify_or_error(t, ty, p.span, "in this pattern");
                foreach (i; 0 .. kids.length) check_pat(kids[i], args[i], scope_, names);
                return;
            }
            case PK.Record:
            {
                TypeNode row = fresh();
                TypeNode[] fts;
                for (size_t i = p.kids.length; i-- > 0;)
                {
                    auto ft = fresh();
                    fts = ft ~ fts;
                    row = row_ext(p.labels[i], ft, row);
                }
                unify_or_error(t, t_record(row), p.span, "in this pattern");
                foreach (i; 0 .. p.kids.length) check_pat(p.kids[i], fts[i], scope_, names);
                return;
            }
            case PK.Annot:
            {
                auto m = new TVarMap;
                auto at = resolve_type(p.annot, scope_, m);
                unify_or_error(at, t, p.span, "in this annotated pattern");
                check_pat(p.kids[0], at, scope_, names);
                return;
            }
        }
    }

    CtorInfo lookup_ctor(string name, string[] path, Scope scope_, Span sp)
    {
        if (path.length)
        {
            auto ms = resolve_mpath(path, scope_, sp);
            if (auto c = ms.ctors.find(name)) return *c;
            error("M0209", "module `" ~ join_strings(path, ".") ~ "` has no constructor `" ~ name ~ "`", sp);
        }
        if (auto c = scope_.ctor(name)) return *c;
        if (scope_.module_(name) !is null) error("M0209", "module `" ~ name ~ "` cannot be used as a value", sp);
        string[] cands;
        for (Scope s = scope_; s !is null; s = s.parent) foreach (k; s.ctors.keys) cands ~= k;
        auto near = closest(name, cands);
        error("M0209", "unbound constructor `" ~ name ~ "`", sp,
            near.length ? ["did you mean `" ~ near ~ "`?"] : null);
    }

    // ---- expressions ----
    static bool is_concat(Expr e)
    {
        if ((e.kind == EK.App || e.kind == EK.Pipe) && e.resolution != Resolution.Apply) return true;
        return e.kind == EK.BinOp && (e.text == ">>" || e.text == "<<" || e.text == "\xE2\x88\x98");
    }

    TypeNode infer_var(Expr e, Scope scope_)
    {
        string n = e.text;
        auto vi = scope_.var(n);
        if (vi is null)
        {
            if (e.kind == EK.Tacit) error("M0206", "tacit parameter `" ~ n ~ "` is not bound here", e.span);
            auto near = closest(n, visible_vars(scope_));
            string[] sugg;
            if (near.length) sugg ~= "did you mean `" ~ near ~ "`?";
            error("M0204", "unbound variable `" ~ n ~ "`", e.span, sugg);
        }
        e.cell = vi.cell;
        return instantiate(vi.scheme);
    }

    TypeNode infer_path(Expr e, Scope scope_)
    {
        auto mp = e.path[0 .. $ - 1];
        string field = e.path[$ - 1];
        e.prim = null;
        e.cell = null;
        auto m = scope_.module_(mp[0]);
        string full = join_strings(e.path, ".");
        if (m !is null)
        {
            Scope cur = *m;
            bool ok = true;
            foreach (i; 1 .. mp.length)
            {
                auto n = cur.modules.find(mp[i]);
                if (n is null) { ok = false; break; }
                cur = *n;
            }
            if (ok)
                if (auto vi = cur.vars.find(field)) return instantiate(vi.scheme);
        }
        if (find_prim !is null)
            if (auto p = find_prim(full))
            {
                e.prim = p;
                return instantiate(prim_type(p, e.span));
            }
        if (m is null)
            error("M0501", "unbound module `" ~ mp[0] ~ "` (and no host primitive `" ~ full ~ "`)", e.span);
        auto ms = resolve_mpath(mp, scope_, e.span);
        auto near = closest(field, ms.vars.keys);
        error("M0506", "module `" ~ join_strings(mp, ".") ~ "` has no member `" ~ field ~ "`", e.span,
            near.length ? ["did you mean `" ~ near ~ "`?"] : null);
    }

    void lambda_warnings(Expr e)
    {
        if (quiet || e.tacit_count > 0) return;
        bool[string] fv;
        ParserImpl.free_vars(e.kids[0], null, fv);
        foreach (p; e.pats)
        {
            bool[string] names;
            ParserImpl.pat_names(p, names);
            foreach (n; sortedKeys(names))
                if (n[0] != '_' && n[0] != '#' && n[0] != '$' && n !in fv)
                    warn("M0307", "unused parameter `" ~ n ~ "`", p.span);
        }
    }

    /// Binds lambda parameters (optionally against known types) in `inner`.
    TypeNode[] bind_params(Expr e, Scope inner, TypeNode[] known)
    {
        bool[string] names;
        TypeNode[] pts;
        foreach (i; 0 .. e.pats.length)
        {
            auto pt = i < known.length ? known[i] : fresh();
            check_pat(e.pats[i], pt, inner, names);
            pts ~= pt;
        }
        for (int k = 1; k <= e.tacit_count && k <= cast(int) pts.length; ++k)
            bind_local(inner, "$" ~ to!string(k), pts[k - 1]);
        return pts;
    }

    TypeNode infer(Expr e, Scope scope_)
    {
        final switch (e.kind)
        {
            case EK.Int: return t_int();
            case EK.Float: return t_float();
            case EK.Str: return t_string();
            case EK.Bool: return t_bool();
            case EK.Unit: return t_unit();
            case EK.Var: return infer_var(e, scope_);
            case EK.Tacit:
                e.text = "$" ~ to!string(e.ival);
                return infer_var(e, scope_);
            case EK.Path: return infer_path(e, scope_);
            case EK.Ctor:
            {
                auto ci = lookup_ctor(e.text, e.path, scope_, e.span);
                e.ctor_index = ci.index;
                e.ctor_arity = ci.arity;
                return instantiate(ci.scheme);
            }
            case EK.App:
            {
                auto tf = infer(e.kids[0], scope_);
                auto tx = infer(e.kids[1], scope_);
                return resolve_juxtaposition(e, tf, tx, e.kids[0], e.kids[1], false);
            }
            case EK.Pipe:
            {
                auto ta = infer(e.kids[0], scope_);
                auto tb = infer(e.kids[1], scope_);
                return resolve_juxtaposition(e, tb, ta, e.kids[1], e.kids[0], true);
            }
            case EK.BinOp:
            {
                TypeNode ft;
                if (is_builtin_op(e.text)) ft = builtin_op_type(e.text);
                else
                {
                    auto vi = scope_.var(e.text);
                    if (vi is null) error("M0204", "unbound operator `" ~ e.text ~ "`", e.span);
                    ft = instantiate(vi.scheme);
                }
                auto ta = infer(e.kids[0], scope_);
                auto tb = infer(e.kids[1], scope_);
                auto r = fresh();
                auto f = prune(ft);
                if (is_fun(f))
                {
                    unify_or_error(f.args[0], ta, e.kids[0].span, "in the left operand of `" ~ e.text ~ "`");
                    auto g = prune(f.args[1]);
                    if (is_fun(g))
                    {
                        unify_or_error(g.args[0], tb, e.kids[1].span,
                            "in the right operand of `" ~ e.text ~ "`");
                        return g.args[1];
                    }
                }
                unify_or_error(ft, t_fun(ta, t_fun(tb, r)), e.span, "in the operator `" ~ e.text ~ "`");
                return r;
            }
            case EK.Neg:
            {
                auto t = infer(e.kids[0], scope_);
                auto n = fresh_num();
                unify_or_error(n, t, e.kids[0].span, "in this negation");
                return n;
            }
            case EK.Lambda:
            {
                auto inner = Scope.child(scope_);
                auto pts = bind_params(e, inner, null);
                lambda_warnings(e);
                TypeNode t = infer(e.kids[0], inner);
                for (size_t i = pts.length; i-- > 0;) t = t_fun(pts[i], t);
                return t;
            }
            case EK.If:
            {
                unify_or_error(t_bool(), infer(e.kids[0], scope_), e.kids[0].span, "in the condition of `if`");
                auto a = infer(e.kids[1], scope_);
                auto b = infer(e.kids[2], scope_);
                unify_or_error(a, b, e.kids[2].span,
                    "the `else` branch must have the same type as the `then` branch");
                return a;
            }
            case EK.Match:
            {
                auto ts = infer(e.kids[0], scope_);
                auto r = fresh();
                foreach (i; 0 .. e.pats.length)
                {
                    auto inner = Scope.child(scope_);
                    bool[string] names;
                    check_pat(e.pats[i], ts, inner, names);
                    if (e.guards[i] !is null)
                        unify_or_error(t_bool(), infer(e.guards[i], inner), e.guards[i].span, "in this guard");
                    auto tb = infer(e.kids[i + 1], inner);
                    unify_or_error(r, tb, e.kids[i + 1].span, "all branches of `match` must have the same type");
                }
                return r;
            }
            case EK.Let:
            {
                ++level;
                auto t1 = infer(e.kids[0], scope_);
                --level;
                auto inner = Scope.child(scope_);
                bool[string] names;
                check_pat(e.pats[0], t1, inner, names);
                foreach (n; inner.vars.keys) generalize(inner.vars.find(n).scheme, level);
                return infer(e.kids[1], inner);
            }
            case EK.LetRec:
            {
                auto inner = Scope.child(scope_);
                ++level;
                TypeNode[] monos;
                foreach (p; e.pats)
                {
                    auto m = fresh();
                    monos ~= m;
                    bind_local(inner, p.text, m);
                }
                foreach (i; 0 .. e.pats.length)
                    unify_or_error(monos[i], infer(e.kids[i], inner), e.kids[i].span,
                        "in the recursive binding `" ~ e.pats[i].text ~ "`");
                --level;
                foreach (m; monos) generalize(m, level);
                return infer(e.kids[$ - 1], inner);
            }
            case EK.List:
            {
                auto el = fresh();
                foreach (k; e.kids)
                    unify_or_error(el, infer(k, scope_), k.span, "list elements must all have the same type");
                return t_list(el);
            }
            case EK.Comp:
            {
                auto inner = Scope.child(scope_);
                foreach (c; e.clauses)
                {
                    if (c.pat is null)
                    {
                        unify_or_error(t_bool(), infer(c.expr, inner), c.expr.span, "in this comprehension guard");
                        continue;
                    }
                    auto el = fresh();
                    unify_or_error(t_list(el), infer(c.expr, inner), c.expr.span,
                        "a comprehension generator must be a list");
                    auto next = Scope.child(inner);
                    bool[string] names;
                    check_pat(c.pat, el, next, names);
                    inner = next;
                }
                return t_list(infer(e.kids[0], inner));
            }
            case EK.Range:
                unify_or_error(t_int(), infer(e.kids[0], scope_), e.kids[0].span, "in the range start");
                unify_or_error(t_int(), infer(e.kids[1], scope_), e.kids[1].span, "in the range end");
                return t_list(t_int());
            case EK.Tuple:
            {
                TypeNode[] items;
                foreach (k; e.kids) items ~= infer(k, scope_);
                return t_tuple(items);
            }
            case EK.Record:
            {
                TypeNode row = row_empty();
                TypeNode[] fts;
                foreach (k; e.kids) fts ~= infer(k, scope_);
                for (size_t i = e.kids.length; i-- > 0;) row = row_ext(e.labels[i], fts[i], row);
                return t_record(row);
            }
            case EK.Update:
            {
                auto tb = infer(e.kids[0], scope_);
                foreach (i; 0 .. e.labels.length)
                {
                    auto ft = fresh();
                    unify_or_error(t_record(row_ext(e.labels[i], ft, fresh())), tb, e.kids[0].span,
                        "in this record update");
                    unify_or_error(ft, infer(e.kids[i + 1], scope_), e.kids[i + 1].span,
                        "in the field `" ~ e.labels[i] ~ "`");
                }
                return tb;
            }
            case EK.Project:
            {
                auto t = infer(e.kids[0], scope_);
                auto ft = fresh();
                unify_or_error(t_record(row_ext(e.text, ft, fresh())), t, e.kids[0].span,
                    "in the projection `." ~ e.text ~ "`");
                return ft;
            }
            case EK.OpRef:
            {
                if (is_builtin_op(e.text)) return builtin_op_type(e.text);
                auto vi = scope_.var(e.text);
                if (vi is null) error("M0204", "unbound operator `" ~ e.text ~ "`", e.span);
                e.cell = vi.cell;
                return instantiate(vi.scheme);
            }
            case EK.Annot:
            {
                auto m = new TVarMap;
                auto at = resolve_type(e.annot, scope_, m);
                unify_or_error(at, infer(e.kids[0], scope_), e.kids[0].span, "in this type annotation");
                return at;
            }
        }
    }

    /// §5.4: `f x` is application unless types require composition.
    TypeNode resolve_juxtaposition(Expr e, TypeNode tf, TypeNode tx, Expr fe, Expr xe, bool pipe)
    {
        {
            auto snap = u.snapshot();
            auto r = fresh();
            if (u.unify(tf, t_fun(tx, r)))
            {
                e.resolution = Resolution.Apply;
                return r;
            }
            u.rollback(snap);
        }
        if (is_fun(tf) && is_fun(tx))
        {
            TypeNode first = pipe ? tx : tf;
            TypeNode second = pipe ? tf : tx;
            auto a = fresh(), b = fresh(), c = fresh();
            auto snap = u.snapshot();
            if (u.unify(first, t_fun(a, b)) && u.unify(second, t_fun(b, c)))
            {
                e.resolution = Resolution.ComposeFwd;
                return t_fun(a, c);
            }
            u.rollback(snap);
            if (!pipe)
            {
                snap = u.snapshot();
                if (u.unify(second, t_fun(a, b)) && u.unify(first, t_fun(b, c)))
                {
                    e.resolution = Resolution.ComposeBwd;
                    return t_fun(a, c);
                }
                u.rollback(snap);
            }
        }
        auto f = prune(tf);
        if (is_fun(f))
        {
            unify_or_error(f.args[0], tx, xe.span, pipe ? "in the value piped by `|>`" : "in this argument");
            error("M0301", "cannot apply this function", e.span);
        }
        auto p = new TypePrinter;
        error("M0301", (pipe ? "the right side of `|>`" : "this expression") ~ " has type `"
            ~ p.print_bare(tf) ~ "` and cannot be applied; it is not a function", fe.span);
    }

    // ---- definitions ----
    noreturn sig_mismatch(Item def, string decl_text, TypeNode actual)
    {
        string code = u.reason_code, reason = u.reason;
        auto p = new TypePrinter;
        string msg = "`" ~ def.name ~ "` is declared as `" ~ decl_text
            ~ "` but its definition has type `" ~ p.print(actual) ~ "`";
        if ((code == "M0302" || code == "M0303" || code == "M0306" || code == "M0309") && reason.length)
            msg ~= "; " ~ reason;
        error("M0305", msg, def.span);
    }

    /// Checks a definition against its declared signature (bidirectional;
    /// concat bodies may take the stack top as an extra argument, §5.4).
    TypeNode check_def_with_sig(Item def, TypeNode scheme, Scope scope_, TypeExpr sig_te)
    {
        auto ts = skolemize(scheme);
        Expr v = def.value;
        string decl_text;
        {
            auto p = new TypePrinter;
            decl_text = p.print(ts);
        }
        if (v.kind == EK.Lambda)
        {
            auto inner = Scope.child(scope_);
            TypeNode[] known;
            TypeNode rest = ts;
            bool short_sig = false;
            foreach (i; 0 .. v.pats.length)
            {
                auto r = prune(rest);
                if (is_fun(r))
                {
                    known ~= r.args[0];
                    rest = r.args[1];
                }
                else
                {
                    short_sig = true;
                    break;
                }
            }
            auto pts = bind_params(v, inner, known);
            lambda_warnings(v);
            auto body = infer(v.kids[0], inner);
            TypeNode full = body;
            for (size_t i = pts.length; i-- > 0;) full = t_fun(pts[i], full);
            auto snap = u.snapshot();
            if (!short_sig)
            {
                if (u.unify(rest, body)) return ts;
                u.rollback(snap);
                auto bp = prune(body);
                if (is_concat(v.kids[0]) && is_fun(bp))
                {
                    if (u.unify(rest, bp.args[1])) return full;
                    u.rollback(snap);
                }
                u.unify(rest, body);
                u.rollback(snap);
                sig_mismatch(def, decl_text, full);
            }
            if (u.unify(ts, full)) return ts;
            u.rollback(snap);
            u.unify(ts, full);
            u.rollback(snap);
            sig_mismatch(def, decl_text, full);
        }
        auto vt = infer(v, scope_);
        auto snap = u.snapshot();
        if (u.unify(ts, vt)) return ts;
        u.rollback(snap);
        auto vp = prune(vt);
        if (is_concat(v) && is_fun(vp))
        {
            if (u.unify(ts, vp.args[1])) return vt;
            u.rollback(snap);
        }
        u.unify(ts, vt);
        u.rollback(snap);
        sig_mismatch(def, decl_text, vt);
    }

    // ---- units & items ----
    static UnitRange[] units(Item[] items)
    {
        UnitRange[] result;
        size_t i = 0;
        while (i < items.length)
        {
            IK k = items[i].kind;
            size_t j = i + 1;
            if (k == IK.Def || k == IK.Sig)
            {
                bool[string] defs;
                if (k == IK.Def) defs[items[i].name] = true;
                while (j < items.length && (items[j].kind == IK.Def || items[j].kind == IK.Sig))
                {
                    if (items[j].kind == IK.Def)
                    {
                        if (items[j].name in defs) break;
                        defs[items[j].name] = true;
                    }
                    if (items[j].kind == IK.Sig && items[j].name in defs) break;
                    ++j;
                }
            }
            else if (k == IK.Type)
                while (j < items.length && items[j].kind == IK.Type) ++j;
            result ~= UnitRange(i, j);
            i = j;
        }
        return result;
    }

    /// Checks one unit. `exports` receives exported members (structures);
    /// `global` creates runtime cells for top-level bindings.
    UnitInfo check_unit(Item[] items, size_t b, size_t e, Scope scope_, Scope exports, bool global_)
    {
        UnitInfo info;
        IK k = items[b].kind;
        if (k == IK.Def || k == IK.Sig) return check_defs(items, b, e, scope_, exports, global_);
        if (k == IK.Type)
        {
            check_types(items, b, e, scope_, exports);
            return info;
        }
        Item it = items[b];
        switch (k)
        {
            case IK.Module:
            {
                string prefix = scope_.prefix ~ it.name ~ ".";
                auto s = check_mexpr(it.mexpr, scope_, prefix);
                if (it.asc != Asc.None)
                {
                    s = check_ascription(s, it.sig, scope_, it.asc, prefix, it.span);
                    it.sealed = it.asc == Asc.Opaque;
                    it.visible_values = s.vars.keys.dup;
                    it.visible_modules = s.modules.keys.dup;
                }
                scope_.modules.set(it.name, s);
                if (exports !is null) exports.modules.set(it.name, s);
                return info;
            }
            case IK.Functor:
            {
                auto param = sig_to_module(it.psig, scope_, it.pname ~ ".", it.span);
                auto env2 = Scope.child(scope_, scope_.prefix ~ it.name ~ ".");
                env2.modules.set(it.pname, param);
                auto s = check_struct(it.mexpr, env2, scope_.prefix ~ it.name ~ ".");
                if (it.asc != Asc.None)
                {
                    s = check_ascription(s, it.sig, env2, it.asc, scope_.prefix ~ it.name ~ ".", it.span);
                    it.sealed = it.asc == Asc.Opaque;
                    it.visible_values = s.vars.keys.dup;
                    it.visible_modules = s.modules.keys.dup;
                }
                auto fi = new FunctorInfo;
                fi.name = it.name;
                fi.item = it;
                fi.env = scope_;
                scope_.functors.set(it.name, fi);
                if (exports !is null) exports.functors.set(it.name, fi);
                return info;
            }
            case IK.ModType:
            {
                auto sd = new SigDef;
                sd.name = it.name;
                sd.expr = it.sig;
                sd.env = scope_;
                sig_to_module(it.sig, scope_, it.name ~ ".", it.span);
                scope_.sigs.set(it.name, sd);
                if (exports !is null) exports.sigs.set(it.name, sd);
                return info;
            }
            case IK.Include:
            case IK.Open:
            {
                auto s = resolve_mpath(it.path, scope_, it.span);
                bool inc = k == IK.Include;
                it.cells = null;
                foreach (n; s.vars.keys)
                {
                    VarInfo vi = *s.vars.find(n);
                    vi.cell = null;
                    if (global_)
                    {
                        vi.cell = new Cell;
                        vi.cell.name = n;
                        it.cells ~= vi.cell;
                    }
                    scope_.vars.set(n, vi);
                    if (inc && exports !is null) exports.vars.set(n, vi);
                }
                foreach (n; s.types.keys)
                {
                    scope_.types.set(n, *s.types.find(n));
                    if (inc && exports !is null) exports.types.set(n, *s.types.find(n));
                }
                foreach (n; s.ctors.keys)
                {
                    scope_.ctors.set(n, *s.ctors.find(n));
                    if (inc && exports !is null) exports.ctors.set(n, *s.ctors.find(n));
                }
                foreach (n; s.modules.keys)
                {
                    scope_.modules.set(n, *s.modules.find(n));
                    if (inc && exports !is null) exports.modules.set(n, *s.modules.find(n));
                }
                foreach (n; s.functors.keys)
                {
                    scope_.functors.set(n, *s.functors.find(n));
                    if (inc && exports !is null) exports.functors.set(n, *s.functors.find(n));
                }
                foreach (n; s.sigs.keys) scope_.sigs.set(n, *s.sigs.find(n));
                return info;
            }
            case IK.Import:
                if (!global_) error("M0505", "`import` is only allowed at the top level", it.span);
                return info;
            case IK.Directive:
                if (!global_)
                    error("M0400", "directive `#" ~ it.name ~ "` is not allowed inside a structure", it.span);
                return info;
            case IK.Expr:
            {
                ++level;
                auto t = infer(it.body, scope_);
                --level;
                generalize(t, level);
                info.expr_type = t;
                return info;
            }
            default:
                break;
        }
        return info;
    }

    UnitInfo check_defs(Item[] items, size_t b, size_t e, Scope scope_, Scope exports, bool global_)
    {
        UnitInfo info;
        Item[] defs;
        Item[string] sigs;
        foreach (i; b .. e)
        {
            if (items[i].kind == IK.Sig) sigs[items[i].name] = items[i];
            else defs ~= items[i];
        }
        bool[string] defined;
        foreach (d; defs) defined[d.name] = true;
        foreach (i; b .. e)
            if (items[i].kind == IK.Sig && items[i].name !in defined)
            {
                sig_scheme(items[i].type, scope_);
                scope_.pending.set(items[i].name, items[i]);
            }
        auto sig_te = new TypeExpr[defs.length];
        foreach (i, d; defs)
        {
            if (d.type !is null) sig_te[i] = d.type;
            else if (auto sp = d.name in sigs) sig_te[i] = sp.type;
            else
                for (Scope s = scope_; s !is null; s = s.parent)
                    if (auto p = s.pending.find(d.name))
                    {
                        sig_te[i] = p.type;
                        s.pending.erase(d.name);
                        break;
                    }
        }
        // dependency graph & strongly connected components (DSLtk DirectedGraph)
        auto g = new DirectedGraph!(string, string)();
        size_t[string] index;
        foreach (i, d; defs)
        {
            g.add_node(d.name);
            index[d.name] = i;
        }
        auto deps = new bool[size_t][defs.length];
        foreach (i, d; defs)
        {
            bool[string] fv;
            ParserImpl.free_vars(d.value, null, fv);
            foreach (n; sortedKeys(fv))
                if (auto j = n in index)
                {
                    g.add_edge(i, *j);
                    deps[i][*j] = true;
                }
        }
        auto comps = g.strongly_connected_components();
        auto comp_of = new size_t[defs.length];
        size_t[][] groups;
        foreach (c; comps)
        {
            size_t[] grp = c.dup;
            grp.sort();
            foreach (n; grp) comp_of[n] = groups.length;
            groups ~= grp;
        }
        auto done = new bool[groups.length];
        size_t[][] order;
        foreach (round; 0 .. groups.length)
        {
            size_t pick = groups.length;
            foreach (gi; 0 .. groups.length)
            {
                if (done[gi]) continue;
                bool ready = true;
                foreach (n; groups[gi])
                    foreach (dd; deps[n].keys)
                        if (comp_of[dd] != gi && !done[comp_of[dd]]) ready = false;
                if (ready && (pick == groups.length || groups[gi][0] < groups[pick][0])) pick = gi;
            }
            done[pick] = true;
            order ~= groups[pick];
        }
        auto final_types = new TypeNode[defs.length];
        foreach (grp; order)
        {
            ++level;
            auto schemes = new TypeNode[defs.length];
            auto monos = new TypeNode[defs.length];
            foreach (i; grp)
            {
                auto d = defs[i];
                Cell cell;
                if (global_)
                {
                    cell = new Cell;
                    cell.name = d.name;
                    d.cells = [cell];
                }
                if (sig_te[i] !is null)
                {
                    --level;
                    schemes[i] = sig_scheme(sig_te[i], scope_);
                    ++level;
                    scope_.vars.set(d.name, VarInfo(schemes[i], cell));
                }
                else
                {
                    monos[i] = fresh();
                    scope_.vars.set(d.name, VarInfo(monos[i], cell));
                }
            }
            auto results = new TypeNode[defs.length];
            foreach (i; grp)
            {
                auto d = defs[i];
                if (sig_te[i] !is null) results[i] = check_def_with_sig(d, schemes[i], scope_, sig_te[i]);
                else
                {
                    auto t = infer(d.value, scope_);
                    unify_or_error(monos[i], t, d.span, "in the recursive use of `" ~ d.name ~ "`");
                    results[i] = monos[i];
                }
            }
            --level;
            foreach (i; grp)
            {
                generalize(results[i], level);
                Cell cell = global_ ? defs[i].cells[0] : null;
                scope_.vars.set(defs[i].name, VarInfo(results[i], cell));
                if (exports !is null) exports.vars.set(defs[i].name, VarInfo(results[i], null));
                final_types[i] = results[i];
            }
        }
        foreach (i, d; defs) info.defs ~= tuple(d.name, final_types[i]);
        return info;
    }

    void check_types(Item[] items, size_t b, size_t e, Scope scope_, Scope exports)
    {
        TypeDef[] defs;
        foreach (i; b .. e)
        {
            auto it = items[i];
            auto d = new TypeDef;
            d.name = it.name;
            d.display = scope_.prefix ~ it.name;
            d.stamp = next_stamp++;
            d.alias_ = it.tkind == 1;
            bool[string] seen;
            foreach (p; it.tparams)
            {
                if (p in seen) error("M0208", "duplicate type parameter `" ~ p ~ "`", it.span);
                seen[p] = true;
                auto v = generic_var();
                v.hint = p;
                d.params ~= v;
            }
            scope_.types.set(it.name, d);
            if (exports !is null) exports.types.set(it.name, d);
            defs ~= d;
        }
        foreach (i; b .. e)
        {
            auto it = items[i];
            auto d = defs[i - b];
            auto m = new TVarMap;
            m.allow_new = false;
            foreach (k; 0 .. it.tparams.length) m.vars[it.tparams[k]] = d.params[k];
            if (it.tkind == 1) d.body = resolve_type(it.type, scope_, m);
            else if (it.tkind == 2)
            {
                bool[string] seen;
                foreach (c; 0 .. it.ctors.length)
                {
                    auto cd = it.ctors[c];
                    if (cd.name in seen) error("M0208", "duplicate constructor `" ~ cd.name ~ "`", cd.span);
                    seen[cd.name] = true;
                    TypeNode[] args;
                    foreach (a; cd.args) args ~= resolve_type(a, scope_, m);
                    TypeNode t = t_con(d.display, d.stamp, d.params);
                    for (size_t kk = args.length; kk-- > 0;) t = t_fun(args[kk], t);
                    auto ci = new CtorInfo;
                    ci.name = cd.name;
                    ci.index = cast(int) c;
                    ci.arity = cast(int) args.length;
                    ci.scheme = t;
                    ci.owner = d;
                    d.ctors ~= ci;
                    scope_.ctors.set(cd.name, ci);
                    if (exports !is null) exports.ctors.set(cd.name, ci);
                }
            }
        }
    }

    // ---- modules ----
    Scope check_struct(ModExpr m, Scope env, string prefix)
    {
        auto inner = Scope.child(env, prefix);
        auto exports = new Scope;
        exports.prefix = prefix;
        foreach (r; units(m.items)) check_unit(m.items, r.b, r.e, inner, exports, false);
        foreach (n; inner.pending.keys)
            error("M0502", "`" ~ n ~ "` is declared in this structure but never defined",
                (*inner.pending.find(n)).span);
        return exports;
    }

    FunctorInfo find_functor(string[] path, Scope scope_, Span sp)
    {
        if (path.length == 1)
        {
            if (auto f = scope_.functor(path[0])) return *f;
            if (scope_.module_(path[0]) !is null) error("M0503", "module `" ~ path[0] ~ "` is not a functor", sp);
            error("M0503", "unbound functor `" ~ path[0] ~ "`", sp);
        }
        auto ms = resolve_mpath(path[0 .. $ - 1], scope_, sp);
        if (auto f = ms.functors.find(path[$ - 1])) return *f;
        error("M0503", "unbound functor `" ~ join_strings(path, ".") ~ "`", sp);
    }

    Scope check_mexpr(ModExpr m, Scope scope_, string prefix)
    {
        final switch (m.kind)
        {
            case MK.Struct: return check_struct(m, scope_, prefix);
            case MK.Path: return resolve_mpath(m.path, scope_, m.span);
            case MK.Apply:
            {
                auto fi = find_functor(m.path, scope_, m.span);
                auto arg = check_mexpr(m.arg, scope_, prefix);
                auto fit = fi.item;
                check_ascription(arg, fit.psig, fi.env, Asc.Transparent, prefix, m.arg.span);
                auto env2 = Scope.child(fi.env, prefix);
                env2.modules.set(fit.pname, arg);
                bool q = quiet;
                quiet = true;
                scope (exit) quiet = q;
                auto s = check_struct(fit.mexpr, env2, prefix);
                if (fit.asc != Asc.None) s = check_ascription(s, fit.sig, env2, fit.asc, prefix, fit.span);
                return s;
            }
        }
    }

    Tuple!(Item[], Scope) get_sig(SigExpr s, Scope env, Span sp)
    {
        if (!s.named) return tuple(s.items, env);
        SigDef* sd = null;
        if (s.path.length == 1) sd = env.sig(s.path[0]);
        else
        {
            auto ms = resolve_mpath(s.path[0 .. $ - 1], env, sp);
            sd = ms.sigs.find(s.path[$ - 1]);
        }
        if (sd is null) error("M0504", "unbound signature `" ~ join_strings(s.path, ".") ~ "`", s.span);
        return tuple((*sd).expr.items, (*sd).env);
    }

    /// A module matching `sig` with abstract types (functor parameters).
    Scope sig_to_module(SigExpr sig, Scope env, string prefix, Span sp)
    {
        auto gs = get_sig(sig, env, sp);
        auto items = gs[0];
        auto local = Scope.child(gs[1], prefix);
        auto r = new Scope;
        r.prefix = prefix;
        foreach (it; items)
        {
            if (it.kind == IK.Type)
            {
                auto d = new TypeDef;
                d.name = it.name;
                d.display = prefix ~ it.name;
                d.stamp = next_stamp++;
                foreach (p; it.tparams)
                {
                    auto v = generic_var();
                    v.hint = p;
                    d.params ~= v;
                }
                auto m = new TVarMap;
                m.allow_new = false;
                foreach (k; 0 .. it.tparams.length) m.vars[it.tparams[k]] = d.params[k];
                if (it.tkind == 1)
                {
                    d.alias_ = true;
                    d.body = resolve_type(it.type, local, m);
                }
                else if (it.tkind == 2)
                {
                    foreach (c; 0 .. it.ctors.length)
                    {
                        auto cd = it.ctors[c];
                        TypeNode[] args;
                        foreach (a; cd.args) args ~= resolve_type(a, local, m);
                        TypeNode t = t_con(d.display, d.stamp, d.params);
                        for (size_t kk = args.length; kk-- > 0;) t = t_fun(args[kk], t);
                        auto ci = new CtorInfo;
                        ci.name = cd.name;
                        ci.index = cast(int) c;
                        ci.arity = cast(int) args.length;
                        ci.scheme = t;
                        ci.owner = d;
                        d.ctors ~= ci;
                        r.ctors.set(cd.name, ci);
                        local.ctors.set(cd.name, ci);
                    }
                }
                local.types.set(it.name, d);
                r.types.set(it.name, d);
            }
            else if (it.kind == IK.Sig)
                r.vars.set(it.name, VarInfo(sig_scheme(it.type, local), null));
        }
        return r;
    }

    /// Checks that module `s` matches `sig`; seals it for `:>`.
    Scope check_ascription(Scope s, SigExpr sig, Scope env, Asc asc, string prefix, Span sp)
    {
        auto gs = get_sig(sig, env, sp);
        auto items = gs[0];
        auto actual = Scope.child(gs[1], prefix);
        auto opaque = Scope.child(gs[1], prefix);
        auto r = new Scope;
        r.prefix = prefix;
        foreach (it; items)
        {
            if (it.kind == IK.Type)
            {
                auto sd = s.types.find(it.name);
                if (sd is null)
                    error("M0502", "the module does not define the type `" ~ it.name
                        ~ "` required by the signature", sp);
                if ((*sd).params.length != it.tparams.length)
                    error("M0502", "type `" ~ it.name ~ "` has " ~ to!string((*sd).params.length)
                        ~ " parameter(s) but the signature expects " ~ to!string(it.tparams.length), sp);
                if (it.tkind == 1)
                {
                    auto m = new TVarMap;
                    TypeNode[] ps;
                    foreach (p; it.tparams)
                    {
                        auto v = fresh();
                        m.vars[p] = v;
                        ps ~= v;
                    }
                    auto want = resolve_type(it.type, actual, m);
                    auto have = apply_typedef(*sd, ps, sp);
                    auto snap = u.snapshot();
                    if (!u.unify(want, have))
                    {
                        u.rollback(snap);
                        auto p = new TypePrinter;
                        string hs = p.print_bare(have);
                        string ws = p.print_bare(want);
                        error("M0502", "type `" ~ it.name ~ "` is `" ~ hs
                            ~ "` in the module but the signature requires `" ~ ws ~ "`", sp);
                    }
                }
                if (it.tkind == 2)
                {
                    bool ok = (*sd).ctors.length == it.ctors.length;
                    for (size_t c = 0; ok && c < it.ctors.length; ++c) ok = (*sd).ctors[c].name == it.ctors[c].name;
                    if (!ok) error("M0502", "the constructors of type `" ~ it.name ~ "` do not match the signature", sp);
                }
                actual.types.set(it.name, *sd);
                if (asc == Asc.Opaque)
                {
                    if (it.tkind == 0)
                    {
                        auto od = new TypeDef;
                        od.name = it.name;
                        od.display = prefix ~ it.name;
                        od.stamp = next_stamp++;
                        foreach (k; 0 .. it.tparams.length)
                        {
                            auto v = generic_var();
                            v.hint = it.tparams[k];
                            od.params ~= v;
                        }
                        r.types.set(it.name, od);
                        opaque.types.set(it.name, od);
                    }
                    else
                    {
                        r.types.set(it.name, *sd);
                        opaque.types.set(it.name, *sd);
                        foreach (ci; (*sd).ctors) r.ctors.set(ci.name, ci);
                    }
                }
            }
            else if (it.kind == IK.Sig)
            {
                auto vi = s.vars.find(it.name);
                if (vi is null)
                    error("M0502", "the module does not define the value `" ~ it.name
                        ~ "` required by the signature", sp);
                auto want = skolemize(sig_scheme(it.type, actual));
                auto have = instantiate(vi.scheme);
                auto snap = u.snapshot();
                if (!u.unify(want, have))
                {
                    u.rollback(snap);
                    auto p1 = new TypePrinter, p2 = new TypePrinter;
                    string hs = p1.print(instantiate(vi.scheme));
                    string ws = p2.print(skolemize(sig_scheme(it.type, actual)));
                    error("M0502", "value `" ~ it.name ~ "` has type `" ~ hs
                        ~ "` in the module but the signature requires `" ~ ws ~ "`", sp);
                }
                if (asc == Asc.Opaque) r.vars.set(it.name, VarInfo(sig_scheme(it.type, opaque), null));
            }
        }
        return asc == Asc.Opaque ? r : s;
    }

    // ---- top-level transactions ----
    static void commit_scope(Scope from, Scope into)
    {
        foreach (k; from.vars.keys) into.vars.set(k, *from.vars.find(k));
        foreach (k; from.types.keys) into.types.set(k, *from.types.find(k));
        foreach (k; from.ctors.keys) into.ctors.set(k, *from.ctors.find(k));
        foreach (k; from.modules.keys) into.modules.set(k, *from.modules.find(k));
        foreach (k; from.functors.keys) into.functors.set(k, *from.functors.find(k));
        foreach (k; from.sigs.keys) into.sigs.set(k, *from.sigs.find(k));
        foreach (k; from.pending.keys) into.pending.set(k, *from.pending.find(k));
    }
}

// ============================================================
//  Runtime values, objects, frames and the evaluator
// ============================================================

enum VK : ubyte
{
    Unit, Bool, Int, Float, Str, List, Tuple, Record, Ctor, Closure, Prim, CtorFn, Compose,
    Module, Foreign
}

/// Base of every heap-allocated runtime object.
abstract class Obj
{
}

/// A MetaML runtime value (immediate scalars + shared heap object).
struct Value
{
    VK kind = VK.Unit;
    long i;
    double f = 0.0;
    Obj obj;

    static Value unit() { return Value.init; }
    static Value boolean(bool b) { Value v; v.kind = VK.Bool; v.i = b ? 1 : 0; return v; }
    static Value integer(long n) { Value v; v.kind = VK.Int; v.i = n; return v; }
    static Value floating(double d) { Value v; v.kind = VK.Float; v.f = d; return v; }
}

/// A top-level binding cell (static binding across REPL redefinitions).
final class Cell
{
    string name;
    bool defined;
    Value value;
}

final class StrObj : Obj
{
    string s;
    this(string v) { s = v; }
}

final class ConsObj : Obj
{
    Value head;
    ConsObj tail;
    this(Value h, ConsObj t) { head = h; tail = t; }
}

alias Labels = immutable(string)[];

/// Tuples, records and constructor applications.
final class AggObj : Obj
{
    string tag;
    Value[] items;
    Labels labels;
}

final class FunctorObj : Obj
{
    Item item;
    Frame env;
}

/// Runtime environment frame; later bindings shadow earlier ones.
final class Frame
{
    Tuple!(string, Value)[] vars;
    Tuple!(string, ModuleObj)[] mods;
    Tuple!(string, FunctorObj)[] functors;
    Frame parent;

    static Frame child(Frame p)
    {
        auto f = new Frame;
        f.parent = p;
        return f;
    }
    Value* find_var(string n)
    {
        for (Frame f = this; f !is null; f = f.parent)
            for (size_t i = f.vars.length; i-- > 0;)
                if (f.vars[i][0] == n) return &f.vars[i][1];
        return null;
    }
    ModuleObj find_mod(string n)
    {
        for (Frame f = this; f !is null; f = f.parent)
            for (size_t i = f.mods.length; i-- > 0;)
                if (f.mods[i][0] == n) return f.mods[i][1];
        return null;
    }
    FunctorObj find_functor(string n)
    {
        for (Frame f = this; f !is null; f = f.parent)
            for (size_t i = f.functors.length; i-- > 0;)
                if (f.functors[i][0] == n) return f.functors[i][1];
        return null;
    }
}

final class ClosureObj : Obj
{
    Expr lam;
    Frame env;
    Value[] args;
    ClosureObj dup()
    {
        auto c = new ClosureObj;
        c.lam = lam;
        c.env = env;
        c.args = args.dup;
        return c;
    }
}

final class PrimObj : Obj
{
    Primitive prim;
    Value[] args;
}

final class CtorFnObj : Obj
{
    string name;
    int index;
    int arity;
    Value[] args;
}

/// A concatenative composition: apply `first`, then `second` (§5.4, §7.3).
final class ComposeObj : Obj
{
    Value first;
    Value second;
}

final class ModuleObj : Obj
{
    enum Src { Struct, Alias, Apply }
    string name;
    Src src = Src.Struct;
    ModExpr mexpr;
    Frame env;
    bool forced;
    bool forcing;
    Frame frame;
    Tuple!(string, Value)[] fields;
    Tuple!(string, ModuleObj)[] mods;
    Tuple!(string, FunctorObj)[] functors;
    Item[] seals;

    static void upsert(T)(ref Tuple!(string, T)[] v, string k, T x)
    {
        foreach (ref kv; v) if (kv[0] == k) { kv[1] = x; return; }
        v ~= tuple(k, x);
    }
}

/// Base for host-defined opaque values (handles).
class ForeignObj : Obj
{
    string type_name = "Foreign";
    this(string t = "Foreign") { type_name = t; }
    string describe() const { return "<" ~ type_name ~ ">"; }
}

// ---- value constructors & accessors ----

Value make_str(string s)
{
    Value v;
    v.kind = VK.Str;
    v.obj = new StrObj(s);
    return v;
}
string str_of(Value v)
{
    if (v.kind != VK.Str || v.obj is null) return "";
    return (cast(StrObj) v.obj).s;
}
Value make_list(Value[] items)
{
    ConsObj acc;
    for (size_t i = items.length; i-- > 0;) acc = new ConsObj(items[i], acc);
    Value v;
    v.kind = VK.List;
    v.obj = acc;
    return v;
}
Value cons_value(Value h, Value tail)
{
    Value v;
    v.kind = VK.List;
    v.obj = new ConsObj(h, cast(ConsObj) tail.obj);
    return v;
}
ConsObj cons_of(Value v) { return cast(ConsObj) v.obj; }
Value[] list_items(Value v)
{
    Value[] result;
    for (ConsObj c = cons_of(v); c !is null; c = c.tail) result ~= c.head;
    return result;
}
Value make_tuple(Value[] items)
{
    auto a = new AggObj;
    a.items = items;
    Value v;
    v.kind = VK.Tuple;
    v.obj = a;
    return v;
}
AggObj agg_of(Value v) { return cast(AggObj) v.obj; }
Value make_ctor(string name, int index, Value[] args = null)
{
    auto a = new AggObj;
    a.tag = name;
    a.items = args;
    Value v;
    v.kind = VK.Ctor;
    v.i = index;
    v.obj = a;
    return v;
}
/// Builds a record from (label, value) pairs; labels are sorted.
Value make_record(Tuple!(string, Value)[] fields)
{
    fields.sort!((a, b) => a[0] < b[0], SwapStrategy.stable);
    string[] labels;
    auto a = new AggObj;
    foreach (f; fields)
    {
        labels ~= f[0];
        a.items ~= f[1];
    }
    a.labels = cast(Labels) labels;
    Value v;
    v.kind = VK.Record;
    v.obj = a;
    return v;
}
Value* record_field(Value v, string label)
{
    if (v.kind != VK.Record) return null;
    auto a = agg_of(v);
    foreach (i, l; a.labels) if (l == label) return &a.items[i];
    return null;
}
Value make_option(Nullable!Value v)
{
    if (v.isNull) return make_ctor("None", 0);
    return make_ctor("Some", 1, [v.get]);
}
Value make_some(Value v) { return make_ctor("Some", 1, [v]); }
Value make_none() { return make_ctor("None", 0); }
Value make_ok(Value v) { return make_ctor("Ok", 0, [v]); }
Value make_err(Value v) { return make_ctor("Err", 1, [v]); }
bool is_callable(Value v)
{
    return v.kind == VK.Closure || v.kind == VK.Prim || v.kind == VK.CtorFn || v.kind == VK.Compose;
}

/// Canonical value rendering (REPL, `show`, to_string).
string show_value(Value v)
{
    final switch (v.kind)
    {
        case VK.Unit: return "()";
        case VK.Bool: return v.i ? "true" : "false";
        case VK.Int: return to!string(v.i);
        case VK.Float: return format_float(v.f);
        case VK.Str: return quote_string(str_of(v));
        case VK.List:
        {
            string s = "[";
            bool first = true;
            for (ConsObj c = cons_of(v); c !is null; c = c.tail)
            {
                if (!first) s ~= ", ";
                first = false;
                s ~= show_value(c.head);
            }
            return s ~ "]";
        }
        case VK.Tuple:
        {
            string s = "(";
            foreach (i, x; agg_of(v).items) s ~= (i ? ", " : "") ~ show_value(x);
            return s ~ ")";
        }
        case VK.Record:
        {
            auto a = agg_of(v);
            if (!a.items.length) return "{}";
            string s = "{ ";
            foreach (i, x; a.items) s ~= (i ? ", " : "") ~ a.labels[i] ~ " = " ~ show_value(x);
            return s ~ " }";
        }
        case VK.Ctor:
        {
            auto a = agg_of(v);
            string s = a.tag;
            foreach (x; a.items)
            {
                bool paren = (x.kind == VK.Ctor && agg_of(x).items.length)
                    || (x.kind == VK.Int && x.i < 0)
                    || (x.kind == VK.Float && signbitd(x.f));
                s ~= " " ~ (paren ? "(" ~ show_value(x) ~ ")" : show_value(x));
            }
            return s;
        }
        case VK.Closure: case VK.Prim: case VK.CtorFn: case VK.Compose:
            return "<fn>";
        case VK.Module: return "<module " ~ (cast(ModuleObj) v.obj).name ~ ">";
        case VK.Foreign: return (cast(ForeignObj) v.obj).describe();
    }
}

/// Unquoted rendering: strings print raw.
string display_value(Value v) { return v.kind == VK.Str ? str_of(v) : show_value(v); }

/// Structural equality (IEEE for floats; functions are incomparable).
bool values_equal(Value a, Value b, Span sp = Span.init)
{
    if ((a.kind == VK.Int || a.kind == VK.Float) && (b.kind == VK.Int || b.kind == VK.Float) && a.kind != b.kind)
    {
        double x = a.kind == VK.Int ? cast(double) a.i : a.f;
        double y = b.kind == VK.Int ? cast(double) b.i : b.f;
        return x == y;
    }
    if (is_callable(a) || is_callable(b)) fail("M0405", "functions cannot be compared for equality", sp);
    if (a.kind != b.kind) return false;
    switch (a.kind)
    {
        case VK.Unit: return true;
        case VK.Bool: case VK.Int: return a.i == b.i;
        case VK.Float: return a.f == b.f;
        case VK.Str: return str_of(a) == str_of(b);
        case VK.List:
        {
            ConsObj x = cons_of(a), y = cons_of(b);
            for (; x !is null && y !is null; x = x.tail, y = y.tail)
                if (!values_equal(x.head, y.head, sp)) return false;
            return x is null && y is null;
        }
        case VK.Tuple: case VK.Record: case VK.Ctor:
        {
            auto x = agg_of(a), y = agg_of(b);
            if (x.tag != y.tag || x.items.length != y.items.length) return false;
            if (a.kind == VK.Record && x.labels != y.labels) return false;
            foreach (i; 0 .. x.items.length) if (!values_equal(x.items[i], y.items[i], sp)) return false;
            return true;
        }
        case VK.Module: case VK.Foreign:
            return a.obj is b.obj;
        default:
            return false;
    }
}

/// Structural total order (-1, 0, 1); NaN compares equal to everything.
int compare_values(Value a, Value b, Span sp = Span.init)
{
    double num(Value v) { return v.kind == VK.Int ? cast(double) v.i : v.f; }
    if ((a.kind == VK.Int || a.kind == VK.Float) && (b.kind == VK.Int || b.kind == VK.Float))
    {
        if (a.kind == VK.Int && b.kind == VK.Int) return a.i < b.i ? -1 : a.i > b.i ? 1 : 0;
        double x = num(a), y = num(b);
        return x < y ? -1 : x > y ? 1 : 0;
    }
    if (is_callable(a) || is_callable(b)) fail("M0405", "functions cannot be compared", sp);
    if (a.kind != b.kind) return cast(int) a.kind < cast(int) b.kind ? -1 : 1;
    switch (a.kind)
    {
        case VK.Unit: return 0;
        case VK.Bool: return a.i < b.i ? -1 : a.i > b.i ? 1 : 0;
        case VK.Str:
        {
            string x = str_of(a), y = str_of(b);
            return x < y ? -1 : x > y ? 1 : 0;
        }
        case VK.List:
        {
            ConsObj x = cons_of(a), y = cons_of(b);
            for (; x !is null && y !is null; x = x.tail, y = y.tail)
                if (int c = compare_values(x.head, y.head, sp)) return c;
            return x !is null ? 1 : y !is null ? -1 : 0;
        }
        case VK.Tuple: case VK.Record: case VK.Ctor:
        {
            if (a.kind == VK.Ctor && a.i != b.i) return a.i < b.i ? -1 : 1;
            auto x = agg_of(a), y = agg_of(b);
            size_t n = min(x.items.length, y.items.length);
            foreach (i; 0 .. n) if (int c = compare_values(x.items[i], y.items[i], sp)) return c;
            return x.items.length < y.items.length ? -1 : x.items.length > y.items.length ? 1 : 0;
        }
        default:
            fail("M0405", "modules and host handles cannot be ordered", sp);
    }
}

// ---- primitives ----

/// Context handed to host primitives.
final class PrimContext
{
    Session session;
    Interp interp;
    Span span;
    this(Session s, Interp i, Span sp) { session = s; interp = i; span = sp; }
    Value apply(Value f, Value[] args) { return interp.apply(f, args, span); }
    noreturn error(string code, string msg) { fail(code, msg, span); }
    void print(string text) { session.write(text); }
}

alias PrimFn = Value delegate(PrimContext, Value[]);

final class Primitive
{
    string name;
    string type_src;
    int arity = 1;
    PrimFn fn;
    bool core;
    TypeNode scheme;
    bool resolving;
}

// ---- the evaluator ----

final class Interp
{
    int max_depth = 10000;
    int call_depth = 0;
    int eval_depth = 0;
    Value[] stack;
    bool tracing;
    int trace_limit = 32;
    int trace_base = 0;
    void delegate(string) trace_sink;
    bool delegate(Primitive) allowed;
    bool delegate() cancelled;
    Cell delegate(string) global_lookup;
    Session session;
    import core.sync.mutex : Mutex;
    Mutex module_lock; /// serialises lazy module init (recursive)

    void check_cancelled(Span sp = Span.init)
    {
        if (cancelled !is null && cancelled()) fail("M0410", "task cancelled", sp);
    }

    private Primitive[string] op_prims_;

    noreturn fail(string code, string msg, Span sp) { .fail(code, msg, sp); }

    Value prim_value(Primitive p)
    {
        auto o = new PrimObj;
        o.prim = p;
        Value v;
        v.kind = VK.Prim;
        v.obj = o;
        return v;
    }

    Primitive op_prim(string op)
    {
        if (auto p = op in op_prims_) return *p;
        auto p = new Primitive;
        p.name = "(" ~ op ~ ")";
        p.arity = 2;
        p.core = true;
        string o = op;
        p.fn = (PrimContext ctx, Value[] a) => binop(o, a[0], a[1], ctx.span);
        op_prims_[op] = p;
        return p;
    }

    // ---- arithmetic ----
    Value binop(string op, Value a, Value b, Span sp)
    {
        if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%")
        {
            if (a.kind == VK.Int && b.kind == VK.Int)
            {
                long x = a.i, y = b.i;
                if (op == "+") return Value.integer(wrap_add(x, y));
                if (op == "-") return Value.integer(wrap_sub(x, y));
                if (op == "*") return Value.integer(wrap_mul(x, y));
                if (y == 0) fail("M0402", "division by zero", sp);
                if (y == -1) return Value.integer(op == "/" ? wrap_neg(x) : 0);
                return Value.integer(op == "/" ? x / y : x % y);
            }
            if ((a.kind != VK.Int && a.kind != VK.Float) || (b.kind != VK.Int && b.kind != VK.Float))
                fail("M0404", "arithmetic on non-numeric values", sp);
            double x = a.kind == VK.Int ? cast(double) a.i : a.f;
            double y = b.kind == VK.Int ? cast(double) b.i : b.f;
            if (op == "+") return Value.floating(x + y);
            if (op == "-") return Value.floating(x - y);
            if (op == "*") return Value.floating(x * y);
            if (op == "/") return Value.floating(x / y);
            return Value.floating(c_fmod(x, y));
        }
        if (op == "==" || op == "=") return Value.boolean(values_equal(a, b, sp));
        if (op == "!=" || op == "<>") return Value.boolean(!values_equal(a, b, sp));
        if (op == "<" || op == ">" || op == "<=" || op == ">=")
        {
            if (a.kind == VK.Float || b.kind == VK.Float)
            {
                double x = a.kind == VK.Int ? cast(double) a.i : a.f;
                double y = b.kind == VK.Int ? cast(double) b.i : b.f;
                bool r = op == "<" ? x < y : op == ">" ? x > y : op == "<=" ? x <= y : x >= y;
                return Value.boolean(r);
            }
            int c = compare_values(a, b, sp);
            bool r = op == "<" ? c < 0 : op == ">" ? c > 0 : op == "<=" ? c <= 0 : c >= 0;
            return Value.boolean(r);
        }
        if (op == "&&") return Value.boolean(a.i && b.i);
        if (op == "||") return Value.boolean(a.i || b.i);
        if (op == "^") return make_str(str_of(a) ~ str_of(b));
        if (op == "@")
        {
            auto xs = list_items(a);
            Value acc = b;
            for (size_t i = xs.length; i-- > 0;) acc = cons_value(xs[i], acc);
            return acc;
        }
        if (op == "::") return cons_value(a, b);
        if (op == ">>") return compose(a, b);
        if (op == "<<" || op == "\xE2\x88\x98") return compose(b, a);
        if (op == "|>") return apply(b, [a], sp);
        fail("M0404", "unknown operator `" ~ op ~ "`", sp);
    }

    static Value compose(Value first, Value second)
    {
        auto c = new ComposeObj;
        c.first = first;
        c.second = second;
        Value v;
        v.kind = VK.Compose;
        v.obj = c;
        return v;
    }

    // ---- patterns ----
    bool match(Pat p, Value v, Frame f)
    {
        final switch (p.kind)
        {
            case PK.Wild: return true;
            case PK.Var:
                f.vars ~= tuple(p.text, v);
                return true;
            case PK.Int: return v.kind == VK.Int && v.i == p.ival;
            case PK.Float: return v.kind == VK.Float && v.f == p.fval;
            case PK.Str: return v.kind == VK.Str && str_of(v) == p.text;
            case PK.Bool: return v.kind == VK.Bool && v.i == p.ival;
            case PK.Unit: return v.kind == VK.Unit;
            case PK.Tuple:
            {
                if (v.kind != VK.Tuple) return false;
                auto a = agg_of(v);
                if (a.items.length != p.kids.length) return false;
                foreach (i; 0 .. p.kids.length) if (!match(p.kids[i], a.items[i], f)) return false;
                return true;
            }
            case PK.List:
            {
                if (v.kind != VK.List) return false;
                ConsObj c = cons_of(v);
                foreach (k; p.kids)
                {
                    if (c is null || !match(k, c.head, f)) return false;
                    c = c.tail;
                }
                return c is null;
            }
            case PK.Cons:
            {
                if (v.kind != VK.List || v.obj is null) return false;
                ConsObj c = cons_of(v);
                Value tail;
                tail.kind = VK.List;
                tail.obj = c.tail;
                return match(p.kids[0], c.head, f) && match(p.kids[1], tail, f);
            }
            case PK.Ctor:
            {
                if (v.kind != VK.Ctor) return false;
                auto a = agg_of(v);
                if (a.tag != p.text || a.items.length != p.kids.length) return false;
                foreach (i; 0 .. p.kids.length) if (!match(p.kids[i], a.items[i], f)) return false;
                return true;
            }
            case PK.Record:
                foreach (i; 0 .. p.kids.length)
                {
                    auto fv = record_field(v, p.labels[i]);
                    if (fv is null || !match(p.kids[i], *fv, f)) return false;
                }
                return true;
            case PK.Annot:
                return match(p.kids[0], v, f);
        }
    }

    // ---- modules ----
    ModuleObj make_module(Item it, Frame env)
    {
        auto m = new ModuleObj;
        m.name = it.name;
        m.mexpr = it.mexpr;
        m.env = env;
        m.src = it.mexpr.kind == MK.Struct ? ModuleObj.Src.Struct
            : it.mexpr.kind == MK.Path ? ModuleObj.Src.Alias : ModuleObj.Src.Apply;
        if (it.sealed) m.seals ~= it;
        return m;
    }

    ModuleObj module_from(ModExpr me, Frame env, string name)
    {
        if (me.kind == MK.Path) return resolve_module(me.path, env, me.span);
        auto m = new ModuleObj;
        m.name = name;
        m.mexpr = me;
        m.env = env;
        m.src = me.kind == MK.Struct ? ModuleObj.Src.Struct : ModuleObj.Src.Apply;
        return m;
    }

    ModuleObj resolve_module(const(string)[] path, Frame env, Span sp)
    {
        if (module_lock !is null) module_lock.lock();
        scope (exit) if (module_lock !is null) module_lock.unlock();
        auto m = env.find_mod(path[0]);
        if (m is null) fail("M0501", "unbound module `" ~ path[0] ~ "` at run time", sp);
        foreach (i; 1 .. path.length)
        {
            force(m, sp);
            ModuleObj next;
            for (size_t k = m.mods.length; k-- > 0;)
                if (m.mods[k][0] == path[i]) { next = m.mods[k][1]; break; }
            if (next is null) fail("M0501", "module `" ~ m.name ~ "` has no submodule `" ~ path[i] ~ "`", sp);
            m = next;
        }
        return m;
    }

    FunctorObj resolve_functor(string[] path, Frame env, Span sp)
    {
        if (module_lock !is null) module_lock.lock();
        scope (exit) if (module_lock !is null) module_lock.unlock();
        if (path.length == 1)
        {
            auto f = env.find_functor(path[0]);
            if (f is null) fail("M0503", "unbound functor `" ~ path[0] ~ "` at run time", sp);
            return f;
        }
        auto m = resolve_module(path[0 .. $ - 1], env, sp);
        force(m, sp);
        for (size_t k = m.functors.length; k-- > 0;)
            if (m.functors[k][0] == path[$ - 1]) return m.functors[k][1];
        fail("M0503", "unbound functor `" ~ join_strings(path, ".") ~ "` at run time", sp);
    }

    void apply_seal(ModuleObj m, Item seal)
    {
        Tuple!(string, Value)[] fields;
        foreach (n; seal.visible_values)
            for (size_t k = m.fields.length; k-- > 0;)
                if (m.fields[k][0] == n) { fields ~= m.fields[k]; break; }
        m.fields = fields;
        Tuple!(string, ModuleObj)[] mods;
        foreach (n; seal.visible_modules)
            for (size_t k = m.mods.length; k-- > 0;)
                if (m.mods[k][0] == n) { mods ~= m.mods[k]; break; }
        m.mods = mods;
        m.functors = null;
    }

    /// Evaluates a module body once, on first access (§7.4).
    void force(ModuleObj m, Span sp)
    {
        if (module_lock !is null) module_lock.lock();
        scope (exit) if (module_lock !is null) module_lock.unlock();
        if (m.forced || m.forcing) return;
        m.forcing = true;
        scope (failure) m.forcing = false;
        final switch (m.src)
        {
            case ModuleObj.Src.Struct:
                m.frame = Frame.child(m.env);
                exec_struct(m.mexpr.items, m.frame, m);
                break;
            case ModuleObj.Src.Alias:
            {
                auto t = resolve_module(m.mexpr.path, m.env, sp);
                force(t, sp);
                m.fields = t.fields.dup;
                m.mods = t.mods.dup;
                m.functors = t.functors.dup;
                break;
            }
            case ModuleObj.Src.Apply:
            {
                auto f = resolve_functor(m.mexpr.path, m.env, sp);
                auto a = module_from(m.mexpr.arg, m.env, m.name ~ ".arg");
                force(a, sp);
                m.frame = Frame.child(f.env);
                m.frame.mods ~= tuple(f.item.pname, a);
                exec_struct(f.item.mexpr.items, m.frame, m);
                if (f.item.sealed) apply_seal(m, f.item);
                break;
            }
        }
        foreach (s; m.seals) apply_seal(m, s);
        m.forcing = false;
        m.forced = true;
    }

    Value module_field(ModuleObj m, string name, Span sp)
    {
        force(m, sp);
        if (m.forcing && m.frame !is null)
            if (auto v = m.frame.find_var(name)) return *v;
        for (size_t k = m.fields.length; k-- > 0;)
            if (m.fields[k][0] == name) return m.fields[k][1];
        if (m.forcing)
            fail("M0507", "module `" ~ m.name ~ "` is still being initialised; `" ~ name
                ~ "` is not available yet", sp);
        fail("M0506", "module `" ~ m.name ~ "` has no member `" ~ name ~ "`", sp);
    }

    void exec_struct(Item[] items, Frame frame, ModuleObj mod)
    {
        foreach (it; items)
            switch (it.kind)
            {
                case IK.Def:
                {
                    Value v = eval(it.value, frame);
                    frame.vars ~= tuple(it.name, v);
                    ModuleObj.upsert(mod.fields, it.name, v);
                    break;
                }
                case IK.Module:
                {
                    auto m = make_module(it, frame);
                    frame.mods ~= tuple(it.name, m);
                    ModuleObj.upsert(mod.mods, it.name, m);
                    break;
                }
                case IK.Functor:
                {
                    auto f = new FunctorObj;
                    f.item = it;
                    f.env = frame;
                    frame.functors ~= tuple(it.name, f);
                    ModuleObj.upsert(mod.functors, it.name, f);
                    break;
                }
                case IK.Open:
                case IK.Include:
                {
                    auto src = resolve_module(it.path, frame, it.span);
                    force(src, it.span);
                    foreach (kv; src.fields)
                    {
                        frame.vars ~= kv;
                        if (it.kind == IK.Include) ModuleObj.upsert(mod.fields, kv[0], kv[1]);
                    }
                    foreach (kv; src.mods)
                    {
                        frame.mods ~= kv;
                        if (it.kind == IK.Include) ModuleObj.upsert(mod.mods, kv[0], kv[1]);
                    }
                    foreach (kv; src.functors)
                    {
                        frame.functors ~= kv;
                        if (it.kind == IK.Include) ModuleObj.upsert(mod.functors, kv[0], kv[1]);
                    }
                    break;
                }
                case IK.Expr:
                    eval(it.body, frame);
                    break;
                default:
                    break;
            }
    }

    // ---- application ----
    void trace_call(ClosureObj c, Value[] args, int depth)
    {
        if (!tracing || trace_sink is null) return;
        int d = depth - trace_base;
        if (d < 0) d = 0;
        if (trace_limit > 0 && d >= trace_limit) return;
        string line = replicate(" ", cast(size_t) d * 2);
        line ~= c.lam.name.length ? c.lam.name : "<fn>";
        foreach (a; args)
        {
            string s = show_value(a);
            bool paren = s.indexOf(' ') >= 0 && s[0] != '[' && s[0] != '(' && s[0] != '{' && s[0] != '"';
            line ~= " " ~ (paren ? "(" ~ s ~ ")" : s);
        }
        trace_sink(line);
    }

    void enter_call(Span sp)
    {
        if (++call_depth > max_depth)
        {
            call_depth = 0;
            fail("M0406", "stack overflow: recursion deeper than " ~ to!string(max_depth) ~ " calls", sp);
        }
    }

    /// Binds a saturated closure call into a fresh frame.
    Frame bind_call(ClosureObj c, Value[] args, Span sp)
    {
        auto f = Frame.child(c.env);
        auto pats = c.lam.pats;
        foreach (i; 0 .. pats.length)
            if (!match(pats[i], args[i], f))
                fail("M0401", "the argument `" ~ show_value(args[i]) ~ "` does not match the parameter pattern", sp);
        for (int k = 1; k <= c.lam.tacit_count; ++k) f.vars ~= tuple("$" ~ to!string(k), args[k - 1]);
        return f;
    }

    Value apply(Value f, Value[] args, Span sp = Span.init)
    {
        while (true)
        {
            check_cancelled(sp);
            if (!args.length) return f;
            switch (f.kind)
            {
                case VK.Closure:
                {
                    auto c = cast(ClosureObj) f.obj;
                    size_t n = c.lam.pats.length;
                    size_t have = c.args.length;
                    if (have + args.length < n)
                    {
                        auto nc = c.dup;
                        nc.args ~= args;
                        Value v;
                        v.kind = VK.Closure;
                        v.obj = nc;
                        return v;
                    }
                    Value[] full = c.args.dup;
                    size_t take = n - have;
                    full ~= args[0 .. take];
                    Value[] rest = args[take .. $];
                    trace_call(c, full, call_depth);
                    auto frame = bind_call(c, full, sp);
                    enter_call(sp);
                    Value r = eval(c.lam.kids[0], frame, true);
                    f = r;
                    args = rest;
                    continue;
                }
                case VK.Prim:
                {
                    auto po = cast(PrimObj) f.obj;
                    size_t n = cast(size_t) po.prim.arity;
                    if (po.args.length + args.length < n)
                    {
                        auto np = new PrimObj;
                        np.prim = po.prim;
                        np.args = po.args ~ args;
                        Value v;
                        v.kind = VK.Prim;
                        v.obj = np;
                        return v;
                    }
                    Value[] full = po.args.dup;
                    size_t take = n - po.args.length;
                    full ~= args[0 .. take];
                    Value[] rest = args[take .. $];
                    if (allowed !is null && !allowed(po.prim))
                        fail("M0403", "unknown primitive `" ~ po.prim.name ~ "` (not in the primitive allowlist)", sp);
                    auto ctx = new PrimContext(session, this, sp);
                    try f = po.prim.fn(ctx, full);
                    catch (MetaError e) { throw e; }
                    catch (Exception e)
                        fail("M0409", "host primitive `" ~ po.prim.name ~ "` failed: " ~ e.msg, sp);
                    args = rest;
                    continue;
                }
                case VK.CtorFn:
                {
                    auto co = cast(CtorFnObj) f.obj;
                    auto nc = new CtorFnObj;
                    nc.name = co.name;
                    nc.index = co.index;
                    nc.arity = co.arity;
                    nc.args = co.args.dup;
                    size_t i = 0;
                    while (i < args.length && cast(int) nc.args.length < nc.arity) nc.args ~= args[i++];
                    Value[] rest = args[i .. $];
                    if (cast(int) nc.args.length < nc.arity)
                    {
                        Value v;
                        v.kind = VK.CtorFn;
                        v.obj = nc;
                        return v;
                    }
                    f = make_ctor(nc.name, nc.index, nc.args);
                    args = rest;
                    continue;
                }
                case VK.Compose:
                {
                    auto co = cast(ComposeObj) f.obj;
                    Value first = co.first, second = co.second;
                    stack ~= args[0];
                    Value top = stack[$ - 1];
                    stack = stack[0 .. $ - 1];
                    Value mid = apply(first, [top], sp);
                    stack ~= mid;
                    top = stack[$ - 1];
                    stack = stack[0 .. $ - 1];
                    Value r = apply(second, [top], sp);
                    args = args[1 .. $];
                    f = r;
                    continue;
                }
                default:
                    fail("M0404", "`" ~ show_value(f) ~ "` is not a function and cannot be applied", sp);
            }
        }
    }

    // ---- evaluation (with tail calls) ----
    Value eval(Expr e, Frame env, bool counted = false)
    {
        ++eval_depth;
        scope (exit)
        {
            --eval_depth;
            if (counted && call_depth > 0) --call_depth;
        }
        if (eval_depth > max_depth * 8 + 1000) fail("M0406", "stack overflow: expression nesting too deep", e.span);
        while (true)
        {
            check_cancelled(e.span);
            final switch (e.kind)
            {
                case EK.Int: return Value.integer(e.ival);
                case EK.Float: return Value.floating(e.fval);
                case EK.Str: return make_str(e.text);
                case EK.Bool: return Value.boolean(e.ival != 0);
                case EK.Unit: return Value.unit();
                case EK.Var:
                case EK.Tacit:
                {
                    if (e.cell !is null)
                    {
                        if (!e.cell.defined) fail("M0408", "`" ~ e.cell.name ~ "` is used before its definition", e.span);
                        return e.cell.value;
                    }
                    string n = e.text;
                    if (auto v = env.find_var(n)) return *v;
                    if (global_lookup !is null)
                        if (auto c = global_lookup(n))
                            if (c.defined) return c.value;
                    fail("M0204", "unbound variable `" ~ n ~ "` at run time", e.span);
                }
                case EK.Path:
                {
                    if (e.prim !is null) return prim_value(e.prim);
                    auto m = resolve_module(e.path[0 .. $ - 1], env, e.span);
                    return module_field(m, e.path[$ - 1], e.span);
                }
                case EK.Ctor:
                {
                    if (e.ctor_arity == 0) return make_ctor(e.text, e.ctor_index);
                    auto c = new CtorFnObj;
                    c.name = e.text;
                    c.index = e.ctor_index;
                    c.arity = e.ctor_arity;
                    Value v;
                    v.kind = VK.CtorFn;
                    v.obj = c;
                    return v;
                }
                case EK.App:
                {
                    if (e.resolution != Resolution.Apply)
                    {
                        Value a = eval(e.kids[0], env);
                        Value b = eval(e.kids[1], env);
                        return e.resolution == Resolution.ComposeFwd ? compose(a, b) : compose(b, a);
                    }
                    Expr[] spine;
                    Expr h = e;
                    while (h.kind == EK.App && h.resolution == Resolution.Apply)
                    {
                        spine ~= h;
                        h = h.kids[0];
                    }
                    Value fv = eval(spine[$ - 1].kids[0], env);
                    Value[] args;
                    for (size_t i = spine.length; i-- > 0;) args ~= eval(spine[i].kids[1], env);
                    if (tail_call(fv, args, e, env, counted)) continue;
                    return apply(fv, args, e.span);
                }
                case EK.Pipe:
                {
                    Value a = eval(e.kids[0], env);
                    Value b = eval(e.kids[1], env);
                    if (e.resolution != Resolution.Apply) return compose(a, b);
                    Value[] args = [a];
                    if (tail_call(b, args, e, env, counted)) continue;
                    return apply(b, args, e.span);
                }
                case EK.BinOp:
                {
                    string op = e.text;
                    if (op == "&&" || op == "||")
                    {
                        Value a = eval(e.kids[0], env);
                        if ((op == "&&") != (a.i != 0)) return a;
                        e = e.kids[1];
                        continue;
                    }
                    Value a = eval(e.kids[0], env);
                    Value b = eval(e.kids[1], env);
                    if (is_builtin_op(op)) return binop(op, a, b, e.span);
                    Value fn;
                    if (e.cell !is null && e.cell.defined) fn = e.cell.value;
                    else if (auto v = env.find_var(op)) fn = *v;
                    else if (global_lookup !is null && global_lookup(op) !is null) fn = global_lookup(op).value;
                    else fail("M0204", "unbound operator `" ~ op ~ "` at run time", e.span);
                    return apply(fn, [a, b], e.span);
                }
                case EK.Neg:
                {
                    Value a = eval(e.kids[0], env);
                    if (a.kind == VK.Int) return Value.integer(wrap_neg(a.i));
                    return Value.floating(-a.f);
                }
                case EK.Lambda:
                {
                    auto c = new ClosureObj;
                    c.lam = e;
                    c.env = env;
                    Value v;
                    v.kind = VK.Closure;
                    v.obj = c;
                    return v;
                }
                case EK.If:
                {
                    Value c = eval(e.kids[0], env);
                    e = c.i ? e.kids[1] : e.kids[2];
                    continue;
                }
                case EK.Match:
                {
                    Value v = eval(e.kids[0], env);
                    bool found = false;
                    foreach (i; 0 .. e.pats.length)
                    {
                        auto f = Frame.child(env);
                        if (!match(e.pats[i], v, f)) continue;
                        if (e.guards[i] !is null && !eval(e.guards[i], f).i) continue;
                        env = f;
                        e = e.kids[i + 1];
                        found = true;
                        break;
                    }
                    if (!found) fail("M0401", "no pattern matched the value `" ~ show_value(v) ~ "`", e.span);
                    continue;
                }
                case EK.Let:
                {
                    Value v = eval(e.kids[0], env);
                    auto f = Frame.child(env);
                    if (!match(e.pats[0], v, f))
                        fail("M0401", "the value `" ~ show_value(v) ~ "` does not match the `let` pattern", e.pats[0].span);
                    env = f;
                    e = e.kids[1];
                    continue;
                }
                case EK.LetRec:
                {
                    auto f = Frame.child(env);
                    foreach (i; 0 .. e.pats.length)
                    {
                        Value v = eval(e.kids[i], f);
                        f.vars ~= tuple(e.pats[i].text, v);
                    }
                    env = f;
                    e = e.kids[$ - 1];
                    continue;
                }
                case EK.List:
                {
                    Value[] items;
                    foreach (k; e.kids) items ~= eval(k, env);
                    return make_list(items);
                }
                case EK.Comp:
                {
                    Value[] result;
                    comprehension(e, 0, env, result);
                    return make_list(result);
                }
                case EK.Range:
                {
                    Value a = eval(e.kids[0], env), b = eval(e.kids[1], env);
                    Value[] items;
                    if (b.i >= a.i && cast(ulong) b.i - cast(ulong) a.i > 50_000_000UL)
                        fail("M0404", "range is too large", e.span);
                    for (long k = a.i; k <= b.i; ++k)
                    {
                        items ~= Value.integer(k);
                        if (k == long.max) break;
                    }
                    return make_list(items);
                }
                case EK.Tuple:
                {
                    Value[] items;
                    foreach (k; e.kids) items ~= eval(k, env);
                    return make_tuple(items);
                }
                case EK.Record:
                {
                    Tuple!(string, Value)[] fields;
                    foreach (i; 0 .. e.kids.length) fields ~= tuple(e.labels[i], eval(e.kids[i], env));
                    return make_record(fields);
                }
                case EK.Update:
                {
                    Value base = eval(e.kids[0], env);
                    if (base.kind != VK.Record) fail("M0404", "record update on a non-record value", e.span);
                    auto a = agg_of(base);
                    auto n = new AggObj;
                    n.tag = a.tag;
                    n.items = a.items.dup;
                    n.labels = a.labels;
                    foreach (i; 0 .. e.labels.length)
                    {
                        Value nv = eval(e.kids[i + 1], env);
                        foreach (k; 0 .. n.labels.length) if (n.labels[k] == e.labels[i]) n.items[k] = nv;
                    }
                    Value v;
                    v.kind = VK.Record;
                    v.obj = n;
                    return v;
                }
                case EK.Project:
                {
                    Value r = eval(e.kids[0], env);
                    auto fv = record_field(r, e.text);
                    if (fv is null) fail("M0404", "the value `" ~ show_value(r) ~ "` has no field `" ~ e.text ~ "`", e.span);
                    return *fv;
                }
                case EK.OpRef:
                {
                    if (is_builtin_op(e.text)) return prim_value(op_prim(e.text));
                    if (e.cell !is null && e.cell.defined) return e.cell.value;
                    if (auto v = env.find_var(e.text)) return *v;
                    fail("M0204", "unbound operator `" ~ e.text ~ "` at run time", e.span);
                }
                case EK.Annot:
                    e = e.kids[0];
                    continue;
            }
        }
    }

private:
    /// If `f args` saturates a closure exactly, rebinds e/env for a tail call.
    bool tail_call(Value f, Value[] args, ref Expr e, ref Frame env, ref bool counted)
    {
        if (f.kind != VK.Closure) return false;
        auto c = cast(ClosureObj) f.obj;
        if (c.args.length + args.length != c.lam.pats.length) return false;
        Value[] full = c.args ~ args;
        if (!counted)
        {
            trace_call(c, full, call_depth);
            auto frame = bind_call(c, full, e.span);
            enter_call(e.span);
            counted = true;
            env = frame;
        }
        else
        {
            trace_call(c, full, call_depth - 1);
            env = bind_call(c, full, e.span);
        }
        e = c.lam.kids[0];
        return true;
    }

    void comprehension(Expr e, size_t idx, Frame env, ref Value[] result)
    {
        if (idx == e.clauses.length)
        {
            result ~= eval(e.kids[0], env);
            return;
        }
        auto c = e.clauses[idx];
        if (c.pat is null)
        {
            if (eval(c.expr, env).i) comprehension(e, idx + 1, env, result);
            return;
        }
        Value src = eval(c.expr, env);
        for (ConsObj n = cons_of(src); n !is null; n = n.tail)
        {
            auto f = Frame.child(env);
            if (match(c.pat, n.head, f)) comprehension(e, idx + 1, f, result);
        }
    }
}

// ============================================================
//  Typed host-primitive registration (§10.3)
// ============================================================

private string freshTypeVar(ref int next)
{
    string s = "'";
    s ~= cast(char)('a' + next % 26);
    if (next >= 26) s ~= to!string(next / 26);
    ++next;
    return s;
}

private enum isNullableT(T) = is(T : Nullable!U, U);
private enum isTupleT(T) = is(T : Tuple!U, U...);
private enum isResultT(T) = is(T : Result!(U, string), U);

/// MetaML type text for a host type.
string host_type(T)(ref int tv)
{
    static if (is(T == Value)) return freshTypeVar(tv);
    else static if (is(T == bool)) return "Bool";
    else static if (is(T == string)) return "String";
    else static if (__traits(isIntegral, T)) return "Int";
    else static if (__traits(isFloating, T)) return "Float";
    else static if (is(T : Nullable!U, U)) return "Option (" ~ host_type!U(tv) ~ ")";
    else static if (is(T : Result!(U, string), U)) return "Result (" ~ host_type!U(tv) ~ ") String";
    else static if (is(T : Tuple!(A, B), A, B))
    {
        auto a = host_type!A(tv);
        return "(" ~ a ~ ", " ~ host_type!B(tv) ~ ")";
    }
    else static if (is(T : U[], U)) return "[" ~ host_type!U(tv) ~ "]";
    else static assert(0, "unsupported host type " ~ T.stringof);
}

T from_value(T)(Value v)
{
    static if (is(T == Value)) return v;
    else static if (is(T == bool)) return v.i != 0;
    else static if (is(T == string)) return str_of(v);
    else static if (__traits(isIntegral, T)) return cast(T)(v.kind == VK.Float ? float_to_int(v.f) : v.i);
    else static if (__traits(isFloating, T)) return cast(T)(v.kind == VK.Int ? cast(double) v.i : v.f);
    else static if (is(T : Nullable!U, U))
    {
        if (v.kind == VK.Ctor && agg_of(v).tag == "Some") return T(from_value!U(agg_of(v).items[0]));
        return T.init;
    }
    else static if (is(T : Tuple!(A, B), A, B))
    {
        auto t = agg_of(v);
        return T(from_value!A(t.items[0]), from_value!B(t.items[1]));
    }
    else static if (is(T : U[], U))
    {
        T result;
        foreach (x; list_items(v)) result ~= from_value!U(x);
        return result;
    }
    else static assert(0, "unsupported host type " ~ T.stringof);
}

Value to_value(T)(T x)
{
    static if (is(T == Value)) return x;
    else static if (is(T == bool)) return Value.boolean(x);
    else static if (is(T == string)) return make_str(x);
    else static if (__traits(isIntegral, T)) return Value.integer(cast(long) x);
    else static if (__traits(isFloating, T)) return Value.floating(cast(double) x);
    else static if (is(T : Nullable!U, U)) return x.isNull ? make_none() : make_some(to_value!U(x.get));
    else static if (is(T : Result!(U, string), U))
        return x.is_ok() ? make_ok(to_value!U(x.unwrap())) : make_err(make_str(x.unwrap_err()));
    else static if (is(T : Tuple!(A, B), A, B)) return make_tuple([to_value!A(x[0]), to_value!B(x[1])]);
    else static if (is(T : U[], U))
    {
        Value[] items;
        foreach (e; x) items ~= to_value!U(e);
        return make_list(items);
    }
    else static assert(0, "unsupported host type " ~ T.stringof);
}

// ============================================================
//  Session — the embedding API (§10)
// ============================================================

alias EvalResult = Result!(Value, Diagnostic[]);

enum RunMode
{
    Repl,  /// print summaries, push results on the stack
    Quiet, /// files and imports: side effects only
    Eval   /// embedding: return the last value
}

struct RunReport
{
    string text;
    Diagnostic[] errors;
    Diagnostic[] warnings;
    Nullable!Value value;
    TypeNode type;
    bool ok() const { return errors.length == 0; }
}

private string metamlSourceDir() { import std.path : dirName; return dirName(__FILE_FULL_PATH__); }

final class Session
{
    private Checker checker_;
    private Interp interp_;
    private Frame root_;
    private Primitive[string] prims_;
    private Cell[string] globals_;
    private Program[] programs_;
    private string[string] sources_;
    private bool[string] imported_;
    private string[] search_path_;
    private string[] allow_;
    private bool allow_all_;
    private string stdlib_dir_;
    private bool stdlib_loaded_;
    private string last_loaded_;
    private bool quit_requested_;
    private string[] stack_types_;
    private void delegate(string) output_;
    import core.sync.mutex : Mutex;
    private Mutex out_mutex_;
    private Mutex module_lock_;
    private Mutex resource_mutex_;
    private bool closing_;
    private struct Resource { void delegate() stop; void delegate() wait; }
    private Resource[] resources_;

    this()
    {
        checker_ = new Checker;
        interp_ = new Interp;
        checker_.find_prim = (string n) => find_primitive(n);
        checker_.prim_type = (Primitive p, Span sp) => primitive_type(p, sp);
        interp_.session = this;
        interp_.allowed = (Primitive p) => primitive_allowed(p);
        interp_.global_lookup = (string n) { auto c = n in globals_; return c is null ? null : *c; };
        root_ = new Frame;
        import core.sync.mutex : Mutex;
        module_lock_ = new Mutex;
        out_mutex_ = new Mutex;
        resource_mutex_ = new Mutex;
        interp_.module_lock = module_lock_;
        output_ = (string s) { import std.stdio : stdout; stdout.write(s); };
        register_core();
        run_source(builtin_source(), "<builtin>", RunMode.Quiet);
    }

    /// Deterministic two-phase shutdown. Embeddings must close sessions with
    /// live resources (rather than depending on GC finalization order).
    void on_close(void delegate() stop, void delegate() wait = null)
    {
        resource_mutex_.lock();
        scope (exit) resource_mutex_.unlock();
        if (closing_) fail("M0404", "the session is closing");
        resources_ ~= Resource(stop, wait);
    }
    void close()
    {
        resource_mutex_.lock();
        if (closing_) { resource_mutex_.unlock(); return; }
        closing_ = true;
        auto resources = resources_;
        resources_ = null;
        resource_mutex_.unlock();
        foreach (r; resources) try { if (r.stop !is null) r.stop(); } catch (Exception) {}
        foreach (r; resources) try { if (r.wait !is null) r.wait(); } catch (Exception) {}
    }

    /// MetaML source evaluated by every fresh session (identical in MetaML.hpp).
    static string builtin_source()
    {
        return "type Option 'a := | None | Some 'a\n"
            ~ "type Result 'a 'e := | Ok 'a | Err 'e\n";
    }

    // ---------------- standard library ----------------
    string[] stdlib_candidates()
    {
        import std.process : environment;
        string[] c;
        auto s = environment.get("METAML_STDLIB");
        if (s.length) c ~= s;
        auto p = environment.get("METAML_PREFIX");
        if (p.length) c ~= p ~ "/stdlib/" ~ dialect;
        auto exe = executable_dir();
        if (exe.length)
        {
            c ~= exe ~ "/../share/metaml/stdlib/" ~ dialect;
            c ~= exe ~ "/../metalib";
        }
        c ~= metamlSourceDir() ~ "/metalib";
        c ~= "metalib";
        return c;
    }

    /// Loads metalib (the prelude and everything it imports).
    Result!(string, Diagnostic[]) load_stdlib(string dir = "")
    {
        import std.file : exists, isFile;
        alias R = Result!(string, Diagnostic[]);
        string found;
        string[] cands = !dir.length ? stdlib_candidates() : [dir];
        foreach (c; cands)
        {
            string f = c ~ "/prelude/mod.ml";
            try { if (exists(f) && isFile(f)) { found = c; break; } }
            catch (Exception) {}
        }
        if (!found.length)
            return R.from_err([make_diag("M0505",
                "cannot locate metalib (set METAML_STDLIB to the metalib directory)", Span.init)]);
        auto previous_dir = stdlib_dir_;
        bool loaded = false;
        bool q = checker_.quiet;
        scope (exit)
        {
            checker_.quiet = q;
            if (!loaded) stdlib_dir_ = previous_dir;
        }
        stdlib_dir_ = found;
        checker_.quiet = true;
        auto r = import_module("prelude", "");
        if (!r.ok()) return R.from_err(r.errors);
        stdlib_loaded_ = true;
        loaded = true;
        return R.from_ok(found);
    }
    alias loadStdlib = load_stdlib;
    string stdlib_dir() const { return stdlib_dir_; }
    bool stdlib_loaded() const { return stdlib_loaded_; }

    // ---------------- evaluation ----------------
    /// Evaluates source; returns the value of the last item.
    EvalResult eval(string source, string file = "<eval>")
    {
        auto r = run_source(source, file, RunMode.Eval);
        if (!r.ok()) return EvalResult.from_err(r.errors);
        return EvalResult.from_ok(r.value.isNull ? Value.unit() : r.value.get);
    }

    /// Infers the type of an expression without evaluating it.
    Result!(string, Diagnostic[]) type_of(string expr)
    {
        alias R = Result!(string, Diagnostic[]);
        sources_["<type>"] = expr;
        auto pe = parse_expression(expr, "<type>");
        if (pe.is_err()) return R.from_err(pe.unwrap_err());
        try
        {
            auto t = check_expr(pe.unwrap());
            checker_.u.commit();
            return R.from_ok(show_type(t));
        }
        catch (MetaError e)
        {
            checker_.u.commit();
            return R.from_err([e.diag]);
        }
    }
    alias typeOf = type_of;

    Result!(bool, Diagnostic[]) load_file(string path)
    {
        alias R = Result!(bool, Diagnostic[]);
        auto r = run_file(path, RunMode.Quiet);
        if (!r.ok()) return R.from_err(r.errors);
        return R.from_ok(true);
    }
    alias loadFile = load_file;

    RunReport run_file(string path, RunMode mode)
    {
        RunReport rep;
        auto text = load_parse_input_file(path);
        if (text.is_err())
        {
            rep.errors ~= make_diag("M0603", "cannot read file `" ~ path ~ "`", Span.init);
            return rep;
        }
        auto key = canonical(path);
        bool was_imported = (key in imported_) !is null;
        imported_[key] = true;
        auto result = run_source(text.unwrap(), path, mode);
        if (!result.ok() && !was_imported) imported_.remove(key);
        return result;
    }

    RunReport run_source(string source, string file, RunMode mode)
    {
        sources_[file] = source;
        auto parsed = parse(source, file);
        if (!parsed.ok())
        {
            RunReport rep;
            rep.errors = parsed.diagnostics;
            return rep;
        }
        return run_program(parsed.program, mode);
    }

    /// Checks and executes a parsed program unit by unit.
    RunReport run_program(Program prog, RunMode mode)
    {
        RunReport rep;
        programs_ ~= prog;
        sources_[prog.file] = prog.source;
        auto items = prog.items;
        foreach (r; Checker.units(items))
        {
            Item it = items[r.b];
            if (it.kind == IK.Import)
            {
                auto ir = import_module(it.text, prog.file, it.span);
                if (!ir.ok()) { rep.errors ~= ir.errors; return rep; }
                continue;
            }
            if (it.kind == IK.Directive)
            {
                auto dr = directive(it.name, it.text);
                rep.text ~= dr.text;
                if (!dr.ok()) { rep.errors ~= dr.errors; return rep; }
                continue;
            }
            auto unit_scope = Scope.child(checker_.global);
            checker_.warnings = null;
            UnitInfo info;
            try info = checker_.check_unit(items, r.b, r.e, unit_scope, null, true);
            catch (MetaError err)
            {
                checker_.u.commit();
                rep.warnings ~= checker_.warnings;
                rep.errors ~= err.diag;
                return rep;
            }
            checker_.u.commit();
            Checker.commit_scope(unit_scope, checker_.global);
            foreach (w; checker_.warnings)
            {
                rep.warnings ~= w;
                if (mode != RunMode.Eval) rep.text ~= render(w);
            }
            checker_.warnings = null;
            try exec_unit(items, r.b, r.e, info, mode, rep);
            catch (MetaError err)
            {
                interp_.stack = null;
                stack_types_ = null;
                interp_.call_depth = 0;
                interp_.eval_depth = 0;
                rep.errors ~= err.diag;
                return rep;
            }
        }
        return rep;
    }

    // ---------------- primitives ----------------
    /// Registers a host primitive with an explicit MetaML type (trusted).
    void register_primitive(string name, string type, int arity, PrimFn fn)
    {
        add_primitive(name, type, arity, fn, false);
        allow_ ~= name;
    }

    /// Registers a typed host function; the MetaML type is derived from the D
    /// signature (Int, Float, Bool, String, Nullable, arrays, tuples, Value).
    void register_primitive(F)(string name, F fn) if (!is(F == PrimFn))
    {
        import std.traits : Parameters, ReturnType;
        alias P = Parameters!F;
        alias R = ReturnType!F;
        int tv = 0;
        string type;
        static foreach (T; P) type ~= host_type!T(tv) ~ " -> ";
        static if (P.length == 0) type = "Unit -> ";
        static if (is(R == void)) type ~= "Unit";
        else type ~= host_type!R(tv);
        PrimFn pf = (PrimContext, Value[] args) {
            P params;
            static foreach (i, T; P) params[i] = from_value!T(args[i]);
            static if (is(R == void)) { fn(params); return Value.unit(); }
            else return to_value!R(fn(params));
        };
        register_primitive(name, type, P.length == 0 ? 1 : cast(int) P.length, pf);
    }
    alias registerPrimitive = register_primitive;

    /// Registers a sandboxed library primitive (metalib host modules).
    void install_primitive(string name, string type, int arity, PrimFn fn)
    {
        add_primitive(name, type, arity, fn, false);
    }

    /// Declares MetaML source (types, helpers) on behalf of a host module.
    RunReport declare(string source, string file)
    {
        bool q = checker_.quiet;
        scope (exit) checker_.quiet = q;
        checker_.quiet = true;
        auto r = run_source(source, file, RunMode.Quiet);
        return r;
    }

    void set_primitive_allowlist(string[] names)
    {
        allow_ = names.dup;
        allow_all_ = false;
    }
    alias setPrimitiveAllowlist = set_primitive_allowlist;
    void allow_all_primitives(bool on = true) { allow_all_ = on; }
    bool primitive_allowed(Primitive p)
    {
        import std.algorithm : startsWith, endsWith;
        if (p.core || allow_all_) return true;
        foreach (a; allow_)
        {
            if (a == "*" || a == p.name) return true;
            if (a.length >= 2 && a.endsWith(".*") && p.name.startsWith(a[0 .. $ - 1])) return true;
        }
        return false;
    }
    Primitive find_primitive(string name)
    {
        if (auto p = name in prims_) return *p;
        return null;
    }
    string[] primitive_names() { return sortedKeys(prims_); }

    // ---------------- configuration ----------------
    void set_output(void delegate(string) o) { output_ = o; }
    void write(string s)
    {
        out_mutex_.lock();
        scope (exit) out_mutex_.unlock();
        output_(s);
    }
    /// Flushes the process's standard output (used before handing the
    /// terminal to a child process or exiting).
    void flush_output()
    {
        import std.stdio : stdout;
        out_mutex_.lock();
        scope (exit) out_mutex_.unlock();
        stdout.flush();
    }

    /// Wraps a host function as a MetaML function value.
    Value make_function(string name, int arity, PrimFn fn)
    {
        auto p = new Primitive;
        p.name = name;
        p.arity = arity < 1 ? 1 : arity;
        p.fn = fn;
        p.core = true;
        return interp_.prim_value(p);
    }

    /// A fresh interpreter sharing this session (for host threads).
    Interp spawn_interp(bool background = false)
    {
        auto it = new Interp;
        it.session = this;
        auto allow = allow_.dup;
        bool all = allow_all_;
        it.allowed = (Primitive p) {
            import std.algorithm : startsWith, endsWith;
            if (p.name.startsWith("Loader.") || p.name.startsWith("Module.")) return false;
            if (background && (p.name == "Threading.spawn" || p.name == "Fiber.create")) return false;
            if (p.core || all) return true;
            foreach (n; allow)
                if (n == "*" || n == p.name || (n.endsWith(".*") && p.name.startsWith(n[0 .. $ - 1]))) return true;
            return false;
        };
        auto globals = globals_.dup;
        it.global_lookup = (string n) { auto p = n in globals; return p is null ? null : *p; };
        it.max_depth = interp_.max_depth < 20000 ? interp_.max_depth : 20000;
        it.module_lock = module_lock_;
        return it;
    }

    /// Command-line arguments visible to scripts (Process.args).
    string[] args;

    string[] imported_files() { return sortedKeys(imported_); }
    string[] search_paths()
    {
        string[] result = search_path_.dup;
        if (stdlib_dir_.length) result ~= stdlib_dir_;
        return result;
    }
    /// Where `import "spec"` would resolve (canonical path), if anywhere.
    Nullable!string resolve_import(string spec, string from = "")
    {
        auto f = find_import(spec, from);
        if (!f.length) return Nullable!string.init;
        return nullable(f);
    }
    void disable_warning(string code) { checker_.disabled ~= code; }
    void set_max_depth(int d) { interp_.max_depth = d; }
    void add_search_path(string dir) { search_path_ ~= dir; }
    Checker checker() { return checker_; }
    Interp interp() { return interp_; }
    Frame root() { return root_; }

    /// Applies a MetaML function value from host code.
    EvalResult call(Value f, Value[] args)
    {
        try return EvalResult.from_ok(interp_.apply(f, args));
        catch (MetaError e)
        {
            interp_.call_depth = 0;
            interp_.eval_depth = 0;
            return EvalResult.from_err([e.diag]);
        }
    }

    /// Value bound to a global name (if any).
    Nullable!Value global(string name)
    {
        auto c = name in globals_;
        if (c is null || !(*c).defined) return Nullable!Value.init;
        return nullable((*c).value);
    }

    // ---------------- diagnostics ----------------
    void remember_source(string file, string text) { sources_[file] = text; }
    string render(Diagnostic d)
    {
        auto s = d.primary_span.file in sources_;
        return render_diagnostic(d, s is null ? null : *s);
    }

    // ---------------- REPL directives (§5.10) ----------------
    RunReport directive(string name, string arg)
    {
        auto rep = directive_impl(name, arg);
        foreach (ref d; rep.errors) if (!d.primary_span.file.length) d.primary_span.file = "<directive>";
        return rep;
    }

    RunReport directive_impl(string name, string arg)
    {
        RunReport rep;
        try
        {
            if (name == "type")
            {
                auto r = type_of(arg);
                if (r.is_err()) rep.errors = r.unwrap_err();
                else rep.text = "- : " ~ r.unwrap() ~ "\n";
                return rep;
            }
            if (name == "trace") return trace(arg);
            if (name == "load")
            {
                string path = unquote(arg);
                if (!path.length)
                {
                    rep.errors ~= make_diag("M0603", "`#load` expects a file name, e.g. `#load \"file.ml\"`", Span.init);
                    return rep;
                }
                last_loaded_ = path;
                return run_file(path, RunMode.Quiet);
            }
            if (name == "reload")
            {
                if (!last_loaded_.length)
                {
                    rep.errors ~= make_diag("M0602", "nothing to reload; use `#load` first", Span.init);
                    return rep;
                }
                return run_file(last_loaded_, RunMode.Quiet);
            }
            if (name == "stack")
            {
                if (!interp_.stack.length) rep.text = "(stack is empty)\n";
                foreach (i, v; interp_.stack)
                    rep.text ~= "  [" ~ to!string(i) ~ "] " ~ show_value(v)
                        ~ (i < stack_types_.length ? " : " ~ stack_types_[i] : "") ~ "\n";
                return rep;
            }
            if (name == "clear")
            {
                interp_.stack = null;
                stack_types_ = null;
                return rep;
            }
            if (name == "ast")
            {
                sources_["<ast>"] = arg;
                auto p = parse(arg, "<ast>");
                if (!p.ok()) rep.errors = p.diagnostics;
                else rep.text = dump_ast(p.program) ~ "\n";
                return rep;
            }
            if (name == "quit")
            {
                quit_requested_ = true;
                return rep;
            }
            rep.errors ~= make_diag("M0601", "unknown directive `#" ~ name ~ "`; try `#help`", Span.init);
        }
        catch (MetaError e) rep.errors ~= e.diag;
        return rep;
    }

    bool quit_requested() const { return quit_requested_; }
    string last_loaded() const { return last_loaded_; }

    /// Completion candidates: identifiers, module members, keywords.
    string[] completions(string prefix)
    {
        import std.algorithm : startsWith, uniq;
        import std.string : lastIndexOf, split;
        string[] result;
        string p = prefix;
        auto dot = p.lastIndexOf('.');
        if (dot >= 0)
        {
            string mpath = p[0 .. dot], rest = p[dot + 1 .. $];
            auto segs = mpath.split(".");
            auto m = checker_.global.module_(segs[0]);
            Scope cur = m is null ? null : *m;
            for (size_t i = 1; cur !is null && i < segs.length; ++i)
            {
                auto n = cur.modules.find(segs[i]);
                cur = n is null ? null : *n;
            }
            if (cur !is null)
            {
                foreach (k; cur.vars.keys) if (k.startsWith(rest)) result ~= mpath ~ "." ~ k;
                foreach (k; cur.modules.keys) if (k.startsWith(rest)) result ~= mpath ~ "." ~ k;
                foreach (k; cur.ctors.keys) if (k.startsWith(rest)) result ~= mpath ~ "." ~ k;
            }
            foreach (k; sortedKeys(prims_))
                if (k.startsWith(p) && primitive_allowed(prims_[k])) result ~= k;
        }
        else
        {
            foreach (k; checker_.global.vars.keys) if (k.startsWith(p)) result ~= k;
            foreach (k; checker_.global.modules.keys) if (k.startsWith(p)) result ~= k;
            foreach (k; checker_.global.ctors.keys) if (k.startsWith(p)) result ~= k;
            static immutable kws = ["module", "type", "sig", "struct", "end", "val", "let", "rec", "in",
                "if", "then", "else", "match", "with", "fn", "fun", "import", "include", "open", "do",
                "true", "false", "when", "for"];
            foreach (k; kws) if (k.startsWith(p)) result ~= k;
        }
        result.sort();
        return result.uniq.array;
    }

    void set_trace_depth(int d) { interp_.trace_limit = d; }

private:
    static string executable_dir()
    {
        import std.file : thisExePath;
        import std.path : dirName;
        try return dirName(thisExePath());
        catch (Exception) return "";
    }

    static string canonical(string path)
    {
        import core.sys.posix.stdlib : realpath;
        import core.stdc.stdlib : free;
        import std.string : toStringz, fromStringz;
        import std.path : absolutePath, buildNormalizedPath;
        auto r = realpath(path.toStringz, null);
        if (r !is null)
        {
            string s = fromStringz(r).idup;
            free(r);
            return s;
        }
        return buildNormalizedPath(absolutePath(path));
    }

    static string unquote(string a)
    {
        if (a.length >= 2 && a[0] == '"' && a[$ - 1] == '"') return a[1 .. $ - 1];
        return a;
    }

    void add_primitive(string name, string type, int arity, PrimFn fn, bool core_)
    {
        if (!name.length || !type.length || fn is null)
            throw new Exception("primitive requires a name, type and callback");
        auto p = new Primitive;
        p.name = name;
        p.type_src = type;
        p.arity = arity < 1 ? 1 : arity;
        p.fn = fn;
        p.core = core_;
        prims_[name] = p;
    }
    void core(string name, string type, int arity, PrimFn fn) { add_primitive(name, type, arity, fn, true); }

    TypeNode primitive_type(Primitive p, Span sp)
    {
        if (p.scheme !is null) return p.scheme;
        if (p.resolving) checker_.error("M0404", "primitive `" ~ p.name ~ "` has a recursive type", sp);
        p.resolving = true;
        auto previous_level = checker_.level;
        scope (exit)
        {
            p.resolving = false;
            checker_.level = previous_level;
        }
        auto te = parse_type_expr(p.type_src, "<primitive " ~ p.name ~ ">");
        if (te.is_err())
        {
            p.resolving = false;
            checker_.error("M0404", "primitive `" ~ p.name ~ "` has an invalid type `" ~ p.type_src ~ "`", sp);
        }
        int saved = checker_.level;
        try
        {
            checker_.level = 0;
            p.scheme = checker_.sig_scheme(te.unwrap(), checker_.global);
            checker_.level = saved;
        }
        catch (MetaError e)
        {
            checker_.level = saved;
            p.resolving = false;
            checker_.error("M0404", "primitive `" ~ p.name ~ "` has an invalid type: " ~ e.diag.message, sp);
        }
        p.resolving = false;
        return p.scheme;
    }

    TypeNode check_expr(Expr e)
    {
        auto scope_ = Scope.child(checker_.global);
        auto it = new Item;
        it.kind = IK.Expr;
        it.body = e;
        it.span = e.span;
        Item[] items = [it];
        auto info = checker_.check_unit(items, 0, 1, scope_, null, true);
        return info.expr_type;
    }

    RunReport trace(string arg)
    {
        RunReport rep;
        sources_["<trace>"] = arg;
        auto pe = parse_expression(arg, "<trace>");
        if (pe.is_err())
        {
            rep.errors = pe.unwrap_err();
            return rep;
        }
        TypeNode t;
        try
        {
            t = check_expr(pe.unwrap());
            checker_.u.commit();
        }
        catch (MetaError e)
        {
            checker_.u.commit();
            rep.errors ~= e.diag;
            return rep;
        }
        string lines;
        interp_.tracing = true;
        interp_.trace_base = interp_.call_depth;
        interp_.trace_sink = (string l) { lines ~= l ~ "\n"; };
        try
        {
            Value v = interp_.eval(pe.unwrap(), root_);
            interp_.tracing = false;
            rep.text = lines ~ "- : " ~ show_type(t) ~ " = " ~ show_value(v) ~ "\n";
        }
        catch (MetaError e)
        {
            interp_.tracing = false;
            interp_.call_depth = 0;
            interp_.eval_depth = 0;
            rep.text = lines;
            rep.errors ~= e.diag;
        }
        return rep;
    }

    RunReport import_module(string spec, string from, Span sp = Span.init)
    {
        RunReport rep;
        string found = find_import(spec, from);
        if (!found.length)
        {
            rep.errors ~= make_diag("M0505", "cannot find the module file `" ~ spec ~ "`", sp);
            return rep;
        }
        if (found in imported_) return rep;
        imported_[found] = true;
        auto text = load_parse_input_file(found);
        if (text.is_err())
        {
            imported_.remove(found);
            rep.errors ~= make_diag("M0505", "cannot read the module file `" ~ found ~ "`", sp);
            return rep;
        }
        string label = found;
        if (stdlib_dir_.length)
        {
            import std.algorithm : startsWith;
            auto base = canonical(stdlib_dir_);
            if (found.startsWith(base)) label = "metalib" ~ found[base.length .. $];
        }
        auto result = run_source(text.unwrap(), label, RunMode.Quiet);
        if (!result.ok()) imported_.remove(found);
        return result;
    }

    string find_import(string spec, string from)
    {
        import std.file : exists, isFile, getcwd;
        import std.path : dirName, buildPath;
        string[] bases;
        if (from.length && from[0] != '<') bases ~= dirName(from);
        try bases ~= getcwd();
        catch (Exception) {}
        foreach (s; search_path_) bases ~= s;
        if (stdlib_dir_.length) bases ~= stdlib_dir_;
        foreach (b; bases)
            foreach (cand; [buildPath(b, spec), buildPath(b, spec ~ ".ml"), buildPath(b, spec, "mod.ml")])
            {
                try
                {
                    if (exists(cand) && isFile(cand)) return canonical(cand);
                }
                catch (Exception) {}
            }
        return "";
    }

    void bind_global(Cell cell) { globals_[cell.name] = cell; }

    void exec_unit(Item[] items, size_t b, size_t e, UnitInfo info, RunMode mode, ref RunReport rep)
    {
        Item first = items[b];
        bool repl = mode == RunMode.Repl;
        switch (first.kind)
        {
            case IK.Def:
            case IK.Sig:
            {
                bool any = false;
                foreach (i; b .. e)
                {
                    auto it = items[i];
                    if (it.kind != IK.Def) continue;
                    any = true;
                    Value v = interp_.eval(it.value, root_);
                    auto cell = it.cells[0];
                    cell.value = v;
                    cell.defined = true;
                    bind_global(cell);
                    rep.value = v;
                }
                if (any && repl)
                {
                    interp_.stack = null;
                    stack_types_ = null;
                }
                if (repl)
                    foreach (d; info.defs)
                    {
                        string line = "val " ~ d[0] ~ " : " ~ show_type(d[1]);
                        auto g = d[0] in globals_;
                        if (g !is null && !is_callable((*g).value)) line ~= " = " ~ show_value((*g).value);
                        rep.text ~= line ~ "\n";
                    }
                if (info.defs.length) rep.type = info.defs[$ - 1][1];
                return;
            }
            case IK.Type:
                if (repl)
                    foreach (i; b .. e)
                    {
                        string line = "type " ~ items[i].name;
                        foreach (p; items[i].tparams) line ~= " " ~ (p[0] == '\'' ? p : "'" ~ p);
                        rep.text ~= line ~ "\n";
                    }
                return;
            case IK.Module:
            {
                module_lock_.lock();
                scope (exit) module_lock_.unlock();
                root_.mods ~= tuple(first.name, interp_.make_module(first, root_));
                if (repl) rep.text ~= "module " ~ first.name ~ "\n";
                return;
            }
            case IK.Functor:
            {
                module_lock_.lock();
                scope (exit) module_lock_.unlock();
                auto f = new FunctorObj;
                f.item = first;
                f.env = root_;
                root_.functors ~= tuple(first.name, f);
                if (repl) rep.text ~= "functor " ~ first.name ~ "\n";
                return;
            }
            case IK.ModType:
                if (repl) rep.text ~= "module type " ~ first.name ~ "\n";
                return;
            case IK.Open:
            case IK.Include:
            {
                module_lock_.lock();
                scope (exit) module_lock_.unlock();
                auto m = interp_.resolve_module(first.path, root_, first.span);
                interp_.force(m, first.span);
                foreach (cell; first.cells)
                {
                    for (size_t k = m.fields.length; k-- > 0;)
                        if (m.fields[k][0] == cell.name)
                        {
                            cell.value = m.fields[k][1];
                            cell.defined = true;
                            break;
                        }
                    bind_global(cell);
                }
                foreach (kv; m.mods) root_.mods ~= kv;
                foreach (kv; m.functors) root_.functors ~= kv;
                return;
            }
            case IK.Expr:
            {
                Value v = interp_.eval(first.body, root_);
                rep.value = v;
                rep.type = info.expr_type;
                if (repl)
                {
                    interp_.stack ~= v;
                    stack_types_ ~= show_type(info.expr_type);
                    auto t = prune(info.expr_type);
                    if (!(t.kind == TK.Con && t.stamp == stamps.Unit))
                        rep.text ~= "- : " ~ show_type(info.expr_type) ~ " = " ~ show_value(v) ~ "\n";
                }
                return;
            }
            default:
                return;
        }
    }

    void register_core() { registerCorePrimitives(this); }
}

/// REPL-style rendering of an evaluation result (or its diagnostics).
string to_string(Value v) { return show_value(v); }
string to_string(EvalResult r)
{
    if (r.is_ok()) return show_value(r.unwrap());
    string result;
    foreach (d; r.unwrap_err()) result ~= d.headline() ~ "\n";
    return result;
}

// ============================================================
//  Core host primitives (registered by both dialects, §18.2)
// ============================================================

long fnv1a(string s)
{
    ulong h = 14695981039346656037UL;
    foreach (char c; s)
    {
        h ^= cast(ubyte) c;
        h *= 1099511628211UL;
    }
    return cast(long) h;
}

string ascii_map(string s, bool upper)
{
    char[] r = s.dup;
    foreach (ref c; r)
    {
        if (upper && c >= 'a' && c <= 'z') c = cast(char)(c - 32);
        else if (!upper && c >= 'A' && c <= 'Z') c = cast(char)(c + 32);
    }
    return r.idup;
}

string trim_ws(string s)
{
    size_t b = 0, e = s.length;
    bool ws(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
    while (b < e && ws(s[b])) ++b;
    while (e > b && ws(s[e - 1])) --e;
    return s[b .. e];
}

/// Stable merge sort with a three-way comparator (same algorithm in C++).
void merge_sort(ref Value[] v, scope int delegate(Value, Value) cmp)
{
    if (v.length < 2) return;
    auto tmp = new Value[v.length];
    for (size_t width = 1; width < v.length; width *= 2)
    {
        for (size_t lo = 0; lo < v.length; lo += 2 * width)
        {
            size_t mid = min(lo + width, v.length);
            size_t hi = min(lo + 2 * width, v.length);
            size_t i = lo, j = mid, k = lo;
            while (i < mid && j < hi)
            {
                if (cmp(v[j], v[i]) < 0) tmp[k++] = v[j++];
                else tmp[k++] = v[i++];
            }
            while (i < mid) tmp[k++] = v[i++];
            while (j < hi) tmp[k++] = v[j++];
        }
        auto t = v; v = tmp; tmp = t;
    }
}

string[] split_string(string sep, string s)
{
    string[] result;
    if (!sep.length)
    {
        foreach (char c; s) result ~= [c].idup;
        return result;
    }
    size_t start = 0;
    while (true)
    {
        auto p = s[start .. $].indexOf(sep);
        if (p < 0)
        {
            result ~= s[start .. $];
            return result;
        }
        result ~= s[start .. start + p];
        start = start + p + sep.length;
    }
}

string replace_all(string from, string to_, string s)
{
    if (!from.length) return s;
    string result;
    size_t start = 0;
    while (true)
    {
        auto p = s[start .. $].indexOf(from);
        if (p < 0)
        {
            result ~= s[start .. $];
            return result;
        }
        result ~= s[start .. start + p];
        result ~= to_;
        start = start + p + from.length;
    }
}

private long clampl(long v, long lo, long hi) { return v < lo ? lo : v > hi ? hi : v; }

private void registerCorePrimitives(Session ss)
{
    alias V = Value[];
    alias C = PrimContext;
    static string S(Value v) { return str_of(v); }
    static double F(Value v) { return v.kind == VK.Int ? cast(double) v.i : v.f; }
    auto interp = ss.interp_;

    // ---- Core ----
    ss.core("Core.show", "'a -> String", 1, (C c, V a) => make_str(show_value(a[0])));
    ss.core("Core.error", "String -> 'a", 1, delegate Value(C c, V a) { c.error("M0407", S(a[0])); });
    ss.core("Core.compare", "'a -> 'a -> Int", 2, (C c, V a) => Value.integer(compare_values(a[0], a[1], c.span)));
    ss.core("Core.equal", "'a -> 'a -> Bool", 2, (C c, V a) => Value.boolean(values_equal(a[0], a[1], c.span)));
    ss.core("Core.hash", "'a -> Int", 1, (C c, V a) => Value.integer(fnv1a(show_value(a[0]))));
    ss.core("Core.ignore", "'a -> Unit", 1, (C c, V a) => Value.unit());
    ss.core("Core.version", "Unit -> String", 1, (C c, V a) => make_str(metamlVersion));

    // ---- Int ----
    ss.core("Int.parse", "String -> Option Int", 1, delegate Value(C c, V a) {
        auto r = parse_int(S(a[0]));
        return r.isNull ? make_none() : make_some(Value.integer(r.get));
    });
    ss.core("Int.render", "Int -> String", 1, (C c, V a) => make_str(to!string(a[0].i)));
    ss.core("Int.to_float", "Int -> Float", 1, (C c, V a) => Value.floating(cast(double) a[0].i));
    ss.core("Int.abs", "Int -> Int", 1, (C c, V a) => Value.integer(a[0].i < 0 ? wrap_neg(a[0].i) : a[0].i));
    ss.core("Int.pow", "Int -> Int -> Int", 2, delegate Value(C c, V a) {
        long base = a[0].i, e = a[1].i, r = 1;
        if (e < 0) c.error("M0404", "`Int.pow` expects a non-negative exponent");
        while (e > 0)
        {
            if (e & 1) r = wrap_mul(r, base);
            base = wrap_mul(base, base);
            e >>= 1;
        }
        return Value.integer(r);
    });
    ss.core("Int.land", "Int -> Int -> Int", 2, (C c, V a) => Value.integer(a[0].i & a[1].i));
    ss.core("Int.lor", "Int -> Int -> Int", 2, (C c, V a) => Value.integer(a[0].i | a[1].i));
    ss.core("Int.lxor", "Int -> Int -> Int", 2, (C c, V a) => Value.integer(a[0].i ^ a[1].i));
    ss.core("Int.shift_left", "Int -> Int -> Int", 2,
        (C c, V a) => Value.integer(cast(long)(cast(ulong) a[0].i << (a[1].i & 63))));
    ss.core("Int.shift_right", "Int -> Int -> Int", 2, (C c, V a) => Value.integer(a[0].i >> (a[1].i & 63)));
    ss.core("Int.min_value", "Unit -> Int", 1, (C c, V a) => Value.integer(long.min));
    ss.core("Int.max_value", "Unit -> Int", 1, (C c, V a) => Value.integer(long.max));
    ss.core("Int.checked_add", "Int -> Int -> Option Int", 2, delegate Value(C c, V a) {
        auto x = a[0].i, y = a[1].i;
        if ((y > 0 && x > long.max - y) || (y < 0 && x < long.min - y)) return make_none();
        return make_some(Value.integer(wrap_add(x, y)));
    });
    ss.core("Int.checked_sub", "Int -> Int -> Option Int", 2, delegate Value(C c, V a) {
        auto x = a[0].i, y = a[1].i;
        if ((y < 0 && x > long.max + y) || (y > 0 && x < long.min + y)) return make_none();
        return make_some(Value.integer(wrap_sub(x, y)));
    });
    ss.core("Int.checked_mul", "Int -> Int -> Option Int", 2, delegate Value(C c, V a) {
        auto x = a[0].i, y = a[1].i;
        ulong magnitude(long n) { return n < 0 ? 0UL - cast(ulong) n : cast(ulong) n; }
        ulong limit = (x < 0) != (y < 0) ? (1UL << 63) : cast(ulong) long.max;
        if (y != 0 && magnitude(x) > limit / magnitude(y)) return make_none();
        return make_some(Value.integer(wrap_mul(x, y)));
    });

    // ---- Float ----
    ss.core("Float.of_int", "Int -> Float", 1, (C c, V a) => Value.floating(cast(double) a[0].i));
    ss.core("Float.to_string", "Float -> String", 1, (C c, V a) => make_str(format_float(F(a[0]))));
    ss.core("Float.parse", "String -> Option Float", 1, delegate Value(C c, V a) {
        auto r = parse_float(S(a[0]));
        return r.isNull ? make_none() : make_some(Value.floating(r.get));
    });
    ss.core("Float.of_string", "String -> Float", 1, delegate Value(C c, V a) {
        auto r = parse_float(S(a[0]));
        if (r.isNull)
            c.error("M0404", "cannot convert " ~ quote_string(S(a[0])) ~ " to a Float; use `Float.parse` for an Option");
        return Value.floating(r.get);
    });
    ss.core("Float.truncate", "Float -> Int", 1, (C c, V a) => Value.integer(float_to_int(F(a[0]))));
    static struct Unary { string name; double function(double) fn; }
    static immutable Unary[] unary = [
        Unary("Float.floor", (double x) => c_floor(x)),
        Unary("Float.ceil", (double x) => c_ceil(x)),
        Unary("Float.round", (double x) => c_round(x)),
        Unary("Float.sqrt", (double x) => c_sqrt(x)),
        Unary("Float.exp", (double x) => c_exp(x)),
        Unary("Float.log", (double x) => c_log(x)),
        Unary("Float.sin", (double x) => c_sin(x)),
        Unary("Float.cos", (double x) => c_cos(x)),
        Unary("Float.tan", (double x) => c_tan(x)),
        Unary("Float.abs", (double x) => c_fabs(x)),
    ];
    // (a helper function gives each delegate its own captured context)
    static PrimFn unaryPrim(double function(double) fn)
    {
        return (C c, V a) => Value.floating(fn(F(a[0])));
    }
    foreach (ref u; unary) ss.core(u.name, "Float -> Float", 1, unaryPrim(u.fn));
    ss.core("Float.pow", "Float -> Float -> Float", 2, (C c, V a) => Value.floating(c_pow(F(a[0]), F(a[1]))));
    ss.core("Float.atan2", "Float -> Float -> Float", 2, (C c, V a) => Value.floating(c_atan2(F(a[0]), F(a[1]))));
    ss.core("Float.is_nan", "Float -> Bool", 1, (C c, V a) => Value.boolean(isNaNd(F(a[0]))));
    ss.core("Float.is_finite", "Float -> Bool", 1,
        (C c, V a) => Value.boolean(!isNaNd(F(a[0])) && !isInfd(F(a[0]))));
    ss.core("Float.is_infinite", "Float -> Bool", 1, (C c, V a) => Value.boolean(isInfd(F(a[0]))));
    ss.core("Float.infinity", "Unit -> Float", 1, (C c, V a) => Value.floating(double.infinity));
    ss.core("Float.nan", "Unit -> Float", 1, (C c, V a) => Value.floating(double.nan));

    // ---- String (bytes; UTF-8 helpers live in metalib/d/String.d) ----
    ss.core("String.length", "String -> Int", 1, (C c, V a) => Value.integer(cast(long) S(a[0]).length));
    ss.core("String.concat", "String -> String -> String", 2, (C c, V a) => make_str(S(a[0]) ~ S(a[1])));
    ss.core("String.sub", "Int -> Int -> String -> String", 3, delegate Value(C c, V a) {
        string s = S(a[2]);
        long n = cast(long) s.length;
        long start = clampl(a[0].i, 0, n);
        long len = clampl(a[1].i, 0, n - start);
        return make_str(s[cast(size_t) start .. cast(size_t)(start + len)]);
    });
    ss.core("String.char_at", "Int -> String -> Option String", 2, delegate Value(C c, V a) {
        string s = S(a[1]);
        if (a[0].i < 0 || a[0].i >= cast(long) s.length) return make_none();
        return make_some(make_str(s[cast(size_t) a[0].i .. cast(size_t) a[0].i + 1]));
    });
    ss.core("String.index_of", "String -> String -> Option Int", 2, delegate Value(C c, V a) {
        auto p = S(a[1]).indexOf(S(a[0]));
        if (p < 0) return make_none();
        return make_some(Value.integer(cast(long) p));
    });
    ss.core("String.chars", "String -> [String]", 1, delegate Value(C c, V a) {
        Value[] result;
        string s = S(a[0]);
        foreach (i; 0 .. s.length) result ~= make_str(s[i .. i + 1]);
        return make_list(result);
    });
    ss.core("String.join", "String -> [String] -> String", 2, delegate Value(C c, V a) {
        string result;
        bool first = true;
        foreach (x; list_items(a[1]))
        {
            if (!first) result ~= S(a[0]);
            first = false;
            result ~= S(x);
        }
        return make_str(result);
    });
    ss.core("String.split", "String -> String -> [String]", 2, delegate Value(C c, V a) {
        Value[] result;
        foreach (p; split_string(S(a[0]), S(a[1]))) result ~= make_str(p);
        return make_list(result);
    });
    ss.core("String.trim", "String -> String", 1, (C c, V a) => make_str(trim_ws(S(a[0]))));
    ss.core("String.upper", "String -> String", 1, (C c, V a) => make_str(ascii_map(S(a[0]), true)));
    ss.core("String.lower", "String -> String", 1, (C c, V a) => make_str(ascii_map(S(a[0]), false)));
    ss.core("String.starts_with", "String -> String -> Bool", 2, delegate Value(C c, V a) {
        string p = S(a[0]), s = S(a[1]);
        return Value.boolean(s.length >= p.length && s[0 .. p.length] == p);
    });
    ss.core("String.ends_with", "String -> String -> Bool", 2, delegate Value(C c, V a) {
        string p = S(a[0]), s = S(a[1]);
        return Value.boolean(s.length >= p.length && s[$ - p.length .. $] == p);
    });
    ss.core("String.contains", "String -> String -> Bool", 2,
        (C c, V a) => Value.boolean(S(a[1]).indexOf(S(a[0])) >= 0));
    ss.core("String.replace", "String -> String -> String -> String", 3,
        (C c, V a) => make_str(replace_all(S(a[0]), S(a[1]), S(a[2]))));
    ss.core("String.repeat", "Int -> String -> String", 2, delegate Value(C c, V a) {
        string result;
        if (a[0].i <= 0 || !S(a[1]).length) return make_str("");
        if (cast(ulong) a[0].i > 100_000_000UL / S(a[1]).length)
            c.error("M0404", "`String.repeat` result is too large");
        for (long k = 0; k < a[0].i; ++k) result ~= S(a[1]);
        return make_str(result);
    });
    ss.core("String.compare", "String -> String -> Int", 2,
        (C c, V a) => Value.integer(compare_values(a[0], a[1], c.span)));
    ss.core("String.to_codes", "String -> [Int]", 1, delegate Value(C c, V a) {
        Value[] result;
        foreach (char ch; S(a[0])) result ~= Value.integer(cast(ubyte) ch);
        return make_list(result);
    });
    ss.core("String.of_codes", "[Int] -> String", 1, delegate Value(C c, V a) {
        char[] result;
        foreach (x; list_items(a[0])) result ~= cast(char)(x.i & 255);
        return make_str(result.idup);
    });

    // ---- List ----
    ss.core("List.length", "['a] -> Int", 1, delegate Value(C c, V a) {
        long n = 0;
        for (ConsObj x = cons_of(a[0]); x !is null; x = x.tail) ++n;
        return Value.integer(n);
    });
    ss.core("List.reverse", "['a] -> ['a]", 1, delegate Value(C c, V a) {
        Value acc;
        acc.kind = VK.List;
        for (ConsObj x = cons_of(a[0]); x !is null; x = x.tail) acc = cons_value(x.head, acc);
        return acc;
    });
    ss.core("List.nth", "Int -> ['a] -> Option 'a", 2, delegate Value(C c, V a) {
        long k = a[0].i;
        if (k < 0) return make_none();
        for (ConsObj x = cons_of(a[1]); x !is null; x = x.tail, --k)
            if (k == 0) return make_some(x.head);
        return make_none();
    });
    ss.core("List.range", "Int -> Int -> [Int]", 2, delegate Value(C c, V a) {
        Value[] result;
        if (a[1].i > a[0].i && cast(ulong) a[1].i - cast(ulong) a[0].i > 50_000_000UL)
            c.error("M0404", "`List.range` is too large");
        for (long k = a[0].i; k < a[1].i; ++k) result ~= Value.integer(k);
        return make_list(result);
    });
    ss.core("List.sum", "Num 'a => ['a] -> 'a", 1, delegate Value(C c, V a) {
        Value acc = Value.integer(0);
        foreach (x; list_items(a[0])) acc = interp.binop("+", acc, x, c.span);
        return acc;
    });
    ss.core("List.product", "Num 'a => ['a] -> 'a", 1, delegate Value(C c, V a) {
        Value acc = Value.integer(1);
        foreach (x; list_items(a[0])) acc = interp.binop("*", acc, x, c.span);
        return acc;
    });
    ss.core("List.sort", "['a] -> ['a]", 1, delegate Value(C c, V a) {
        auto items = list_items(a[0]);
        Span sp = c.span;
        merge_sort(items, (Value x, Value y) => compare_values(x, y, sp));
        return make_list(items);
    });
    ss.core("List.sort_by", "('a -> 'a -> Int) -> ['a] -> ['a]", 2, delegate Value(C c, V a) {
        auto items = list_items(a[1]);
        Value cmp = a[0];
        merge_sort(items, (Value x, Value y) => cast(int) clampl(c.apply(cmp, [x, y]).i, -1, 1));
        return make_list(items);
    });
    ss.core("List.concat", "[['a]] -> ['a]", 1, delegate Value(C c, V a) {
        Value[] result;
        foreach (xs; list_items(a[0])) foreach (x; list_items(xs)) result ~= x;
        return make_list(result);
    });
    ss.core("List.append", "['a] -> ['a] -> ['a]", 2, (C c, V a) => interp.binop("@", a[0], a[1], c.span));

    // ---- IO ----
    ss.core("IO.print", "String -> Unit", 1, delegate Value(C c, V a) { c.print(S(a[0])); return Value.unit(); });
    ss.core("IO.println", "String -> Unit", 1, delegate Value(C c, V a) { c.print(S(a[0]) ~ "\n"); return Value.unit(); });
    ss.core("IO.eprint", "String -> Unit", 1, delegate Value(C c, V a) {
        import std.stdio : stdout, stderr;
        stdout.flush();
        stderr.write(S(a[0]));
        stderr.flush();
        return Value.unit();
    });
    ss.core("IO.read_line", "Unit -> Option String", 1, delegate Value(C c, V a) {
        import std.stdio : stdin;
        auto line = stdin.readln();
        if (line is null) return make_none();
        if (line.length && line[$ - 1] == '\n') line = line[0 .. $ - 1];
        if (line.length && line[$ - 1] == '\r') line = line[0 .. $ - 1];
        return make_some(make_str(line));
    });
}

// ============================================================
//  REPL Language policy for DSLtk.Session (§11)
// ============================================================

final class ReplLanguage
{
    alias Program = .Program;
    alias Value = string;

    MetaML.Session session;
    int submission = 0;

    this(MetaML.Session s) { session = s; }

    ParseResult!Program parse(string source)
    {
        string file = "<repl:" ~ to!string(submission + 1) ~ ">";
        session.remember_source(file, source);
        auto o = .parse(source, file);
        if (o.ok()) ++submission;
        if (!o.ok())
            return o.incomplete ? ParseResult!Program.incomplete(o.diagnostics) : ParseResult!Program.err(o.diagnostics);
        return ParseResult!Program.ok(o.program);
    }

    Result!(string, Diagnostic[]) eval(ref Program p)
    {
        alias R = Result!(string, Diagnostic[]);
        auto r = session.run_program(p, RunMode.Repl);
        if (!r.ok())
        {
            Diagnostic[] all = r.warnings.dup;
            all ~= r.errors;
            if (r.text.length) session.write(r.text);
            return R.from_err(all);
        }
        return R.from_ok(r.text);
    }

    string format(string v) { return v; }
    string format_error(Diagnostic d) { return session.render(d); }
}

alias ReplSessionT = DSLtk.Session!ReplLanguage;

/// Registers the MetaML directives (§5.10, §11.2) on a DSLtk REPL session.
void install_repl_directives(ReplSessionT repl, MetaML.Session s)
{
    repl.set_unknown_directive_code("M0601");
    static struct D { string name; string help; }
    static immutable D[] ds = [
        D("type", "print the inferred type of expr"),
        D("load", "load a MetaML source file"),
        D("reload", "reload the last loaded file"),
        D("trace", "evaluate with a reduction trace"),
        D("stack", "print the implicit value stack"),
        D("clear", "clear the value stack"),
        D("ast", "print the AST of source"),
    ];
    // (a helper function gives each delegate its own captured context)
    static void delegate(ReplSessionT, string) handler(MetaML.Session s, string name)
    {
        return (ReplSessionT r, string arg) {
            auto rep = s.directive(name, arg);
            if (rep.text.length) r.write(rep.text);
            foreach (dg; rep.errors) r.write(s.render(dg));
            if (!rep.ok()) r.mark_error();
        };
    }
    foreach (ref d; ds) repl.register_directive(d.name, d.help, handler(s, d.name));
    repl.set_completion((string prefix) => s.completions(prefix));
}
