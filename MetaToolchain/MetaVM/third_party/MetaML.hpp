/**
 * @file MetaML.hpp
 * @brief MetaML — an embeddable, header-only dialect of ML (C++ dialect).
 *
 * MetaML is a functional / tacit / concatenative / modular language built on
 * DSLtk.hpp. This header is the complete C++ dialect: lexer, parser, AST,
 * Hindley–Milner type checker (with row-polymorphic records, numeric
 * constraints and ML modules), evaluator, core host primitives, the embedding
 * Session and the REPL Language policy for dsltk::repl::Session.
 *
 * Its twin is MetaML.d; both MUST behave identically (AGENTS.md §9). Every
 * algorithm here is mirrored function-for-function in MetaML.d.
 *
 * Minimal embedding:
 *
 *   #include <MetaML.hpp>
 *   int main() {
 *       metaml::Session s;
 *       s.load_stdlib();
 *       s.eval("val x := 1 + 2");
 *       auto v = s.eval("x * 10");
 *       std::cout << metaml::to_string(v) << "\n";   // 30
 *   }
 *
 * Requirements: C++20 (DSLtk.hpp uses concepts and class NTTPs).
 */
#pragma once

#include "DSLtk.hpp"

#include <cmath>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

namespace metaml
{

using dsl::Diagnostic;
using dsl::Severity;
using dsl::Span;

inline constexpr const char *version = "1.0.0";
inline constexpr const char *dialect = "cpp";

// ============================================================
//  Utilities shared by every stage (mirrored in MetaML.d)
// ============================================================

namespace detail
{

/// Deterministic shortest round-trip float rendering (identical in MetaML.d).
inline std::string
format_float (double d)
{
  if (std::isnan (d))
    return "nan";
  if (std::isinf (d))
    return d < 0 ? "-inf" : "inf";
  if (d == 0.0)
    return std::signbit (d) ? "-0.0" : "0.0";
  char buf[64];
  int prec = 1;
  for (; prec <= 17; ++prec)
    {
      std::snprintf (buf, sizeof buf, "%.*e", prec - 1, d);
      if (std::strtod (buf, nullptr) == d)
        break;
    }
  std::string s (buf);
  bool neg = false;
  if (!s.empty () && s[0] == '-')
    {
      neg = true;
      s.erase (0, 1);
    }
  std::size_t epos = s.find ('e');
  std::string mant = s.substr (0, epos);
  std::string es = s.substr (epos + 1);
  int exp = 0;
  bool eneg = false;
  std::size_t k = 0;
  if (k < es.size () && (es[k] == '+' || es[k] == '-'))
    {
      eneg = es[k] == '-';
      ++k;
    }
  for (; k < es.size (); ++k)
    exp = exp * 10 + (es[k] - '0');
  if (eneg)
    exp = -exp;
  std::string digits;
  for (char c : mant)
    if (c != '.')
      digits += c;
  while (digits.size () > 1 && digits.back () == '0')
    digits.pop_back ();
  std::string out;
  if (exp >= -5 && exp < 17)
    {
      if (exp >= 0)
        {
          std::size_t intlen = static_cast<std::size_t> (exp) + 1;
          if (digits.size () <= intlen)
            out = digits + std::string (intlen - digits.size (), '0') + ".0";
          else
            out = digits.substr (0, intlen) + "." + digits.substr (intlen);
        }
      else
        out = "0." + std::string (static_cast<std::size_t> (-exp - 1), '0')
              + digits;
    }
  else
    {
      out = digits.substr (0, 1);
      if (digits.size () > 1)
        out += "." + digits.substr (1);
      out += "e" + std::to_string (exp);
    }
  return neg ? "-" + out : out;
}

/// Quotes a string with MetaML escapes.
inline std::string
quote_string (std::string_view s)
{
  static const char *hex = "0123456789abcdef";
  std::string out = "\"";
  for (char ch : s)
    {
      unsigned char c = static_cast<unsigned char> (ch);
      switch (c)
        {
        case '\n':
          out += "\\n";
          break;
        case '\t':
          out += "\\t";
          break;
        case '\r':
          out += "\\r";
          break;
        case '"':
          out += "\\\"";
          break;
        case '\\':
          out += "\\\\";
          break;
        default:
          if (c < 0x20 || c == 0x7f)
            {
              out += "\\u{";
              out += hex[c >> 4];
              out += hex[c & 15];
              out += "}";
            }
          else
            out += ch;
        }
    }
  return out + "\"";
}

/// Appends the UTF-8 encoding of a code point.
inline bool
append_utf8 (std::string &out, std::uint32_t cp)
{
  if (cp < 0x80)
    out += static_cast<char> (cp);
  else if (cp < 0x800)
    {
      out += static_cast<char> (0xC0 | (cp >> 6));
      out += static_cast<char> (0x80 | (cp & 0x3F));
    }
  else if (cp < 0x10000)
    {
      if (cp >= 0xD800 && cp <= 0xDFFF)
        return false;
      out += static_cast<char> (0xE0 | (cp >> 12));
      out += static_cast<char> (0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char> (0x80 | (cp & 0x3F));
    }
  else if (cp <= 0x10FFFF)
    {
      out += static_cast<char> (0xF0 | (cp >> 18));
      out += static_cast<char> (0x80 | ((cp >> 12) & 0x3F));
      out += static_cast<char> (0x80 | ((cp >> 6) & 0x3F));
      out += static_cast<char> (0x80 | (cp & 0x3F));
    }
  else
    return false;
  return true;
}

/// Wrapping 64-bit arithmetic (no UB; identical to D's two's complement).
inline std::int64_t
wrap_add (std::int64_t a, std::int64_t b)
{
  return static_cast<std::int64_t> (static_cast<std::uint64_t> (a)
                                    + static_cast<std::uint64_t> (b));
}
inline std::int64_t
wrap_sub (std::int64_t a, std::int64_t b)
{
  return static_cast<std::int64_t> (static_cast<std::uint64_t> (a)
                                    - static_cast<std::uint64_t> (b));
}
inline std::int64_t
wrap_mul (std::int64_t a, std::int64_t b)
{
  return static_cast<std::int64_t> (static_cast<std::uint64_t> (a)
                                    * static_cast<std::uint64_t> (b));
}
inline std::int64_t
wrap_neg (std::int64_t a)
{
  return static_cast<std::int64_t> (0 - static_cast<std::uint64_t> (a));
}

/// Saturating float -> int conversion (NaN -> 0).
inline std::int64_t
float_to_int (double d)
{
  if (std::isnan (d))
    return 0;
  if (d >= 9223372036854775807.0)
    return INT64_MAX;
  if (d <= -9223372036854775808.0)
    return INT64_MIN;
  return static_cast<std::int64_t> (d);
}

/// Parses an Int literal body (decimal, 0x, 0b, underscores, optional sign).
inline std::optional<std::int64_t>
parse_int (std::string_view s)
{
  bool neg = false;
  std::size_t i = 0;
  if (i < s.size () && (s[i] == '-' || s[i] == '+' || s[i] == '~'))
    {
      neg = s[i] != '+';
      ++i;
    }
  int base = 10;
  if (i + 1 < s.size () && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X'))
    {
      base = 16;
      i += 2;
    }
  else if (i + 1 < s.size () && s[i] == '0'
           && (s[i + 1] == 'b' || s[i + 1] == 'B'))
    {
      base = 2;
      i += 2;
    }
  if (i >= s.size ())
    return std::nullopt;
  std::uint64_t v = 0;
  std::uint64_t limit = neg ? 9223372036854775808ULL : 9223372036854775807ULL;
  bool any = false;
  for (; i < s.size (); ++i)
    {
      char c = s[i];
      if (c == '_' && any && i + 1 < s.size () && s[i - 1] != '_')
        {
          char next = s[i + 1];
          int d = next >= '0' && next <= '9' ? next - '0'
                  : next >= 'a' && next <= 'f' ? next - 'a' + 10
                  : next >= 'A' && next <= 'F' ? next - 'A' + 10 : base;
          if (d < base)
            continue;
        }
      int d;
      if (c >= '0' && c <= '9')
        d = c - '0';
      else if (c >= 'a' && c <= 'f')
        d = c - 'a' + 10;
      else if (c >= 'A' && c <= 'F')
        d = c - 'A' + 10;
      else
        return std::nullopt;
      if (d >= base)
        return std::nullopt;
      if (v > (limit - static_cast<std::uint64_t> (d))
                  / static_cast<std::uint64_t> (base))
        return std::nullopt;
      v = v * static_cast<std::uint64_t> (base) + static_cast<std::uint64_t> (d);
      any = true;
    }
  if (!any)
    return std::nullopt;
  if (neg)
    return static_cast<std::int64_t> (0 - v);
  return static_cast<std::int64_t> (v);
}

/// Strict float parse: whole string, no leading whitespace.
inline std::optional<double>
parse_float (std::string_view s)
{
  if (s.empty () || s[0] == ' ' || s[0] == '\t' || s[0] == '\n'
      || s[0] == '\r' || s[0] == '\v' || s[0] == '\f')
    return std::nullopt;
  std::string t;
  for (std::size_t i = 0; i < s.size (); ++i)
    if (s[i] != '_')
      t += s[i];
    else if (i == 0 || i + 1 == s.size () || s[i - 1] < '0' || s[i - 1] > '9'
             || s[i + 1] < '0' || s[i + 1] > '9')
      return std::nullopt;
  if (!t.empty () && t[0] == '~')
    t[0] = '-';
  char *end = nullptr;
  double d = std::strtod (t.c_str (), &end);
  if (end == t.c_str () || end != t.c_str () + t.size ())
    return std::nullopt;
  return d;
}

inline bool
is_lower (char c)
{
  return (c >= 'a' && c <= 'z') || c == '_';
}
inline bool
is_upper (char c)
{
  return c >= 'A' && c <= 'Z';
}
inline bool
is_digit (char c)
{
  return c >= '0' && c <= '9';
}
inline bool
is_ident_char (char c)
{
  return is_lower (c) || is_upper (c) || is_digit (c) || c == '\'';
}
inline bool
is_op_char (char c)
{
  switch (c)
    {
    case '!':
    case '$':
    case '%':
    case '&':
    case '*':
    case '+':
    case '-':
    case '.':
    case '/':
    case ':':
    case '<':
    case '=':
    case '>':
    case '?':
    case '@':
    case '^':
    case '|':
    case '~':
      return true;
    default:
      return false;
    }
}

inline std::string
join (const std::vector<std::string> &parts, std::string_view sep)
{
  std::string out;
  for (std::size_t i = 0; i < parts.size (); ++i)
    {
      if (i)
        out += sep;
      out += parts[i];
    }
  return out;
}

/// Thrown internally; never escapes the embedding API.
struct MetaError
{
  Diagnostic diag;
};

inline Diagnostic
make_diag (std::string code, std::string message, Span span,
           Severity sev = Severity::Error)
{
  Diagnostic d;
  d.severity = sev;
  d.code = std::move (code);
  d.message = std::move (message);
  d.primary_span = std::move (span);
  return d;
}

[[noreturn]] inline void
fail (std::string code, std::string message, Span span = {})
{
  throw MetaError{ make_diag (std::move (code), std::move (message),
                              std::move (span)) };
}

} // namespace detail

// ============================================================
//  Lexer (tokens are dsl::BasicToken; scanners use DSLtk combinators)
// ============================================================

enum class Tok
{
  Eof,
  Int,
  Float,
  Str,
  LIdent,
  UIdent,
  TyVar,
  Tacit,
  Hole,
  Directive,
  Op,
  Kw,
  LParen,
  RParen,
  LBracket,
  RBracket,
  LBrace,
  RBrace,
  Comma,
  Semi,
  Dot
};

/// A MetaML token: a DSLtk token plus decoded literal payloads.
struct Token : dsl::BasicToken<Tok>
{
  std::int64_t ival = 0;
  double fval = 0.0;
  std::string sval{};
};

namespace detail
{

inline bool
is_keyword (std::string_view s)
{
  static const char *kws[]
      = { "module", "type",  "sig",   "struct", "end",    "val",  "let",
          "rec",    "in",    "if",    "then",   "else",   "match", "with",
          "fn",     "fun",   "import", "include", "open", "do",   "true",
          "false",  "when",  "for",   "and",    "of" };
  for (const char *k : kws)
    if (s == k)
      return true;
  return false;
}

/// DSLtk combinator scanners; each returns the end offset of a match.
struct Scanners
{
  dsl::Parser<std::pair<char, std::vector<char>>> lower_ident;
  dsl::Parser<std::pair<char, std::vector<char>>> upper_ident;
  dsl::Parser<std::pair<char, std::vector<char>>> op_run;
  dsl::Parser<std::pair<std::pair<char, char>, std::pair<char, std::vector<char>>>>
      hex_lit;
  dsl::Parser<std::pair<std::pair<char, char>, std::pair<char, std::vector<char>>>>
      bin_lit;
  dsl::Parser<std::pair<char, std::vector<char>>> digits;
  dsl::Parser<std::pair<char, std::pair<char, std::vector<char>>>> fraction;
  dsl::Parser<std::pair<std::pair<char, std::optional<char>>,
                        std::pair<char, std::vector<char>>>>
      exponent;

  Scanners ()
  {
    using dsl::ch;
    using dsl::satisfy;
    auto ident_char = satisfy (is_ident_char, "identifier character");
    lower_ident = satisfy (is_lower, "lowercase identifier") & *ident_char;
    upper_ident = satisfy (is_upper, "uppercase identifier") & *ident_char;
    auto opc = satisfy (is_op_char, "operator character");
    op_run = opc & *opc;
    auto hexd = satisfy (
        [] (char c)
          {
            return is_digit (c) || (c >= 'a' && c <= 'f')
                   || (c >= 'A' && c <= 'F');
          },
        "hex digit");
    auto hexu = satisfy (
        [] (char c)
          {
            return is_digit (c) || (c >= 'a' && c <= 'f')
                   || (c >= 'A' && c <= 'F') || c == '_';
          },
        "hex digit");
    hex_lit = (ch ('0') & (ch ('x') | ch ('X'))) & (hexd & *hexu);
    auto bind = satisfy ([] (char c) { return c == '0' || c == '1'; },
                         "binary digit");
    auto binu = satisfy ([] (char c) { return c == '0' || c == '1' || c == '_'; },
                         "binary digit");
    bin_lit = (ch ('0') & (ch ('b') | ch ('B'))) & (bind & *binu);
    auto dig = satisfy (is_digit, "digit");
    auto digu = satisfy ([] (char c) { return is_digit (c) || c == '_'; },
                         "digit");
    digits = dig & *digu;
    fraction = dsl::try_parse (ch ('.') & (dig & *digu));
    exponent = dsl::try_parse ((((ch ('e') | ch ('E')))
                                & dsl::optional (ch ('+') | ch ('-')))
                               & (dig & *digu));
  }

  template <class T>
  static std::optional<std::size_t>
  scan (const dsl::Parser<T> &p, std::string_view src, std::size_t pos)
  {
    dsl::ParsecInput in{ src, pos };
    auto r = p (in);
    if (!r)
      return std::nullopt;
    return in.pos;
  }
};

inline const Scanners &
scanners ()
{
  static const Scanners s;
  return s;
}

inline bool
value_end (const Token &t)
{
  switch (t.kind)
    {
    case Tok::Int:
    case Tok::Float:
    case Tok::Str:
    case Tok::LIdent:
    case Tok::UIdent:
    case Tok::Tacit:
    case Tok::Hole:
    case Tok::RParen:
    case Tok::RBracket:
    case Tok::RBrace:
      return true;
    case Tok::Kw:
      return t.text == "true" || t.text == "false";
    default:
      return false;
    }
}

} // namespace detail

struct LexResult
{
  std::vector<Token> tokens;
  std::vector<Diagnostic> diagnostics;
  bool incomplete = false; ///< input ended inside a string or comment
};

/// Tokenizes MetaML source. Never throws.
inline LexResult
lex (std::string_view src, const std::string &file = "<input>")
{
  using namespace detail;
  LexResult res;
  std::size_t i = 0, line = 1, line_start = 0;
  bool space = false, newline = true;
  const auto &sc = scanners ();

  auto span_at = [&] (std::size_t b, std::size_t e)
    {
      Span s;
      s.begin = b;
      s.end = e;
      s.line = line;
      s.column = b - line_start + 1;
      s.file = file;
      return s;
    };
  auto error = [&] (std::string code, std::string msg, std::size_t b,
                    std::size_t e)
    { res.diagnostics.push_back (make_diag (code, msg, span_at (b, e))); };
  auto push = [&] (Tok k, std::size_t b, std::size_t e) -> Token &
    {
      Token t;
      t.kind = k;
      t.text = std::string (src.substr (b, e - b));
      t.span = span_at (b, e);
      t.trivia.space_before = space;
      t.trivia.newline_before = newline;
      res.tokens.push_back (std::move (t));
      space = false;
      newline = false;
      return res.tokens.back ();
    };
  auto advance_lines = [&] (std::size_t from, std::size_t to)
    {
      for (std::size_t k = from; k < to; ++k)
        if (src[k] == '\n')
          {
            ++line;
            line_start = k + 1;
          }
    };

  while (i < src.size ())
    {
      char c = src[i];
      if (c == '\n')
        {
          ++i;
          ++line;
          line_start = i;
          space = true;
          newline = true;
          continue;
        }
      if (c == ' ' || c == '\t' || c == '\r')
        {
          ++i;
          space = true;
          continue;
        }
      char n1 = i + 1 < src.size () ? src[i + 1] : '\0';
      // comments
      if (c == '-' && n1 == '-')
        {
          while (i < src.size () && src[i] != '\n')
            ++i;
          space = true;
          continue;
        }
      if (c == '{' && n1 == '-')
        {
          std::size_t start = i;
          std::size_t start_line = line, start_ls = line_start;
          int depth = 0;
          bool closed = false;
          while (i < src.size ())
            {
              if (src[i] == '{' && i + 1 < src.size () && src[i + 1] == '-')
                {
                  ++depth;
                  i += 2;
                  continue;
                }
              if (src[i] == '-' && i + 1 < src.size () && src[i + 1] == '}')
                {
                  --depth;
                  i += 2;
                  if (depth == 0)
                    {
                      closed = true;
                      break;
                    }
                  continue;
                }
              if (src[i] == '\n')
                {
                  ++line;
                  line_start = i + 1;
                  newline = true;
                }
              ++i;
            }
          space = true;
          if (!closed)
            {
              std::size_t save_line = line, save_ls = line_start;
              line = start_line;
              line_start = start_ls;
              error ("M0104", "unterminated block comment", start, start + 2);
              line = save_line;
              line_start = save_ls;
              res.incomplete = true;
            }
          continue;
        }
      if (c == '#')
        {
          if (is_digit (n1))
            {
              std::size_t b = i++;
              while (i < src.size () && is_digit (src[i]))
                ++i;
              auto &t = push (Tok::Hole, b, i);
              auto index = parse_int (src.substr (b + 1, i - b - 1));
              if (!index || *index > 1024)
                error ("M0105", "placeholder index exceeds the limit of 1024", b, i);
              t.ival = index && *index <= 1024 ? *index : 0;
              continue;
            }
          if (is_lower (n1) || is_upper (n1))
            {
              std::size_t b = i++;
              while (i < src.size () && is_ident_char (src[i]))
                ++i;
              std::size_t name_end = i;
              std::size_t e = i;
              while (e < src.size () && src[e] != '\n')
                ++e;
              auto &t = push (Tok::Directive, b, name_end);
              std::string arg (src.substr (name_end, e - name_end));
              while (!arg.empty () && (arg.front () == ' ' || arg.front () == '\t'))
                arg.erase (0, 1);
              while (!arg.empty ()
                     && (arg.back () == ' ' || arg.back () == '\t'
                         || arg.back () == '\r'))
                arg.pop_back ();
              t.sval = arg;
              t.text = std::string (src.substr (b + 1, name_end - b - 1));
              i = e;
              continue;
            }
          while (i < src.size () && src[i] != '\n')
            ++i;
          space = true;
          continue;
        }
      std::size_t b = i;
      // strings
      if (c == '"' || (c == 'r' && n1 == '"'))
        {
          bool raw = c == 'r';
          i += raw ? 2 : 1;
          std::string val;
          bool closed = false, bad = false;
          std::size_t lines_from = i;
          while (i < src.size ())
            {
              char d = src[i];
              if (d == '"')
                {
                  ++i;
                  closed = true;
                  break;
                }
              if (d == '\\' && !raw)
                {
                  if (i + 1 >= src.size ())
                    {
                      ++i;
                      break;
                    }
                  char e = src[i + 1];
                  i += 2;
                  switch (e)
                    {
                    case 'n':
                      val += '\n';
                      break;
                    case 't':
                      val += '\t';
                      break;
                    case 'r':
                      val += '\r';
                      break;
                    case '0':
                      val += '\0';
                      break;
                    case '"':
                      val += '"';
                      break;
                    case '\\':
                      val += '\\';
                      break;
                    case 'u':
                      {
                        std::uint32_t cp = 0;
                        bool ok = i < src.size () && src[i] == '{';
                        std::size_t j = i + 1;
                        int nd = 0;
                        while (ok && j < src.size () && src[j] != '}')
                          {
                            char h = src[j];
                            int dv = is_digit (h)               ? h - '0'
                                     : (h >= 'a' && h <= 'f') ? h - 'a' + 10
                                     : (h >= 'A' && h <= 'F') ? h - 'A' + 10
                                                               : -1;
                            if (dv < 0 || nd >= 6)
                              {
                                ok = false;
                                break;
                              }
                            cp = cp * 16 + static_cast<std::uint32_t> (dv);
                            ++nd;
                            ++j;
                          }
                        ok = ok && j < src.size () && nd > 0;
                        if (ok && append_utf8 (val, cp))
                          i = j + 1;
                        else
                          {
                            error ("M0103", "invalid unicode escape; expected `\\u{XXXX}`",
                                   i - 2, i);
                            bad = true;
                          }
                        break;
                      }
                    default:
                      error ("M0103",
                             std::string ("invalid escape sequence `\\") + e + "`",
                             i - 2, i);
                      bad = true;
                    }
                  continue;
                }
              val += d;
              ++i;
            }
          (void)bad;
          if (!closed)
            {
              error ("M0102", "unterminated string literal", b, b + 1);
              res.incomplete = true;
              advance_lines (lines_from, i);
              break;
            }
          auto &t = push (Tok::Str, b, i);
          t.sval = val;
          advance_lines (lines_from, i);
          continue;
        }
      // numbers (and negative literals directly after a non-value token)
      bool neg_lit = (c == '-' || c == '~') && is_digit (n1)
                     && (res.tokens.empty () || !value_end (res.tokens.back ())
                         || (c == '~'));
      if (is_digit (c) || neg_lit)
        {
          std::size_t start = neg_lit ? i + 1 : i;
          std::optional<std::size_t> e;
          bool is_float = false;
          bool special = false;
          if ((e = Scanners::scan (sc.hex_lit, src, start))
              || (e = Scanners::scan (sc.bin_lit, src, start)))
            special = true;
          else
            {
              e = Scanners::scan (sc.digits, src, start);
              std::size_t p = *e;
              if (auto f = Scanners::scan (sc.fraction, src, p))
                {
                  p = *f;
                  is_float = true;
                }
              if (auto x = Scanners::scan (sc.exponent, src, p))
                {
                  p = *x;
                  is_float = true;
                }
              e = p;
            }
          (void)special;
          i = *e;
          std::string body (src.substr (b, i - b));
          if (is_float)
            {
              auto v = parse_float (body);
              if (!v)
                error ("M0105", "invalid float literal `" + body + "`", b, i);
              auto &t = push (Tok::Float, b, i);
              t.fval = v ? *v : 0.0;
            }
          else
            {
              auto v = parse_int (body);
              if (!v)
                error ("M0105", "invalid or out-of-range integer literal `" + body + "`",
                       b, i);
              auto &t = push (Tok::Int, b, i);
              t.ival = v ? *v : 0;
            }
          if (i < src.size () && is_ident_char (src[i]))
            {
              std::size_t j = i;
              while (j < src.size () && is_ident_char (src[j]))
                ++j;
              error ("M0105",
                     "invalid numeric literal `" + std::string (src.substr (b, j - b))
                         + "`",
                     b, j);
              i = j;
            }
          continue;
        }
      if (c == '$' && is_digit (n1))
        {
          ++i;
          while (i < src.size () && is_digit (src[i]))
            ++i;
          auto &t = push (Tok::Tacit, b, i);
          auto index = parse_int (src.substr (b + 1, i - b - 1));
          if (!index || *index > 1024)
            error ("M0105", "placeholder index exceeds the limit of 1024", b, i);
          t.ival = index && *index <= 1024 ? *index : 0;
          continue;
        }
      if (c == '\'' && is_lower (n1))
        {
          ++i;
          while (i < src.size () && is_ident_char (src[i]))
            ++i;
          push (Tok::TyVar, b, i);
          continue;
        }
      if (is_lower (c))
        {
          i = *Scanners::scan (sc.lower_ident, src, i);
          auto text = src.substr (b, i - b);
          push (is_keyword (text) ? Tok::Kw : Tok::LIdent, b, i);
          continue;
        }
      if (is_upper (c))
        {
          i = *Scanners::scan (sc.upper_ident, src, i);
          push (Tok::UIdent, b, i);
          continue;
        }
      switch (c)
        {
        case '(':
          push (Tok::LParen, b, ++i);
          continue;
        case ')':
          push (Tok::RParen, b, ++i);
          continue;
        case '[':
          push (Tok::LBracket, b, ++i);
          continue;
        case ']':
          push (Tok::RBracket, b, ++i);
          continue;
        case '{':
          push (Tok::LBrace, b, ++i);
          continue;
        case '}':
          push (Tok::RBrace, b, ++i);
          continue;
        case ',':
          push (Tok::Comma, b, ++i);
          continue;
        case ';':
          push (Tok::Semi, b, ++i);
          continue;
        default:
          break;
        }
      if (c == '.' && n1 != '.')
        {
          push (Tok::Dot, b, ++i);
          continue;
        }
      // U+2218 RING OPERATOR (composition)
      if (static_cast<unsigned char> (c) == 0xE2 && i + 2 < src.size ()
          && static_cast<unsigned char> (src[i + 1]) == 0x88
          && static_cast<unsigned char> (src[i + 2]) == 0x98)
        {
          i += 3;
          push (Tok::Op, b, i);
          continue;
        }
      if (is_op_char (c))
        {
          std::size_t e = *Scanners::scan (sc.op_run, src, i);
          // stop before `$<digit>` (tacit ref) and before a `--` comment
          for (std::size_t k = i + 1; k < e; ++k)
            if ((src[k] == '$' && k + 1 < src.size () && is_digit (src[k + 1]))
                || (src[k] == '-' && k + 1 < e && src[k + 1] == '-'))
              {
                e = k;
                break;
              }
          i = e;
          push (Tok::Op, b, i);
          continue;
        }
      // unknown character (skip one UTF-8 sequence)
      std::size_t e = i + 1;
      while (e < src.size () && (static_cast<unsigned char> (src[e]) & 0xC0) == 0x80)
        ++e;
      error ("M0101",
             "unexpected character `" + std::string (src.substr (i, e - i)) + "`",
             i, e);
      i = e;
    }
  Token eof;
  eof.kind = Tok::Eof;
  eof.span = span_at (src.size (), src.size ());
  eof.trivia.newline_before = true;
  eof.trivia.space_before = true;
  res.tokens.push_back (eof);
  return res;
}

// ============================================================
//  AST (MetaML extensions over the DSLtk AST; dumped via dsl::ASTNode)
// ============================================================

struct Expr;
struct Pat;
struct TypeExpr;
struct Item;
struct ModExpr;
struct SigExpr;
struct Primitive;
struct Cell;
using ExprP = std::shared_ptr<Expr>;
using PatP = std::shared_ptr<Pat>;
using TypeExprP = std::shared_ptr<TypeExpr>;
using ItemP = std::shared_ptr<Item>;
using ModExprP = std::shared_ptr<ModExpr>;
using SigExprP = std::shared_ptr<SigExpr>;
using CellP = std::shared_ptr<Cell>;

/// Base of every MetaML AST node: a span (DSLtk) plus a node identity.
struct NodeBase
{
  Span span{};
};

enum class EK
{
  Int,
  Float,
  Str,
  Bool,
  Unit,
  Var,
  Path,
  Ctor,
  Tacit,
  App,
  Pipe,
  BinOp,
  Neg,
  Lambda,
  If,
  Match,
  Let,
  LetRec,
  List,
  Comp,
  Range,
  Tuple,
  Record,
  Update,
  Project,
  OpRef,
  Annot
};

/// How the type checker resolved a juxtaposition / pipeline (§5.4).
enum class Resolution
{
  Apply = 0,       ///< ordinary application
  ComposeFwd = 1,  ///< concatenative: left then right
  ComposeBwd = 2   ///< concatenative: right then left
};

struct CompClause
{
  PatP pat;   ///< generator pattern (null for a guard)
  ExprP expr; ///< generator source or guard condition
};

struct Expr : NodeBase
{
  EK kind = EK::Unit;
  std::int64_t ival = 0;
  double fval = 0.0;
  std::string text{};             ///< name / operator / literal / field
  std::vector<std::string> path{}; ///< qualified names (Path, Ctor qualifiers)
  std::vector<ExprP> kids{};
  std::vector<PatP> pats{};
  std::vector<ExprP> guards{};     ///< match guards, parallel to pats
  std::vector<std::string> labels{}; ///< record labels, parallel to kids
  std::vector<CompClause> clauses{};
  TypeExprP annot{};
  std::string name{};              ///< lambda: binding name (for #trace)
  int tacit_count = 0;             ///< lambda: binds $1..$n to its params
  // ---- annotations written by the type checker ----
  Resolution resolution = Resolution::Apply;
  CellP cell{};                    ///< Var: global binding cell
  Primitive *prim = nullptr;       ///< Path: host primitive
  int ctor_index = -1;             ///< Ctor
  int ctor_arity = 0;              ///< Ctor
};

enum class PK
{
  Wild,
  Var,
  Int,
  Float,
  Str,
  Bool,
  Unit,
  Tuple,
  List,
  Cons,
  Ctor,
  Record,
  Annot
};

struct Pat : NodeBase
{
  PK kind = PK::Wild;
  std::int64_t ival = 0;
  double fval = 0.0;
  std::string text{};
  std::vector<std::string> path{};
  std::vector<PatP> kids{};
  std::vector<std::string> labels{};
  bool open = false; ///< record pattern with `..`
  TypeExprP annot{};
  int ctor_index = -1;
  int ctor_arity = 0;
};

enum class TEK
{
  Var,
  Name,
  Fun,
  Tuple,
  List,
  Record,
  Unit
};

struct TypeExpr : NodeBase
{
  TEK kind = TEK::Unit;
  std::string name{};              ///< var or constructor name
  std::vector<std::string> path{}; ///< module qualifiers of Name
  std::vector<TypeExprP> kids{};   ///< args / fun parts / fields
  std::vector<std::string> labels{};
  std::string row{};               ///< record row variable (`| r`)
  std::vector<std::string> num_vars{}; ///< `Num 'a =>` constraints
};

enum class IK
{
  Sig,
  Def,
  Type,
  Module,
  Functor,
  ModType,
  Import,
  Include,
  Open,
  Expr,
  Directive
};

struct CtorDecl
{
  std::string name;
  std::vector<TypeExprP> args;
  Span span;
};

/// Ascription kind for module definitions.
enum class Asc
{
  None = 0,
  Transparent = 1, ///< `:`
  Opaque = 2       ///< `:>`
};

struct Item : NodeBase
{
  IK kind = IK::Expr;
  std::string name{};
  TypeExprP type{};               ///< Sig: type; Def: inline annotation; Type: alias
  std::vector<PatP> params{};     ///< Def params
  ExprP body{};                   ///< Def: raw body; Expr: the expression
  ExprP value{};                  ///< Def: elaborated value (lambda or body)
  bool tacit = false;             ///< Def written with $n
  std::vector<std::string> tparams{}; ///< Type params
  int tkind = 0;                  ///< Type: 0 abstract, 1 alias, 2 ADT
  std::vector<CtorDecl> ctors{};
  Asc asc = Asc::None;
  SigExprP sig{};                 ///< Module/Functor result sig, ModType body
  ModExprP mexpr{};               ///< Module body / Functor body
  std::string pname{};            ///< Functor param name
  SigExprP psig{};                ///< Functor param sig
  std::vector<std::string> path{}; ///< Include/Open path
  std::string text{};             ///< Import path / directive argument
  // ---- annotations written by the type checker ----
  bool sealed = false;
  std::vector<std::string> visible_values{};
  std::vector<std::string> visible_modules{};
  std::vector<CellP> cells{};     ///< Def / Open: cells defined at top level
};

enum class MK
{
  Struct,
  Path,
  Apply
};

struct ModExpr : NodeBase
{
  MK kind = MK::Struct;
  std::vector<ItemP> items{};
  std::vector<std::string> path{};
  ModExprP arg{};
};

struct SigExpr : NodeBase
{
  bool named = false;
  std::vector<ItemP> items{}; ///< Sig (val) and Type items
  std::vector<std::string> path{};
};

struct Program
{
  std::vector<ItemP> items;
  std::string file;
  std::string source;
};

// ---- AST dump (S-expressions through dsl::ASTNode / dsl::SExpr) ----

namespace detail
{

inline dsl::ASTNode
leafn (std::string_view tag, std::string v)
{
  return dsl::ASTNode (tag, std::move (v));
}
inline dsl::ASTNode
nodn (std::string_view tag, std::vector<dsl::ASTNode> kids)
{
  return dsl::ASTNode (tag, std::move (kids));
}

inline dsl::ASTNode dump_type (const TypeExprP &t);
inline dsl::ASTNode dump_pat (const PatP &p);
inline dsl::ASTNode dump_expr (const ExprP &e);
inline dsl::ASTNode dump_item (const ItemP &i);

inline dsl::ASTNode
dump_type (const TypeExprP &t)
{
  if (!t)
    return leafn ("none", "");
  std::vector<dsl::ASTNode> k;
  switch (t->kind)
    {
    case TEK::Var:
      return leafn ("tvar", t->name);
    case TEK::Unit:
      return leafn ("tunit", "");
    case TEK::Name:
      {
        std::vector<std::string> p = t->path;
        p.push_back (t->name);
        k.push_back (leafn ("name", join (p, ".")));
        for (auto &a : t->kids)
          k.push_back (dump_type (a));
        return nodn ("tcon", std::move (k));
      }
    case TEK::Fun:
      return nodn ("tfun", { dump_type (t->kids[0]), dump_type (t->kids[1]) });
    case TEK::Tuple:
      for (auto &a : t->kids)
        k.push_back (dump_type (a));
      return nodn ("ttuple", std::move (k));
    case TEK::List:
      return nodn ("tlist", { dump_type (t->kids[0]) });
    case TEK::Record:
      for (std::size_t i = 0; i < t->kids.size (); ++i)
        k.push_back (nodn ("tfield", { leafn ("label", t->labels[i]),
                                       dump_type (t->kids[i]) }));
      if (!t->row.empty ())
        k.push_back (leafn ("row", t->row));
      return nodn ("trecord", std::move (k));
    }
  return leafn ("none", "");
}

inline dsl::ASTNode
dump_pat (const PatP &p)
{
  std::vector<dsl::ASTNode> k;
  switch (p->kind)
    {
    case PK::Wild:
      return leafn ("pwild", "");
    case PK::Var:
      return leafn ("pvar", p->text);
    case PK::Int:
      return leafn ("pint", std::to_string (p->ival));
    case PK::Float:
      return leafn ("pfloat", format_float (p->fval));
    case PK::Str:
      return leafn ("pstr", p->text);
    case PK::Bool:
      return leafn ("pbool", p->ival ? "true" : "false");
    case PK::Unit:
      return leafn ("punit", "");
    case PK::Tuple:
      for (auto &x : p->kids)
        k.push_back (dump_pat (x));
      return nodn ("ptuple", std::move (k));
    case PK::List:
      for (auto &x : p->kids)
        k.push_back (dump_pat (x));
      return nodn ("plist", std::move (k));
    case PK::Cons:
      return nodn ("pcons", { dump_pat (p->kids[0]), dump_pat (p->kids[1]) });
    case PK::Ctor:
      {
        std::vector<std::string> q = p->path;
        q.push_back (p->text);
        k.push_back (leafn ("name", join (q, ".")));
        for (auto &x : p->kids)
          k.push_back (dump_pat (x));
        return nodn ("pctor", std::move (k));
      }
    case PK::Record:
      for (std::size_t i = 0; i < p->kids.size (); ++i)
        k.push_back (
            nodn ("pfield", { leafn ("label", p->labels[i]), dump_pat (p->kids[i]) }));
      if (p->open)
        k.push_back (leafn ("open", ""));
      return nodn ("precord", std::move (k));
    case PK::Annot:
      return nodn ("pannot", { dump_pat (p->kids[0]), dump_type (p->annot) });
    }
  return leafn ("pwild", "");
}

inline dsl::ASTNode
dump_expr (const ExprP &e)
{
  std::vector<dsl::ASTNode> k;
  switch (e->kind)
    {
    case EK::Int:
      return leafn ("int", std::to_string (e->ival));
    case EK::Float:
      return leafn ("float", format_float (e->fval));
    case EK::Str:
      return leafn ("str", e->text);
    case EK::Bool:
      return leafn ("bool", e->ival ? "true" : "false");
    case EK::Unit:
      return leafn ("unit", "");
    case EK::Var:
      return leafn ("var", e->text);
    case EK::Path:
      return leafn ("path", join (e->path, "."));
    case EK::Ctor:
      {
        std::vector<std::string> q = e->path;
        q.push_back (e->text);
        return leafn ("ctor", join (q, "."));
      }
    case EK::Tacit:
      return leafn ("tacit", std::to_string (e->ival));
    case EK::App:
      return nodn ("app", { dump_expr (e->kids[0]), dump_expr (e->kids[1]) });
    case EK::Pipe:
      return nodn ("pipe", { dump_expr (e->kids[0]), dump_expr (e->kids[1]) });
    case EK::BinOp:
      return nodn ("binop", { leafn ("op", e->text), dump_expr (e->kids[0]),
                              dump_expr (e->kids[1]) });
    case EK::Neg:
      return nodn ("neg", { dump_expr (e->kids[0]) });
    case EK::Lambda:
      {
        std::vector<dsl::ASTNode> ps;
        for (auto &p : e->pats)
          ps.push_back (dump_pat (p));
        k.push_back (nodn ("params", std::move (ps)));
        k.push_back (dump_expr (e->kids[0]));
        return nodn ("fn", std::move (k));
      }
    case EK::If:
      return nodn ("if", { dump_expr (e->kids[0]), dump_expr (e->kids[1]),
                           dump_expr (e->kids[2]) });
    case EK::Match:
      k.push_back (dump_expr (e->kids[0]));
      for (std::size_t i = 0; i < e->pats.size (); ++i)
        {
          std::vector<dsl::ASTNode> b{ dump_pat (e->pats[i]) };
          if (e->guards[i])
            b.push_back (nodn ("when", { dump_expr (e->guards[i]) }));
          b.push_back (dump_expr (e->kids[i + 1]));
          k.push_back (nodn ("branch", std::move (b)));
        }
      return nodn ("match", std::move (k));
    case EK::Let:
      return nodn ("let", { dump_pat (e->pats[0]), dump_expr (e->kids[0]),
                            dump_expr (e->kids[1]) });
    case EK::LetRec:
      for (std::size_t i = 0; i < e->pats.size (); ++i)
        k.push_back (nodn ("bind", { dump_pat (e->pats[i]), dump_expr (e->kids[i]) }));
      k.push_back (dump_expr (e->kids.back ()));
      return nodn ("letrec", std::move (k));
    case EK::List:
      for (auto &x : e->kids)
        k.push_back (dump_expr (x));
      return nodn ("list", std::move (k));
    case EK::Comp:
      k.push_back (dump_expr (e->kids[0]));
      for (auto &c : e->clauses)
        {
          if (c.pat)
            k.push_back (nodn ("gen", { dump_pat (c.pat), dump_expr (c.expr) }));
          else
            k.push_back (nodn ("guard", { dump_expr (c.expr) }));
        }
      return nodn ("comp", std::move (k));
    case EK::Range:
      return nodn ("range", { dump_expr (e->kids[0]), dump_expr (e->kids[1]) });
    case EK::Tuple:
      for (auto &x : e->kids)
        k.push_back (dump_expr (x));
      return nodn ("tuple", std::move (k));
    case EK::Record:
      for (std::size_t i = 0; i < e->kids.size (); ++i)
        k.push_back (
            nodn ("field", { leafn ("label", e->labels[i]), dump_expr (e->kids[i]) }));
      return nodn ("record", std::move (k));
    case EK::Update:
      k.push_back (dump_expr (e->kids[0]));
      for (std::size_t i = 0; i < e->labels.size (); ++i)
        k.push_back (nodn ("field", { leafn ("label", e->labels[i]),
                                      dump_expr (e->kids[i + 1]) }));
      return nodn ("update", std::move (k));
    case EK::Project:
      return nodn ("proj", { dump_expr (e->kids[0]), leafn ("label", e->text) });
    case EK::OpRef:
      return leafn ("opref", e->text);
    case EK::Annot:
      return nodn ("annot", { dump_expr (e->kids[0]), dump_type (e->annot) });
    }
  return leafn ("unit", "");
}

inline dsl::ASTNode dump_sig (const SigExprP &s);

inline dsl::ASTNode
dump_mexpr (const ModExprP &m)
{
  std::vector<dsl::ASTNode> k;
  switch (m->kind)
    {
    case MK::Struct:
      for (auto &i : m->items)
        k.push_back (dump_item (i));
      return nodn ("struct", std::move (k));
    case MK::Path:
      return leafn ("mpath", join (m->path, "."));
    case MK::Apply:
      return nodn ("mapply", { leafn ("functor", join (m->path, ".")),
                               dump_mexpr (m->arg) });
    }
  return leafn ("struct", "");
}

inline dsl::ASTNode
dump_sig (const SigExprP &s)
{
  if (!s)
    return leafn ("none", "");
  if (s->named)
    return leafn ("signame", join (s->path, "."));
  std::vector<dsl::ASTNode> k;
  for (auto &i : s->items)
    k.push_back (dump_item (i));
  return nodn ("sig", std::move (k));
}

inline std::string
asc_name (Asc a)
{
  return a == Asc::None ? "none" : a == Asc::Transparent ? ":" : ":>";
}

inline dsl::ASTNode
dump_item (const ItemP &it)
{
  std::vector<dsl::ASTNode> k;
  switch (it->kind)
    {
    case IK::Sig:
      return nodn ("Signature", { leafn ("name", it->name), dump_type (it->type) });
    case IK::Def:
      k.push_back (leafn ("name", it->name));
      if (it->type)
        k.push_back (dump_type (it->type));
      k.push_back (dump_expr (it->value));
      return nodn ("FnDef", std::move (k));
    case IK::Type:
      {
        k.push_back (leafn ("name", it->name));
        std::vector<dsl::ASTNode> ps;
        for (auto &p : it->tparams)
          ps.push_back (leafn ("tvar", p));
        k.push_back (nodn ("tparams", std::move (ps)));
        if (it->tkind == 1)
          k.push_back (dump_type (it->type));
        else if (it->tkind == 2)
          for (auto &c : it->ctors)
            {
              std::vector<dsl::ASTNode> cs{ leafn ("name", c.name) };
              for (auto &a : c.args)
                cs.push_back (dump_type (a));
              k.push_back (nodn ("ctor", std::move (cs)));
            }
        return nodn ("TypeDef", std::move (k));
      }
    case IK::Module:
      return nodn ("ModuleDef", { leafn ("name", it->name), leafn ("asc", asc_name (it->asc)),
                                  dump_sig (it->sig), dump_mexpr (it->mexpr) });
    case IK::Functor:
      return nodn ("FunctorDef",
                   { leafn ("name", it->name), leafn ("param", it->pname),
                     dump_sig (it->psig), leafn ("asc", asc_name (it->asc)),
                     dump_sig (it->sig), dump_mexpr (it->mexpr) });
    case IK::ModType:
      return nodn ("ModuleType", { leafn ("name", it->name), dump_sig (it->sig) });
    case IK::Import:
      return leafn ("Import", it->text);
    case IK::Include:
      return leafn ("Include", join (it->path, "."));
    case IK::Open:
      return leafn ("Open", join (it->path, "."));
    case IK::Expr:
      return nodn ("Expr", { dump_expr (it->body) });
    case IK::Directive:
      return nodn ("Directive", { leafn ("name", it->name), leafn ("arg", it->text) });
    }
  return leafn ("none", "");
}

} // namespace detail

/// Canonical S-expression dump of a program (identical in MetaML.d).
inline std::string
dump_ast (const Program &prog)
{
  std::vector<dsl::ASTNode> items;
  for (auto &i : prog.items)
    items.push_back (detail::dump_item (i));
  return dsl::ast_to_sexpr (dsl::ASTNode ("Program", std::move (items))).dump ();
}

// ============================================================
//  Parser (recursive descent over DSLtk tokens; §5.9 grammar)
// ============================================================

struct ParseOutput
{
  Program program;
  std::vector<Diagnostic> diagnostics;
  bool incomplete = false; ///< more input could complete the form (REPL)
  bool
  ok () const
  {
    return diagnostics.empty ();
  }
};

namespace detail
{

struct ParseFail
{
  Diagnostic diag;
  bool at_eof;
};

inline bool
is_reserved_op (std::string_view op)
{
  return op == ":=" || op == "->" || op == "<-" || op == "=>" || op == "|"
         || op == ":" || op == ".." || op == "~" || op == ":>" || op == ".";
}

/// Binary operator precedence (higher binds tighter); -1 if not binary.
inline int
op_prec (std::string_view op, bool &right)
{
  right = false;
  if (op.empty () || is_reserved_op (op))
    return -1;
  if (op == "|>")
    return 0;
  if (op == ">>")
    return 1;
  if (op == "<<" || op == "\xE2\x88\x98")
    {
      right = true;
      return 1;
    }
  if (op == "||")
    {
      right = true;
      return 2;
    }
  if (op == "&&")
    {
      right = true;
      return 3;
    }
  if (op == "==" || op == "!=" || op == "=" || op == "<>" || op == "<"
      || op == ">" || op == "<=" || op == ">=")
    return 4;
  if (op == "::")
    {
      right = true;
      return 5;
    }
  if (op == "@" || op == "^")
    {
      right = true;
      return 6;
    }
  if (op == "+" || op == "-")
    return 7;
  if (op == "*" || op == "/" || op == "%")
    return 8;
  switch (op[0])
    {
    case '*':
    case '/':
    case '%':
      return 8;
    case '+':
    case '-':
      return 7;
    case '@':
    case '^':
      right = true;
      return 6;
    case ':':
      right = true;
      return 5;
    default:
      return 4;
    }
}

inline bool
is_builtin_op (std::string_view op)
{
  static const char *ops[]
      = { "+",  "-",  "*",  "/",  "%",  "==", "!=", "=",  "<>", "<",
          ">",  "<=", ">=", "&&", "||", "^",  "@",  "::", ">>", "<<",
          "\xE2\x88\x98", "|>" };
  for (const char *o : ops)
    if (op == o)
      return true;
  return false;
}

class ParserImpl
{
public:
  ParserImpl (std::vector<Token> toks, std::string file)
      : toks_ (std::move (toks)), file_ (std::move (file))
  {
  }

  std::vector<ItemP>
  program ()
  {
    std::vector<ItemP> items;
    while (!at (Tok::Eof))
      items.push_back (item (false));
    return items;
  }

  ExprP
  expression_only ()
  {
    auto e = expr ();
    if (!at (Tok::Eof))
      error_here ("M0202", "unexpected " + describe (peek ()) + " after expression");
    check_no_tacit (e);
    return e;
  }

  TypeExprP
  type_only ()
  {
    auto t = type ();
    if (!at (Tok::Eof))
      error_here ("M0202", "unexpected " + describe (peek ()) + " after type");
    return t;
  }

private:
  std::vector<Token> toks_;
  std::size_t p_ = 0;
  std::string file_;
  int depth_ = 0;
  std::vector<int> holes_{};
  int sect_counter_ = 0;

  // ---- token helpers ----
  const Token &
  peek (std::size_t k = 0) const
  {
    std::size_t i = p_ + k;
    return i < toks_.size () ? toks_[i] : toks_.back ();
  }
  const Token &
  prev () const
  {
    return toks_[p_ > 0 ? p_ - 1 : 0];
  }
  bool
  at (Tok k) const
  {
    return peek ().kind == k;
  }
  bool
  at_op (std::string_view op, std::size_t k = 0) const
  {
    return peek (k).kind == Tok::Op && peek (k).text == op;
  }
  bool
  at_kw (std::string_view kw, std::size_t k = 0) const
  {
    return peek (k).kind == Tok::Kw && peek (k).text == kw;
  }
  const Token &
  next ()
  {
    const Token &t = toks_[p_];
    if (p_ + 1 < toks_.size ())
      ++p_;
    return t;
  }
  static std::string
  describe (const Token &t)
  {
    switch (t.kind)
      {
      case Tok::Eof:
        return "end of input";
      case Tok::Str:
        return "string literal";
      case Tok::Int:
      case Tok::Float:
        return "number `" + t.text + "`";
      case Tok::Directive:
        return "directive `#" + t.text + "`";
      case Tok::Kw:
        return "keyword `" + t.text + "`";
      default:
        return "`" + t.text + "`";
      }
  }
  [[noreturn]] void
  error_at (const Token &t, std::string code, std::string msg)
  {
    throw ParseFail{ make_diag (std::move (code), std::move (msg), t.span),
                     t.kind == Tok::Eof };
  }
  [[noreturn]] void
  error_here (std::string code, std::string msg)
  {
    error_at (peek (), std::move (code), std::move (msg));
  }
  [[noreturn]] void
  unexpected (std::string_view wanted)
  {
    if (at (Tok::Directive))
      error_here ("M0400", "directive `#" + peek ().text
                               + "` is only legal at the start of a top-level form");
    error_here ("M0202", "expected " + std::string (wanted) + " but found "
                             + describe (peek ()));
  }
  void
  expect (Tok k, std::string_view what)
  {
    if (!at (k))
      unexpected (what);
    next ();
  }
  void
  expect_op (std::string_view op)
  {
    if (!at_op (op))
      unexpected ("`" + std::string (op) + "`");
    next ();
  }
  void
  expect_kw (std::string_view kw)
  {
    if (!at_kw (kw))
      unexpected ("`" + std::string (kw) + "`");
    next ();
  }
  Span
  span_from (const Span &start) const
  {
    Span s = start;
    const Token &last = prev ();
    if (last.span.end > s.end)
      s.end = last.span.end;
    return s;
  }
  template <class T>
  std::shared_ptr<T>
  mk (const Span &s)
  {
    auto n = std::make_shared<T> ();
    n->span = s;
    return n;
  }
  ExprP
  mk_expr (EK k, const Span &s)
  {
    auto e = mk<Expr> (s);
    e->kind = k;
    return e;
  }
  ExprP
  mk_var (const std::string &name, const Span &s)
  {
    auto e = mk_expr (EK::Var, s);
    e->text = name;
    return e;
  }
  PatP
  mk_pvar (const std::string &name, const Span &s)
  {
    auto p = mk<Pat> (s);
    p->kind = PK::Var;
    p->text = name;
    return p;
  }
  ExprP
  mk_app (ExprP f, ExprP x)
  {
    auto e = mk_expr (EK::App, merge_span (f->span, x->span));
    e->kids = { std::move (f), std::move (x) };
    return e;
  }
  static Span
  merge_span (const Span &a, const Span &b)
  {
    return dsl::merge_spans (a, b);
  }
  ExprP
  mk_binop (const std::string &op, ExprP a, ExprP b)
  {
    auto e = mk_expr (op == "|>" ? EK::Pipe : EK::BinOp, merge_span (a->span, b->span));
    if (op != "|>")
      e->text = op;
    e->kids = { std::move (a), std::move (b) };
    return e;
  }
  ExprP
  mk_lambda (std::vector<PatP> params, ExprP body, const Span &s)
  {
    auto e = mk_expr (EK::Lambda, s);
    e->pats = std::move (params);
    e->kids = { std::move (body) };
    return e;
  }

  // ---- boundaries ----
  bool
  starts_atom (const Token &t) const
  {
    switch (t.kind)
      {
      case Tok::Int:
      case Tok::Float:
      case Tok::Str:
      case Tok::LIdent:
      case Tok::UIdent:
      case Tok::Tacit:
      case Tok::Hole:
      case Tok::LParen:
      case Tok::LBracket:
      case Tok::LBrace:
      case Tok::Directive:
        return true;
      case Tok::Kw:
        // keyword-led forms (fn, if, match, let) need parentheses as arguments
        return t.text == "true" || t.text == "false" || t.text == "do";
      default:
        return false;
      }
  }
  /// Scans a definition head starting at token index i: returns
  /// 1 for `name ::`, 2 for `name params (:=|=)`, 0 otherwise.
  int
  def_head_at (std::size_t i, bool allow_eq) const
  {
    if (i >= toks_.size () || toks_[i].kind != Tok::LIdent)
      return 0;
    std::size_t j = i + 1;
    if (j < toks_.size () && toks_[j].kind == Tok::Op && toks_[j].text == "::")
      return 1;
    int d = 0;
    for (std::size_t n = 0; j < toks_.size () && n < 256; ++j, ++n)
      {
        const Token &t = toks_[j];
        if (d == 0 && t.kind == Tok::Op)
          {
            if (t.text == ":=")
              return 2;
            if (t.text == "=" && allow_eq)
              return 2;
            return 0;
          }
        switch (t.kind)
          {
          case Tok::LParen:
          case Tok::LBracket:
          case Tok::LBrace:
            ++d;
            continue;
          case Tok::RParen:
          case Tok::RBracket:
          case Tok::RBrace:
            if (--d < 0)
              return 0;
            continue;
          case Tok::Eof:
            return 0;
          case Tok::Kw:
            if (d == 0)
              return 0;
            continue;
          case Tok::LIdent:
          case Tok::UIdent:
          case Tok::Int:
          case Tok::Float:
          case Tok::Str:
          case Tok::TyVar:
          case Tok::Comma:
            continue;
          case Tok::Op:
            if (d > 0)
              continue;
            return 0;
          default:
            if (d > 0)
              continue;
            return 0;
          }
      }
    return 0;
  }
  /// True when the current token begins a new top-level form on a new line.
  bool
  at_boundary () const
  {
    const Token &t = peek ();
    if (depth_ > 0 || !t.trivia.newline_before)
      return false;
    // a line that starts in column 1 begins a new top-level form
    if (t.kind == Tok::Directive || t.span.column == 1)
      return true;
    return def_head_at (p_, false) != 0;
  }

  // ---- items ----
  ItemP
  item (bool in_struct)
  {
    const Token &t = peek ();
    Span start = t.span;
    if (t.kind == Tok::Directive)
      {
        next ();
        auto it = mk<Item> (start);
        it->kind = IK::Directive;
        it->name = t.text;
        it->text = t.sval;
        return it;
      }
    if (t.kind == Tok::Kw)
      {
        if (t.text == "module")
          return module_item ();
        if (t.text == "type")
          return type_decl (false);
        if (t.text == "val")
          return val_item (false);
        if (t.text == "import")
          {
            next ();
            if (!at (Tok::Str))
              unexpected ("a string path after `import`");
            auto it = mk<Item> (start);
            it->kind = IK::Import;
            it->text = next ().sval;
            it->span = span_from (start);
            return it;
          }
        if (t.text == "include" || t.text == "open")
          {
            next ();
            auto it = mk<Item> (start);
            it->kind = t.text == "include" ? IK::Include : IK::Open;
            it->path = module_path ();
            it->span = span_from (start);
            return it;
          }
        if (t.text == "let")
          return let_item ();
        if (t.text == "end" && !in_struct)
          error_here ("M0202", "unexpected `end` outside of a `struct` or `sig`");
      }
    if (t.kind == Tok::LIdent)
      {
        int h = def_head_at (p_, true);
        if (h == 1)
          return sig_item ();
        if (h == 2)
          return def_item ();
      }
    auto it = mk<Item> (start);
    it->kind = IK::Expr;
    it->body = expr ();
    check_no_tacit (it->body);
    it->span = span_from (start);
    return it;
  }

  std::vector<std::string>
  module_path ()
  {
    std::vector<std::string> path;
    if (!at (Tok::UIdent))
      unexpected ("a module name");
    path.push_back (next ().text);
    while (at (Tok::Dot) && peek (1).kind == Tok::UIdent)
      {
        next ();
        path.push_back (next ().text);
      }
    return path;
  }

  ItemP
  sig_item ()
  {
    Span start = peek ().span;
    auto it = mk<Item> (start);
    it->kind = IK::Sig;
    it->name = next ().text;
    expect_op ("::");
    it->type = type ();
    it->span = span_from (start);
    return it;
  }

  std::string
  binder_name ()
  {
    if (at (Tok::LIdent))
      return next ().text;
    if (at (Tok::LParen) && peek (1).kind == Tok::Op && peek (2).kind == Tok::RParen)
      {
        next ();
        std::string op = next ().text;
        next ();
        return op;
      }
    unexpected ("a value name");
  }

  ItemP
  def_item ()
  {
    Span start = peek ().span;
    std::string name = binder_name ();
    std::vector<PatP> params;
    while (!at_op (":=") && !at_op ("="))
      params.push_back (patom ());
    next ();
    auto body = expr ();
    return make_def (name, std::move (params), nullptr, body, span_from (start));
  }

  ItemP
  val_item (bool in_sig)
  {
    Span start = peek ().span;
    expect_kw ("val");
    std::string name = binder_name ();
    if (at_op (":"))
      {
        next ();
        auto ty = type ();
        if (!in_sig && (at_op (":=") || at_op ("=")))
          {
            next ();
            auto body = expr ();
            return make_def (name, {}, ty, body, span_from (start));
          }
        auto it = mk<Item> (start);
        it->kind = IK::Sig;
        it->name = name;
        it->type = ty;
        it->span = span_from (start);
        return it;
      }
    if (in_sig)
      unexpected ("`:` and a type in a signature");
    std::vector<PatP> params;
    while (!at_op (":=") && !at_op ("="))
      {
        if (at (Tok::Eof))
          unexpected ("`:=`");
        params.push_back (patom ());
      }
    next ();
    auto body = expr ();
    return make_def (name, std::move (params), nullptr, body, span_from (start));
  }

  ItemP
  let_item ()
  {
    // top-level `let x = e` is a definition; `let ... in ...` an expression
    Span start = peek ().span;
    std::size_t save = p_;
    next ();
    bool rec = false;
    if (at_kw ("rec"))
      {
        rec = true;
        next ();
      }
    if (at (Tok::LIdent))
      {
        std::size_t q = p_;
        std::string name = next ().text;
        std::vector<PatP> params;
        bool ok = true;
        while (!at_op ("=") && !at_op (":="))
          {
            if (at (Tok::Eof) || at_kw ("in"))
              {
                ok = false;
                break;
              }
            params.push_back (patom ());
          }
        if (ok)
          {
            next ();
            auto body = expr ();
            if (!at_kw ("in") && !at_kw ("and"))
              return make_def (name, std::move (params), nullptr, body,
                               span_from (start));
          }
        (void)q;
      }
    (void)rec;
    p_ = save;
    auto it = mk<Item> (start);
    it->kind = IK::Expr;
    it->body = expr ();
    check_no_tacit (it->body);
    it->span = span_from (start);
    return it;
  }

  ItemP
  make_def (const std::string &name, std::vector<PatP> params, TypeExprP annot,
            ExprP body, const Span &span)
  {
    auto it = mk<Item> (span);
    it->kind = IK::Def;
    it->name = name;
    it->type = annot;
    it->body = body;
    int tmax = max_tacit (body);
    std::set<std::string> names;
    for (auto &pp : params)
      pat_names (pp, names);
    if (tmax > 0)
      {
        bool named_used = false;
        if (!params.empty ())
          {
            std::set<std::string> fv;
            free_vars (body, {}, fv);
            for (auto &n : names)
              if (fv.count (n))
                named_used = true;
          }
        if (named_used || (!params.empty () && tmax > static_cast<int> (params.size ())))
          {
            Diagnostic d = make_diag (
                "M0201",
                "mixed parameter styles: `" + name
                    + "` uses both named parameters and `$n` placeholders",
                span);
            d.suggestions.push_back (
                "use either named parameters or `$1`, `$2`, ... but not both");
            throw ParseFail{ d, false };
          }
        it->tacit = true;
        std::vector<PatP> ps = params;
        if (ps.empty ())
          for (int k = 1; k <= tmax; ++k)
            ps.push_back (mk_pvar ("$" + std::to_string (k), body->span));
        auto lam = mk_lambda (std::move (ps), body, span);
        lam->tacit_count = tmax;
        lam->name = name;
        it->params = std::move (params);
        it->value = lam;
        return it;
      }
    it->params = params;
    if (params.empty ())
      {
        it->value = body;
        if (body->kind == EK::Lambda && body->name.empty ())
          body->name = name;
      }
    else
      {
        auto lam = mk_lambda (std::move (params), body, span);
        lam->name = name;
        it->value = lam;
      }
    return it;
  }

  ItemP
  type_decl (bool in_sig)
  {
    Span start = peek ().span;
    expect_kw ("type");
    auto it = mk<Item> (start);
    it->kind = IK::Type;
    if (!at (Tok::UIdent) && !at (Tok::LIdent))
      unexpected ("a type name");
    it->name = next ().text;
    while ((at (Tok::TyVar) || at (Tok::LIdent)) && !peek ().trivia.newline_before)
      it->tparams.push_back (next ().text);
    if (at_op ("=") || at_op (":="))
      {
        next ();
        if (at_op ("|") || adt_ahead ())
          {
            it->tkind = 2;
            if (at_op ("|"))
              next ();
            it->ctors.push_back (ctor_decl ());
            while (at_op ("|"))
              {
                next ();
                it->ctors.push_back (ctor_decl ());
              }
          }
        else
          {
            it->tkind = 1;
            it->type = type ();
          }
      }
    (void)in_sig;
    it->span = span_from (start);
    return it;
  }

  bool
  adt_ahead () const
  {
    if (!at (Tok::UIdent))
      return false;
    int d = 0;
    for (std::size_t j = p_; j < toks_.size (); ++j)
      {
        const Token &t = toks_[j];
        if (j > p_ && d == 0 && t.trivia.newline_before
            && (t.kind == Tok::Kw || t.kind == Tok::Directive || t.kind == Tok::Eof
                || def_head_at (j, true)))
          return false;
        if (t.kind == Tok::Eof)
          return false;
        if (t.kind == Tok::LParen || t.kind == Tok::LBracket || t.kind == Tok::LBrace)
          ++d;
        else if (t.kind == Tok::RParen || t.kind == Tok::RBracket
                 || t.kind == Tok::RBrace)
          {
            if (--d < 0)
              return false;
          }
        else if (d == 0 && t.kind == Tok::Op && t.text == "|")
          return true;
        else if (d == 0 && t.kind == Tok::Op && t.text == "->")
          return false;
      }
    return false;
  }

  CtorDecl
  ctor_decl ()
  {
    CtorDecl c;
    c.span = peek ().span;
    if (!at (Tok::UIdent))
      unexpected ("a constructor name");
    c.name = next ().text;
    if (at_kw ("of"))
      {
        next ();
        c.args.push_back (type ());
        return c;
      }
    while (starts_tatom (peek ()) && !peek ().trivia.newline_before)
      c.args.push_back (tatom ());
    return c;
  }

  SigExprP
  sig_expr ()
  {
    Span start = peek ().span;
    auto s = mk<SigExpr> (start);
    if (at_kw ("sig"))
      {
        next ();
        while (!at_kw ("end"))
          {
            if (at (Tok::Eof))
              unexpected ("`end` closing `sig`");
            if (at_kw ("type"))
              s->items.push_back (type_decl (true));
            else if (at_kw ("val"))
              s->items.push_back (val_item (true));
            else if (at (Tok::LIdent) && peek (1).kind == Tok::Op && peek (1).text == "::")
              s->items.push_back (sig_item ());
            else
              unexpected ("`type`, `val` or `end` in a signature");
          }
        next ();
        s->span = span_from (start);
        return s;
      }
    if (at (Tok::UIdent))
      {
        s->named = true;
        s->path = module_path ();
        s->span = span_from (start);
        return s;
      }
    unexpected ("`sig` or a signature name");
  }

  ModExprP
  mod_expr ()
  {
    Span start = peek ().span;
    auto m = mk<ModExpr> (start);
    if (at_kw ("struct"))
      {
        next ();
        m->kind = MK::Struct;
        while (!at_kw ("end"))
          {
            if (at (Tok::Eof))
              unexpected ("`end` closing `struct`");
            m->items.push_back (item (true));
          }
        next ();
        m->span = span_from (start);
        return m;
      }
    if (at (Tok::UIdent))
      {
        m->path = module_path ();
        if (at (Tok::LParen))
          {
            next ();
            m->kind = MK::Apply;
            m->arg = mod_expr ();
            expect (Tok::RParen, "`)`");
          }
        else
          m->kind = MK::Path;
        m->span = span_from (start);
        return m;
      }
    unexpected ("`struct` or a module path");
  }

  ItemP
  module_item ()
  {
    Span start = peek ().span;
    expect_kw ("module");
    auto it = mk<Item> (start);
    if (at_kw ("type"))
      {
        next ();
        it->kind = IK::ModType;
        if (!at (Tok::UIdent))
          unexpected ("a signature name");
        it->name = next ().text;
        if (!at_op (":=") && !at_op ("="))
          unexpected ("`:=`");
        next ();
        it->sig = sig_expr ();
        it->span = span_from (start);
        return it;
      }
    if (!at (Tok::UIdent))
      unexpected ("a module name");
    it->name = next ().text;
    it->kind = IK::Module;
    if (at (Tok::LParen))
      {
        next ();
        it->kind = IK::Functor;
        if (!at (Tok::UIdent))
          unexpected ("a functor parameter name");
        it->pname = next ().text;
        expect_op (":");
        it->psig = sig_expr ();
        expect (Tok::RParen, "`)`");
      }
    if (at_op (":") || at_op (":>"))
      {
        it->asc = next ().text == ":" ? Asc::Transparent : Asc::Opaque;
        it->sig = sig_expr ();
      }
    if (!at_op (":=") && !at_op ("="))
      unexpected ("`:=`");
    next ();
    it->mexpr = mod_expr ();
    if (it->kind == IK::Functor && it->mexpr->kind != MK::Struct)
      error_at (prev (), "M0202", "a functor body must be `struct ... end`");
    it->span = span_from (start);
    return it;
  }

  // ---- expressions ----
  ExprP
  expr ()
  {
    return binary (0);
  }

  ExprP
  binary (int min_prec)
  {
    auto lhs = prefix ();
    while (true)
      {
        const Token &t = peek ();
        if (t.kind != Tok::Op || at_boundary ())
          break;
        if (peek (1).kind == Tok::RParen)
          break; // `(e op)` is a left section
        bool right = false;
        int prec = op_prec (t.text, right);
        if (prec < 0 || prec < min_prec)
          break;
        std::string op = next ().text;
        if (op == "|>")
          {
            ExprP rhs;
            const Token &u = peek ();
            bool r2 = false;
            if (u.kind == Tok::Op && op_prec (u.text, r2) >= 0)
              rhs = bare_section ();
            else
              rhs = binary (1);
            lhs = mk_binop (op, lhs, rhs);
            continue;
          }
        auto rhs = binary (right ? prec : prec + 1);
        lhs = mk_binop (op, lhs, rhs);
      }
    return lhs;
  }

  /// `|> + 1` — a right section without parentheses.
  ExprP
  bare_section ()
  {
    const Token &t = next ();
    std::string op = t.text;
    bool right = false;
    int prec = op_prec (op, right);
    auto operand = binary (prec + 1);
    std::string pn = "#0";
    auto body = mk_binop (op, mk_var (pn, t.span), operand);
    std::vector<PatP> ps{ mk_pvar (pn, t.span) };
    return mk_lambda (std::move (ps), body, span_from (t.span));
  }

  ExprP
  prefix ()
  {
    if (at_op ("-") || at_op ("~"))
      {
        Span s = next ().span;
        auto operand = app ();
        auto e = mk_expr (EK::Neg, merge_span (s, operand->span));
        e->kids = { operand };
        return e;
      }
    return app ();
  }

  ExprP
  app ()
  {
    bool head_greedy = at_kw ("fn") || at_kw ("fun") || at_kw ("if")
                       || at_kw ("match") || at_kw ("let");
    auto f = postfix ();
    if (head_greedy)
      return f;
    while (starts_atom (peek ()) && !at_boundary ())
      {
        auto arg = postfix ();
        f = mk_app (f, arg);
      }
    return f;
  }

  ExprP
  postfix ()
  {
    auto a = atom ();
    while (true)
      {
        if (at (Tok::Dot) && peek (1).kind == Tok::LIdent)
          {
            next ();
            const Token &f = next ();
            auto e = mk_expr (EK::Project, merge_span (a->span, f.span));
            e->kids = { a };
            e->text = f.text;
            a = e;
            continue;
          }
        if (at (Tok::LParen) && !peek ().trivia.space_before && value_end (prev ()))
          {
            Span ps = next ().span;
            ++depth_;
            if (at (Tok::RParen))
              {
                next ();
                --depth_;
                auto u = mk_expr (EK::Unit, span_from (ps));
                a = mk_app (a, u);
                continue;
              }
            std::vector<ExprP> args{ expr () };
            while (at (Tok::Comma))
              {
                next ();
                args.push_back (expr ());
              }
            expect (Tok::RParen, "`)` closing the argument list");
            --depth_;
            for (auto &x : args)
              {
                a = mk_app (a, x);
                a->span = span_from (a->kids[0]->span);
              }
            continue;
          }
        break;
      }
    return a;
  }

  ExprP
  atom ()
  {
    const Token &t = peek ();
    Span s = t.span;
    switch (t.kind)
      {
      case Tok::Int:
        {
          next ();
          auto e = mk_expr (EK::Int, s);
          e->ival = t.ival;
          return e;
        }
      case Tok::Float:
        {
          next ();
          auto e = mk_expr (EK::Float, s);
          e->fval = t.fval;
          return e;
        }
      case Tok::Str:
        {
          next ();
          auto e = mk_expr (EK::Str, s);
          e->text = t.sval;
          return e;
        }
      case Tok::LIdent:
        next ();
        return mk_var (t.text, s);
      case Tok::UIdent:
        return upper_path ();
      case Tok::Tacit:
        {
          next ();
          if (t.ival < 1)
            error_at (t, "M0201", "tacit parameters are numbered from `$1`");
          auto e = mk_expr (EK::Tacit, s);
          e->ival = t.ival;
          return e;
        }
      case Tok::Hole:
        {
          next ();
          if (holes_.empty ())
            error_at (t, "M0207",
                      "section hole `#" + std::to_string (t.ival)
                          + "` is only legal inside parentheses");
          if (t.ival < 1)
            error_at (t, "M0207", "section holes are numbered from `#1`");
          if (t.ival > holes_.back ())
            holes_.back () = static_cast<int> (t.ival);
          return mk_var ("#" + std::to_string (t.ival), s);
        }
      case Tok::LParen:
        return paren ();
      case Tok::LBracket:
        return list ();
      case Tok::LBrace:
        return record ();
      case Tok::Kw:
        if (t.text == "true" || t.text == "false")
          {
            next ();
            auto e = mk_expr (EK::Bool, s);
            e->ival = t.text == "true";
            return e;
          }
        if (t.text == "fn" || t.text == "fun")
          {
            next ();
            std::vector<PatP> ps;
            while (!at_op ("->"))
              {
                if (at (Tok::Eof))
                  unexpected ("`->`");
                ps.push_back (patom ());
              }
            if (ps.empty ())
              unexpected ("a parameter");
            next ();
            auto body = expr ();
            return mk_lambda (std::move (ps), body, span_from (s));
          }
        if (t.text == "if")
          {
            next ();
            auto c = expr ();
            expect_kw ("then");
            auto a = expr ();
            expect_kw ("else");
            auto b = expr ();
            auto e = mk_expr (EK::If, span_from (s));
            e->kids = { c, a, b };
            return e;
          }
        if (t.text == "match")
          return match_expr ();
        if (t.text == "let")
          return let_expr ();
        if (t.text == "do")
          return do_block ();
        break;
      default:
        break;
      }
    unexpected ("an expression");
  }

  ExprP
  upper_path ()
  {
    Span s = peek ().span;
    std::vector<std::string> segs{ next ().text };
    while (at (Tok::Dot))
      {
        if (peek (1).kind == Tok::UIdent)
          {
            next ();
            segs.push_back (next ().text);
            continue;
          }
        if (peek (1).kind == Tok::LIdent)
          {
            next ();
            segs.push_back (next ().text);
            auto e = mk_expr (EK::Path, span_from (s));
            e->path = segs;
            return e;
          }
        if (peek (1).kind == Tok::LParen && peek (2).kind == Tok::Op
            && peek (3).kind == Tok::RParen)
          {
            next ();
            next ();
            segs.push_back (next ().text);
            next ();
            auto e = mk_expr (EK::Path, span_from (s));
            e->path = segs;
            return e;
          }
        break;
      }
    auto e = mk_expr (EK::Ctor, span_from (s));
    e->text = segs.back ();
    segs.pop_back ();
    e->path = segs;
    return e;
  }

  ExprP
  paren ()
  {
    Span s = next ().span;
    ++depth_;
    if (at (Tok::RParen))
      {
        next ();
        --depth_;
        return mk_expr (EK::Unit, span_from (s));
      }
    bool r = false;
    if (at (Tok::Op) && op_prec (peek ().text, r) >= 0)
      {
        const Token &opt = next ();
        std::string op = opt.text;
        if (at (Tok::RParen))
          {
            next ();
            --depth_;
            auto e = mk_expr (EK::OpRef, span_from (s));
            e->text = op;
            return e;
          }
        // right section: (op e)  ==  fn x -> x op e
        holes_.push_back (0);
        auto operand = expr ();
        int nh = holes_.back ();
        holes_.pop_back ();
        expect (Tok::RParen, "`)` closing the section");
        --depth_;
        std::string pn = "#0";
        auto body = mk_binop (op, mk_var (pn, opt.span), operand);
        std::vector<PatP> ps{ mk_pvar (pn, opt.span) };
        auto lam = mk_lambda (std::move (ps), body, span_from (s));
        return wrap_holes (lam, nh, s);
      }
    holes_.push_back (0);
    auto e = expr ();
    if (at (Tok::Op) && op_prec (peek ().text, r) >= 0 && peek (1).kind == Tok::RParen)
      {
        // left section: (e op)  ==  fn x -> e op x
        const Token &opt = next ();
        next ();
        --depth_;
        int nh = holes_.back ();
        holes_.pop_back ();
        std::string pn = "#0";
        auto body = mk_binop (opt.text, e, mk_var (pn, opt.span));
        std::vector<PatP> ps{ mk_pvar (pn, opt.span) };
        return wrap_holes (mk_lambda (std::move (ps), body, span_from (s)), nh, s);
      }
    if (at (Tok::Comma))
      {
        auto tup = mk_expr (EK::Tuple, s);
        tup->kids.push_back (e);
        while (at (Tok::Comma))
          {
            next ();
            tup->kids.push_back (expr ());
          }
        e = tup;
      }
    else if (at_op (":"))
      {
        next ();
        auto an = mk_expr (EK::Annot, s);
        an->kids = { e };
        an->annot = type ();
        e = an;
      }
    expect (Tok::RParen, "`)`");
    --depth_;
    int nh = holes_.back ();
    holes_.pop_back ();
    e->span = span_from (s);
    return wrap_holes (e, nh, s);
  }

  ExprP
  wrap_holes (ExprP e, int n, const Span &s)
  {
    if (n == 0)
      return e;
    std::vector<PatP> ps;
    for (int k = 1; k <= n; ++k)
      ps.push_back (mk_pvar ("#" + std::to_string (k), s));
    return mk_lambda (std::move (ps), e, span_from (s));
  }

  ExprP
  list ()
  {
    Span s = next ().span;
    ++depth_;
    if (at (Tok::RBracket))
      {
        next ();
        --depth_;
        return mk_expr (EK::List, span_from (s));
      }
    auto first = expr ();
    if (at_op (".."))
      {
        next ();
        auto last = expr ();
        expect (Tok::RBracket, "`]`");
        --depth_;
        auto e = mk_expr (EK::Range, span_from (s));
        e->kids = { first, last };
        return e;
      }
    if (at_kw ("for"))
      {
        next ();
        auto e = mk_expr (EK::Comp, s);
        e->kids = { first };
        while (true)
          {
            if (at_kw ("if"))
              {
                next ();
                e->clauses.push_back ({ nullptr, expr () });
              }
            else
              {
                auto pat = pattern ();
                expect_op ("<-");
                e->clauses.push_back ({ pat, expr () });
              }
            if (at (Tok::Comma))
              {
                next ();
                continue;
              }
            if (at_kw ("if"))
              continue;
            break;
          }
        expect (Tok::RBracket, "`]`");
        --depth_;
        e->span = span_from (s);
        return e;
      }
    auto e = mk_expr (EK::List, s);
    e->kids.push_back (first);
    while (at (Tok::Comma))
      {
        next ();
        if (at (Tok::RBracket))
          break;
        e->kids.push_back (expr ());
      }
    expect (Tok::RBracket, "`]`");
    --depth_;
    e->span = span_from (s);
    return e;
  }

  void
  record_fields (const ExprP &e)
  {
    while (!at (Tok::RBrace))
      {
        if (!at (Tok::LIdent))
          unexpected ("a field name");
        e->labels.push_back (next ().text);
        if (!at_op ("=") && !at_op (":="))
          unexpected ("`=` after the field name");
        next ();
        e->kids.push_back (expr ());
        if (at (Tok::Comma) || at (Tok::Semi))
          next ();
        else
          break;
      }
    expect (Tok::RBrace, "`}`");
  }

  ExprP
  record ()
  {
    Span s = next ().span;
    ++depth_;
    if (at (Tok::LIdent) && (at_op ("=", 1) || at_op (":=", 1)))
      {
        auto e = mk_expr (EK::Record, s);
        record_fields (e);
        --depth_;
        e->span = span_from (s);
        check_dup_labels (e->labels, s);
        return e;
      }
    if (at (Tok::RBrace))
      {
        next ();
        --depth_;
        return mk_expr (EK::Record, span_from (s));
      }
    auto base = expr ();
    expect_kw ("with");
    auto e = mk_expr (EK::Update, s);
    e->kids.push_back (base);
    record_fields (e);
    --depth_;
    e->span = span_from (s);
    return e;
  }

  void
  check_dup_labels (const std::vector<std::string> &labels, const Span &s)
  {
    std::set<std::string> seen;
    for (auto &l : labels)
      if (!seen.insert (l).second)
        throw ParseFail{ make_diag ("M0208", "duplicate field `" + l + "` in record", s),
                         false };
  }

  ExprP
  match_expr ()
  {
    Span s = next ().span;
    auto scrut = expr ();
    expect_kw ("with");
    auto e = mk_expr (EK::Match, s);
    e->kids.push_back (scrut);
    if (at_op ("|"))
      next ();
    while (true)
      {
        auto pat = pattern ();
        ExprP guard;
        if (at_kw ("when"))
          {
            next ();
            guard = expr ();
          }
        expect_op ("->");
        auto body = expr ();
        e->pats.push_back (pat);
        e->guards.push_back (guard);
        e->kids.push_back (body);
        if (at_op ("|"))
          {
            next ();
            continue;
          }
        break;
      }
    e->span = span_from (s);
    return e;
  }

  struct Binding
  {
    PatP pat;
    ExprP rhs;
  };

  Binding
  binding (bool allow_colon_eq)
  {
    if (at (Tok::LIdent) && !(at_op ("=", 1) || at_op (":=", 1)) && peek (1).kind != Tok::Comma)
      {
        // function binding: name params = rhs
        Span s = peek ().span;
        std::string name = next ().text;
        std::vector<PatP> params;
        while (!at_op ("=") && !(allow_colon_eq && at_op (":=")))
          {
            if (at (Tok::Eof))
              unexpected ("`=`");
            params.push_back (patom ());
          }
        next ();
        auto body = expr ();
        auto lam = mk_lambda (std::move (params), body, span_from (s));
        lam->name = name;
        return { mk_pvar (name, s), lam };
      }
    auto pat = pattern ();
    if (!at_op ("=") && !(allow_colon_eq && at_op (":=")))
      unexpected ("`=`");
    next ();
    auto rhs = expr ();
    if (pat->kind == PK::Var && rhs->kind == EK::Lambda && rhs->name.empty ())
      rhs->name = pat->text;
    return { pat, rhs };
  }

  ExprP
  let_expr ()
  {
    Span s = next ().span;
    bool rec = false;
    if (at_kw ("rec"))
      {
        next ();
        rec = true;
      }
    std::vector<Binding> bs{ binding (true) };
    while (at_kw ("and"))
      {
        next ();
        bs.push_back (binding (true));
      }
    expect_kw ("in");
    auto body = expr ();
    if (rec)
      {
        auto e = mk_expr (EK::LetRec, span_from (s));
        for (auto &b : bs)
          {
            if (b.pat->kind != PK::Var)
              throw ParseFail{ make_diag ("M0205",
                                          "`let rec` can only bind names", b.pat->span),
                               false };
            e->pats.push_back (b.pat);
            e->kids.push_back (b.rhs);
          }
        e->kids.push_back (body);
        return e;
      }
    for (std::size_t i = bs.size (); i-- > 0;)
      {
        auto e = mk_expr (EK::Let, span_from (s));
        e->pats = { bs[i].pat };
        e->kids = { bs[i].rhs, body };
        body = e;
      }
    return body;
  }

  ExprP
  do_block ()
  {
    Span s = next ().span;
    expect (Tok::LBrace, "`{` after `do`");
    ++depth_;
    struct Stmt
    {
      bool is_let = false;
      bool rec = false;
      Binding b;
      ExprP e;
    };
    std::vector<Stmt> stmts;
    while (!at (Tok::RBrace))
      {
        Stmt st;
        if (at_kw ("let"))
          {
            next ();
            if (at_kw ("rec"))
              {
                next ();
                st.rec = true;
              }
            st.is_let = true;
            st.b = binding (true);
          }
        else
          st.e = expr ();
        stmts.push_back (std::move (st));
        if (at (Tok::Semi))
          {
            next ();
            if (at (Tok::RBrace))
              {
                Stmt u;
                u.e = mk_expr (EK::Unit, peek ().span);
                stmts.push_back (u);
              }
            continue;
          }
        break;
      }
    expect (Tok::RBrace, "`}` closing `do`");
    --depth_;
    Span whole = span_from (s);
    ExprP acc;
    if (stmts.empty () || stmts.back ().is_let)
      acc = mk_expr (EK::Unit, whole);
    for (std::size_t i = stmts.size (); i-- > 0;)
      {
        auto &st = stmts[i];
        if (!acc)
          {
            acc = st.e;
            continue;
          }
        if (st.is_let && st.rec)
          {
            auto e = mk_expr (EK::LetRec, whole);
            e->pats = { st.b.pat };
            e->kids = { st.b.rhs, acc };
            acc = e;
            continue;
          }
        auto e = mk_expr (EK::Let, whole);
        if (st.is_let)
          {
            e->pats = { st.b.pat };
            e->kids = { st.b.rhs, acc };
          }
        else
          {
            auto w = mk<Pat> (st.e->span);
            w->kind = PK::Wild;
            e->pats = { w };
            e->kids = { st.e, acc };
          }
        acc = e;
      }
    return acc;
  }

  // ---- patterns ----
  bool
  starts_patom (const Token &t) const
  {
    switch (t.kind)
      {
      case Tok::LIdent:
      case Tok::UIdent:
      case Tok::Int:
      case Tok::Float:
      case Tok::Str:
      case Tok::LParen:
      case Tok::LBracket:
      case Tok::LBrace:
        return true;
      case Tok::Kw:
        return t.text == "true" || t.text == "false";
      default:
        return false;
      }
  }

  PatP
  pattern ()
  {
    auto p = pat_app ();
    if (at_op ("::"))
      {
        next ();
        auto rest = pattern ();
        auto c = mk<Pat> (merge_span (p->span, rest->span));
        c->kind = PK::Cons;
        c->kids = { p, rest };
        return c;
      }
    return p;
  }

  PatP
  ctor_pat_head ()
  {
    Span s = peek ().span;
    std::vector<std::string> segs{ next ().text };
    while (at (Tok::Dot) && peek (1).kind == Tok::UIdent)
      {
        next ();
        segs.push_back (next ().text);
      }
    auto c = mk<Pat> (s);
    c->kind = PK::Ctor;
    c->text = segs.back ();
    segs.pop_back ();
    c->path = segs;
    if (at (Tok::LParen) && !peek ().trivia.space_before)
      {
        next ();
        ++depth_;
        if (!at (Tok::RParen))
          {
            c->kids.push_back (pattern ());
            while (at (Tok::Comma))
              {
                next ();
                c->kids.push_back (pattern ());
              }
          }
        expect (Tok::RParen, "`)`");
        --depth_;
      }
    c->span = span_from (s);
    return c;
  }

  PatP
  pat_app ()
  {
    if (at (Tok::UIdent))
      {
        auto c = ctor_pat_head ();
        if (c->kids.empty ())
          while (starts_patom (peek ()) && !at_boundary ())
            c->kids.push_back (patom ());
        c->span = span_from (c->span);
        return c;
      }
    return patom ();
  }

  PatP
  patom ()
  {
    const Token &t = peek ();
    Span s = t.span;
    auto p = mk<Pat> (s);
    switch (t.kind)
      {
      case Tok::LIdent:
        next ();
        if (t.text == "_")
          p->kind = PK::Wild;
        else
          {
            p->kind = PK::Var;
            p->text = t.text;
          }
        return p;
      case Tok::UIdent:
        return ctor_pat_head ();
      case Tok::Int:
        next ();
        p->kind = PK::Int;
        p->ival = t.ival;
        return p;
      case Tok::Float:
        next ();
        p->kind = PK::Float;
        p->fval = t.fval;
        return p;
      case Tok::Str:
        next ();
        p->kind = PK::Str;
        p->text = t.sval;
        return p;
      case Tok::Kw:
        if (t.text == "true" || t.text == "false")
          {
            next ();
            p->kind = PK::Bool;
            p->ival = t.text == "true";
            return p;
          }
        break;
      case Tok::LParen:
        {
          next ();
          ++depth_;
          if (at (Tok::RParen))
            {
              next ();
              --depth_;
              p->kind = PK::Unit;
              p->span = span_from (s);
              return p;
            }
          auto first = pattern ();
          if (at (Tok::Comma))
            {
              p->kind = PK::Tuple;
              p->kids.push_back (first);
              while (at (Tok::Comma))
                {
                  next ();
                  p->kids.push_back (pattern ());
                }
            }
          else if (at_op (":"))
            {
              next ();
              p->kind = PK::Annot;
              p->kids.push_back (first);
              p->annot = type ();
            }
          else
            p = first;
          expect (Tok::RParen, "`)`");
          --depth_;
          p->span = span_from (s);
          return p;
        }
      case Tok::LBracket:
        {
          next ();
          ++depth_;
          p->kind = PK::List;
          while (!at (Tok::RBracket))
            {
              p->kids.push_back (pattern ());
              if (at (Tok::Comma))
                next ();
              else
                break;
            }
          expect (Tok::RBracket, "`]`");
          --depth_;
          p->span = span_from (s);
          return p;
        }
      case Tok::LBrace:
        {
          next ();
          ++depth_;
          p->kind = PK::Record;
          while (!at (Tok::RBrace))
            {
              if (at_op (".."))
                {
                  next ();
                  p->open = true;
                  break;
                }
              if (!at (Tok::LIdent))
                unexpected ("a field name");
              const Token &f = next ();
              p->labels.push_back (f.text);
              if (at_op ("="))
                {
                  next ();
                  p->kids.push_back (pattern ());
                }
              else
                p->kids.push_back (mk_pvar (f.text, f.span));
              if (at (Tok::Comma) || at (Tok::Semi))
                next ();
              else
                break;
            }
          expect (Tok::RBrace, "`}`");
          --depth_;
          p->span = span_from (s);
          return p;
        }
      default:
        break;
      }
    unexpected ("a pattern");
  }

  // ---- types ----
  bool
  starts_tatom (const Token &t) const
  {
    return t.kind == Tok::TyVar || t.kind == Tok::LIdent || t.kind == Tok::UIdent
           || t.kind == Tok::LParen || t.kind == Tok::LBracket
           || t.kind == Tok::LBrace;
  }

  TypeExprP
  type ()
  {
    std::vector<std::string> nums;
    while (at (Tok::UIdent) && peek ().text == "Num"
           && (peek (1).kind == Tok::TyVar || peek (1).kind == Tok::LIdent)
           && peek (2).kind == Tok::Op && peek (2).text == "=>")
      {
        next ();
        nums.push_back (next ().text);
        next ();
      }
    auto t = type_arrow ();
    t->num_vars = nums;
    return t;
  }

  TypeExprP
  type_arrow ()
  {
    auto a = type_app ();
    if (at_op ("->"))
      {
        next ();
        auto b = type_arrow ();
        auto f = mk<TypeExpr> (merge_span (a->span, b->span));
        f->kind = TEK::Fun;
        f->kids = { a, b };
        return f;
      }
    return a;
  }

  bool
  type_arg_ok () const
  {
    const Token &t = peek ();
    if (!starts_tatom (t))
      return false;
    if (depth_ == 0 && t.trivia.newline_before)
      return false;
    if (t.kind == Tok::LIdent && (t.text == "list" || t.text == "option"))
      return false;
    return true;
  }

  TypeExprP
  type_app ()
  {
    TypeExprP t;
    if (at (Tok::UIdent))
      {
        t = tname ();
        if (!(at (Tok::LParen) && !peek ().trivia.space_before))
          while (type_arg_ok ())
            t->kids.push_back (tatom ());
      }
    else
      t = tatom ();
    while (at (Tok::LIdent) && (peek ().text == "list" || peek ().text == "option")
           && !(depth_ == 0 && peek ().trivia.newline_before))
      {
        const Token &w = next ();
        auto n = mk<TypeExpr> (merge_span (t->span, w.span));
        if (w.text == "list")
          {
            n->kind = TEK::List;
            n->kids = { t };
          }
        else
          {
            n->kind = TEK::Name;
            n->name = "Option";
            n->kids = { t };
          }
        t = n;
      }
    return t;
  }

  TypeExprP
  tname ()
  {
    Span s = peek ().span;
    auto t = mk<TypeExpr> (s);
    t->kind = TEK::Name;
    std::vector<std::string> segs{ next ().text };
    while (at (Tok::Dot) && (peek (1).kind == Tok::UIdent || peek (1).kind == Tok::LIdent))
      {
        next ();
        segs.push_back (next ().text);
        if (prev ().kind == Tok::LIdent)
          break;
      }
    t->name = segs.back ();
    segs.pop_back ();
    t->path = segs;
    if (at (Tok::LParen) && !peek ().trivia.space_before)
      {
        next ();
        ++depth_;
        t->kids.push_back (type ());
        while (at (Tok::Comma))
          {
            next ();
            t->kids.push_back (type ());
          }
        expect (Tok::RParen, "`)`");
        --depth_;
      }
    t->span = span_from (s);
    return t;
  }

  TypeExprP
  tatom ()
  {
    const Token &tk = peek ();
    Span s = tk.span;
    auto t = mk<TypeExpr> (s);
    switch (tk.kind)
      {
      case Tok::TyVar:
        next ();
        t->kind = TEK::Var;
        t->name = tk.text;
        return t;
      case Tok::LIdent:
        next ();
        t->kind = TEK::Name;
        t->name = tk.text;
        return t;
      case Tok::UIdent:
        return tname ();
      case Tok::LParen:
        {
          next ();
          ++depth_;
          if (at (Tok::RParen))
            {
              next ();
              --depth_;
              t->kind = TEK::Unit;
              return t;
            }
          auto first = type ();
          if (at (Tok::Comma))
            {
              t->kind = TEK::Tuple;
              t->kids.push_back (first);
              while (at (Tok::Comma))
                {
                  next ();
                  t->kids.push_back (type ());
                }
            }
          else
            t = first;
          expect (Tok::RParen, "`)`");
          --depth_;
          return t;
        }
      case Tok::LBracket:
        {
          next ();
          ++depth_;
          t->kind = TEK::List;
          t->kids.push_back (type ());
          expect (Tok::RBracket, "`]`");
          --depth_;
          t->span = span_from (s);
          return t;
        }
      case Tok::LBrace:
        {
          next ();
          ++depth_;
          t->kind = TEK::Record;
          while (!at (Tok::RBrace) && !at_op ("|"))
            {
              if (!at (Tok::LIdent))
                unexpected ("a field name");
              t->labels.push_back (next ().text);
              expect_op (":");
              t->kids.push_back (type ());
              if (at (Tok::Comma) || at (Tok::Semi))
                next ();
              else
                break;
            }
          if (at_op ("|"))
            {
              next ();
              if (!at (Tok::LIdent) && !at (Tok::TyVar))
                unexpected ("a row variable");
              t->row = next ().text;
            }
          expect (Tok::RBrace, "`}`");
          --depth_;
          t->span = span_from (s);
          return t;
        }
      default:
        break;
      }
    unexpected ("a type");
  }

  // ---- analyses ----
  static int
  max_tacit (const ExprP &e)
  {
    if (!e)
      return 0;
    int m = e->kind == EK::Tacit ? static_cast<int> (e->ival) : 0;
    for (auto &k : e->kids)
      m = std::max (m, max_tacit (k));
    for (auto &g : e->guards)
      m = std::max (m, max_tacit (g));
    for (auto &c : e->clauses)
      m = std::max (m, max_tacit (c.expr));
    return m;
  }

  void
  check_no_tacit (const ExprP &e)
  {
    if (max_tacit (e) > 0)
      {
        auto t = find_tacit (e);
        Diagnostic d = make_diag ("M0206",
                                  "tacit parameter `$" + std::to_string (t->ival)
                                      + "` used outside of a definition",
                                  t->span);
        d.suggestions.push_back ("`$n` refers to the n-th parameter of the enclosing "
                                 "`name := ...` definition");
        throw ParseFail{ d, false };
      }
  }
  static ExprP
  find_tacit (const ExprP &e)
  {
    if (e->kind == EK::Tacit)
      return e;
    for (auto &k : e->kids)
      if (max_tacit (k) > 0)
        return find_tacit (k);
    for (auto &g : e->guards)
      if (g && max_tacit (g) > 0)
        return find_tacit (g);
    for (auto &c : e->clauses)
      if (max_tacit (c.expr) > 0)
        return find_tacit (c.expr);
    return e;
  }

public:
  static void
  pat_names (const PatP &p, std::set<std::string> &out)
  {
    if (!p)
      return;
    if (p->kind == PK::Var)
      out.insert (p->text);
    for (auto &k : p->kids)
      pat_names (k, out);
  }

  /// Free variables of an expression (honouring binders).
  static void
  free_vars (const ExprP &e, const std::set<std::string> &bound,
             std::set<std::string> &out)
  {
    if (!e)
      return;
    switch (e->kind)
      {
      case EK::Var:
        if (!bound.count (e->text))
          out.insert (e->text);
        return;
      case EK::Lambda:
        {
          auto b = bound;
          for (auto &p : e->pats)
            pat_names (p, b);
          free_vars (e->kids[0], b, out);
          return;
        }
      case EK::Let:
        {
          free_vars (e->kids[0], bound, out);
          auto b = bound;
          pat_names (e->pats[0], b);
          free_vars (e->kids[1], b, out);
          return;
        }
      case EK::LetRec:
        {
          auto b = bound;
          for (auto &p : e->pats)
            pat_names (p, b);
          for (auto &k : e->kids)
            free_vars (k, b, out);
          return;
        }
      case EK::Match:
        {
          free_vars (e->kids[0], bound, out);
          for (std::size_t i = 0; i < e->pats.size (); ++i)
            {
              auto b = bound;
              pat_names (e->pats[i], b);
              if (e->guards[i])
                free_vars (e->guards[i], b, out);
              free_vars (e->kids[i + 1], b, out);
            }
          return;
        }
      case EK::Comp:
        {
          auto b = bound;
          for (auto &c : e->clauses)
            {
              free_vars (c.expr, b, out);
              if (c.pat)
                pat_names (c.pat, b);
            }
          free_vars (e->kids[0], b, out);
          return;
        }
      case EK::BinOp:
        if (!is_builtin_op (e->text) && !bound.count (e->text))
          out.insert (e->text);
        break;
      case EK::OpRef:
        if (!is_builtin_op (e->text) && !bound.count (e->text))
          out.insert (e->text);
        return;
      default:
        break;
      }
    for (auto &k : e->kids)
      free_vars (k, bound, out);
  }
};

} // namespace detail

/// Parses a whole program. Never throws.
inline ParseOutput
parse (std::string_view source, const std::string &file = "<input>")
{
  ParseOutput out;
  out.program.file = file;
  out.program.source = std::string (source);
  auto lx = lex (source, file);
  if (!lx.diagnostics.empty ())
    {
      out.diagnostics = lx.diagnostics;
      out.incomplete = lx.incomplete;
      return out;
    }
  detail::ParserImpl p (std::move (lx.tokens), file);
  try
    {
      out.program.items = p.program ();
    }
  catch (const detail::ParseFail &f)
    {
      out.diagnostics.push_back (f.diag);
      out.incomplete = f.at_eof;
    }
  catch (const detail::MetaError &e)
    {
      out.diagnostics.push_back (e.diag);
    }
  return out;
}

/// Parses a single expression (used by #type / #trace).
inline dsl::Result<ExprP, std::vector<Diagnostic>>
parse_expression (std::string_view source, const std::string &file = "<input>")
{
  using R = dsl::Result<ExprP, std::vector<Diagnostic>>;
  auto lx = lex (source, file);
  if (!lx.diagnostics.empty ())
    return R::from_err (lx.diagnostics);
  detail::ParserImpl p (std::move (lx.tokens), file);
  try
    {
      return R::from_ok (p.expression_only ());
    }
  catch (const detail::ParseFail &f)
    {
      return R::from_err ({ f.diag });
    }
}

/// Parses a type expression (used by primitives and Type.*).
inline dsl::Result<TypeExprP, std::vector<Diagnostic>>
parse_type_expr (std::string_view source, const std::string &file = "<type>")
{
  using R = dsl::Result<TypeExprP, std::vector<Diagnostic>>;
  auto lx = lex (source, file);
  if (!lx.diagnostics.empty ())
    return R::from_err (lx.diagnostics);
  detail::ParserImpl p (std::move (lx.tokens), file);
  try
    {
      return R::from_ok (p.type_only ());
    }
  catch (const detail::ParseFail &f)
    {
      return R::from_err ({ f.diag });
    }
}

// ============================================================
//  Types: union-find type graph with a trail (Algorithm W + Rémy rows)
// ============================================================

enum class TK
{
  Var,
  Con,
  RowEmpty,
  RowExt
};

struct TypeNode;
using TypeP = std::shared_ptr<TypeNode>;

inline constexpr int GENERIC = 1 << 30;

struct TypeNode
{
  TK kind = TK::Var;
  // Var
  int id = 0;
  int level = 0;
  bool num = false;   ///< constrained to Int | Float
  bool rigid = false; ///< skolem from a signature
  TypeP link{};
  std::string hint{}; ///< display name for rigid variables
  // Con
  std::string name{};
  int stamp = 0;
  std::vector<TypeP> args{};
  // RowExt
  std::string label{};
  TypeP field{};
  TypeP rest{};
};

/// Builtin constructor stamps (shared by both dialects).
namespace stamps
{
inline constexpr int Int = 1, Float = 2, String = 3, Bool = 4, Unit = 5,
                     List = 6, Fun = 7, Tuple = 8, Record = 9, First = 100;
}

inline TypeP
prune (TypeP t)
{
  while (t->kind == TK::Var && t->link)
    t = t->link;
  return t;
}

inline TypeP
t_con (const std::string &name, int stamp, std::vector<TypeP> args = {})
{
  auto t = std::make_shared<TypeNode> ();
  t->kind = TK::Con;
  t->name = name;
  t->stamp = stamp;
  t->args = std::move (args);
  return t;
}
inline TypeP
t_int ()
{
  return t_con ("Int", stamps::Int);
}
inline TypeP
t_float ()
{
  return t_con ("Float", stamps::Float);
}
inline TypeP
t_string ()
{
  return t_con ("String", stamps::String);
}
inline TypeP
t_bool ()
{
  return t_con ("Bool", stamps::Bool);
}
inline TypeP
t_unit ()
{
  return t_con ("Unit", stamps::Unit);
}
inline TypeP
t_list (TypeP a)
{
  return t_con ("List", stamps::List, { std::move (a) });
}
inline TypeP
t_fun (TypeP a, TypeP b)
{
  return t_con ("->", stamps::Fun, { std::move (a), std::move (b) });
}
inline TypeP
t_tuple (std::vector<TypeP> items)
{
  return t_con ("*", stamps::Tuple, std::move (items));
}
inline TypeP
t_record (TypeP row)
{
  return t_con ("Record", stamps::Record, { std::move (row) });
}
inline TypeP
row_empty ()
{
  auto t = std::make_shared<TypeNode> ();
  t->kind = TK::RowEmpty;
  return t;
}
inline TypeP
row_ext (std::string label, TypeP field, TypeP rest)
{
  auto t = std::make_shared<TypeNode> ();
  t->kind = TK::RowExt;
  t->label = std::move (label);
  t->field = std::move (field);
  t->rest = std::move (rest);
  return t;
}
inline bool
is_fun (const TypeP &t)
{
  auto p = prune (t);
  return p->kind == TK::Con && p->stamp == stamps::Fun;
}

/// Deterministic type printer: variables named 'a, 'b, ... by first use.
class TypePrinter
{
public:
  std::string
  print (const TypeP &t)
  {
    std::string body = go (t, 0);
    std::string pre;
    for (auto &n : num_order_)
      pre += (pre.empty () ? "" : ", ") + std::string ("Num ") + n;
    return pre.empty () ? body : pre + " => " + body;
  }
  std::string
  print_bare (const TypeP &t)
  {
    return go (t, 0);
  }

private:
  std::map<const TypeNode *, std::string> names_{};
  std::vector<std::string> num_order_{};
  int next_ = 0;

  std::string
  var_name (const TypeNode *v)
  {
    auto it = names_.find (v);
    if (it != names_.end ())
      return it->second;
    std::string n;
    if (v->rigid && !v->hint.empty ())
      n = v->hint[0] == '\'' ? v->hint : "'" + v->hint;
    else
      {
        int k = next_++;
        n = "'";
        n += static_cast<char> ('a' + k % 26);
        if (k >= 26)
          n += std::to_string (k / 26);
      }
    // avoid clashes between hinted and generated names
    for (auto &kv : names_)
      if (kv.second == n)
        n += "_" + std::to_string (v->id);
    names_[v] = n;
    if (v->num)
      num_order_.push_back (n);
    return n;
  }

  std::string
  go (const TypeP &t0, int prec)
  {
    auto t = prune (t0);
    switch (t->kind)
      {
      case TK::Var:
        return var_name (t.get ());
      case TK::RowEmpty:
        return "{}";
      case TK::RowExt:
        return "{ " + row_text (t) + " }";
      case TK::Con:
        break;
      }
    if (t->stamp == stamps::Fun)
      {
        std::string lhs = go (t->args[0], 1);
        std::string s = lhs + " -> " + go (t->args[1], 0);
        return prec > 0 ? "(" + s + ")" : s;
      }
    if (t->stamp == stamps::List)
      return "[" + go (t->args[0], 0) + "]";
    if (t->stamp == stamps::Tuple)
      {
        std::string s = "(";
        for (std::size_t i = 0; i < t->args.size (); ++i)
          s += (i ? ", " : "") + go (t->args[i], 0);
        return s + ")";
      }
    if (t->stamp == stamps::Record)
      {
        auto row = prune (t->args[0]);
        if (row->kind == TK::RowEmpty)
          return "{}";
        return "{ " + row_text (row) + " }";
      }
    if (t->args.empty ())
      return t->name;
    std::string s = t->name;
    for (auto &a : t->args)
      s += " " + go (a, 2);
    return prec > 1 ? "(" + s + ")" : s;
  }

  std::string
  row_text (TypeP row)
  {
    std::vector<std::pair<std::string, TypeP>> fields;
    row = prune (row);
    while (row->kind == TK::RowExt)
      {
        fields.push_back ({ row->label, row->field });
        row = prune (row->rest);
      }
    std::stable_sort (fields.begin (), fields.end (),
                      [] (const auto &a, const auto &b) { return a.first < b.first; });
    std::string s;
    for (std::size_t i = 0; i < fields.size (); ++i)
      s += (i ? ", " : "") + fields[i].first + " : " + go (fields[i].second, 0);
    if (row->kind == TK::Var)
      s += " | " + var_name (row.get ());
    return s;
  }
};

inline std::string
show_type (const TypeP &t)
{
  TypePrinter p;
  return p.print (t);
}

/// Unifier with an undo trail (needed for type-directed resolution).
class Unifier
{
public:
  std::string reason{};      ///< detail of the last failure
  std::string reason_code{}; ///< diagnostic code for the last failure

  std::size_t
  snapshot () const
  {
    return trail_.size ();
  }
  void
  rollback (std::size_t s)
  {
    while (trail_.size () > s)
      {
        auto &e = trail_.back ();
        e.v->link = e.link;
        e.v->level = e.level;
        e.v->num = e.num;
        trail_.pop_back ();
      }
  }
  void
  commit ()
  {
    trail_.clear ();
  }
  void
  set_level (TypeNode *v, int level)
  {
    record (v);
    v->level = level;
  }

  bool
  unify (TypeP a, TypeP b)
  {
    a = prune (a);
    b = prune (b);
    if (a == b)
      return true;
    if (a->kind == TK::Var && b->kind == TK::Var)
      {
        if (a->rigid && b->rigid)
          return fail ("M0309", "the rigid type variables `" + rigid_name (a) + "` and `"
                                    + rigid_name (b) + "` are distinct");
        if (a->rigid)
          return bind (b, a);
        return bind (a, b);
      }
    if (a->kind == TK::Var)
      return bind (a, b);
    if (b->kind == TK::Var)
      return bind (b, a);
    if (a->kind == TK::Con && b->kind == TK::Con)
      {
        if (a->stamp != b->stamp || a->args.size () != b->args.size ())
          return fail ("M0301", "");
        for (std::size_t i = 0; i < a->args.size (); ++i)
          if (!unify (a->args[i], b->args[i]))
            return false;
        return true;
      }
    if (a->kind == TK::RowEmpty && b->kind == TK::RowEmpty)
      return true;
    if (a->kind == TK::RowEmpty && b->kind == TK::RowExt)
      std::swap (a, b);
    if (a->kind == TK::RowExt && (b->kind == TK::RowExt || b->kind == TK::RowEmpty))
      {
        TypeP tail = row_tail (a->rest);
        TypeP f, r;
        if (!rewrite (b, a->label, f, r))
          return false;
        if (tail->kind == TK::Var && tail->link)
          return fail ("M0303", "recursive row type");
        if (!unify (a->field, f))
          return false;
        return unify (a->rest, r);
      }
    return fail ("M0301", "");
  }

  std::function<TypeP (int level)> fresh_var{};

private:
  struct Entry
  {
    TypeNode *v;
    TypeP link;
    int level;
    bool num;
  };
  std::vector<Entry> trail_{};

  void
  record (TypeNode *v)
  {
    trail_.push_back ({ v, v->link, v->level, v->num });
  }
  bool
  fail (std::string code, std::string why)
  {
    reason_code = std::move (code);
    reason = std::move (why);
    return false;
  }
  static std::string
  rigid_name (const TypeP &v)
  {
    if (v->hint.empty ())
      return "'?";
    return v->hint[0] == '\'' ? v->hint : "'" + v->hint;
  }
  static TypeP
  row_tail (TypeP r)
  {
    r = prune (r);
    while (r->kind == TK::RowExt)
      r = prune (r->rest);
    return r;
  }
  bool
  occurs_adjust (TypeNode *v, const TypeP &t0)
  {
    auto t = prune (t0);
    if (t.get () == v)
      return true;
    switch (t->kind)
      {
      case TK::Var:
        if (t->level > v->level)
          set_level (t.get (), v->level);
        return false;
      case TK::Con:
        for (auto &a : t->args)
          if (occurs_adjust (v, a))
            return true;
        return false;
      case TK::RowExt:
        return occurs_adjust (v, t->field) || occurs_adjust (v, t->rest);
      case TK::RowEmpty:
        return false;
      }
    return false;
  }
  bool
  bind (const TypeP &v, const TypeP &t)
  {
    if (v->rigid)
      {
        if (t->kind == TK::Con)
          return fail ("M0309", "the type variable `" + rigid_name (v)
                                    + "` comes from a signature and cannot be `"
                                    + show_type (t) + "`");
        return fail ("M0309", "rigid type variable `" + rigid_name (v) + "`");
      }
    if (occurs_adjust (v.get (), t))
      {
        TypePrinter p;
        std::string vs = p.print_bare (v);
        std::string ts = p.print_bare (t);
        return fail ("M0302", "cannot construct the infinite type `" + vs + " = " + ts + "`");
      }
    if (v->num)
      {
        if (t->kind == TK::Var)
          {
            if (!t->num)
              {
                if (t->rigid)
                  return fail ("M0306", "the type variable `" + rigid_name (t)
                                            + "` from the signature is not numeric; "
                                              "declare it as `Num "
                                            + rigid_name (t) + " =>`");
                record (t.get ());
                t->num = true;
              }
          }
        else if (t->kind != TK::Con
                 || (t->stamp != stamps::Int && t->stamp != stamps::Float))
          return fail ("M0306", "expected a numeric type (`Int` or `Float`) but found `"
                                    + show_type (t) + "`");
      }
    record (v.get ());
    v->link = t;
    return true;
  }
  bool
  rewrite (TypeP row, const std::string &label, TypeP &field, TypeP &rest)
  {
    row = prune (row);
    if (row->kind == TK::RowExt)
      {
        if (row->label == label)
          {
            field = row->field;
            rest = row->rest;
            return true;
          }
        TypeP f, r;
        if (!rewrite (row->rest, label, f, r))
          return false;
        field = f;
        rest = row_ext (row->label, row->field, r);
        return true;
      }
    if (row->kind == TK::Var)
      {
        if (row->rigid)
          return fail ("M0303", "the record type is closed by a signature and has no field `"
                                    + label + "`");
        field = fresh_var (row->level);
        rest = fresh_var (row->level);
        return bind (row, row_ext (label, field, rest));
      }
    return fail ("M0303", "the record has no field `" + label + "`");
  }
};

// ============================================================
//  Static environment: scopes, type definitions, module signatures
// ============================================================

/// Insertion-ordered map (deterministic iteration; mirrored in D).
template <class T> class OrderedMap
{
public:
  T *
  find (const std::string &k)
  {
    auto it = map_.find (k);
    return it == map_.end () ? nullptr : &it->second;
  }
  const T *
  find (const std::string &k) const
  {
    auto it = map_.find (k);
    return it == map_.end () ? nullptr : &it->second;
  }
  void
  set (const std::string &k, T v)
  {
    auto it = map_.find (k);
    if (it == map_.end ())
      {
        order_.push_back (k);
        map_.emplace (k, std::move (v));
      }
    else
      it->second = std::move (v);
  }
  void
  erase (const std::string &k)
  {
    if (map_.erase (k))
      order_.erase (std::find (order_.begin (), order_.end (), k));
  }
  const std::vector<std::string> &
  keys () const
  {
    return order_;
  }
  std::size_t
  size () const
  {
    return order_.size ();
  }

private:
  std::vector<std::string> order_{};
  std::unordered_map<std::string, T> map_{};
};

struct CtorInfo;
struct TypeDef;
struct Scope;
struct FunctorInfo;
struct SigDef;
using CtorInfoP = std::shared_ptr<CtorInfo>;
using TypeDefP = std::shared_ptr<TypeDef>;
using ScopeP = std::shared_ptr<Scope>;
using FunctorInfoP = std::shared_ptr<FunctorInfo>;
using SigDefP = std::shared_ptr<SigDef>;

struct TypeDef
{
  std::string name{};
  std::string display{};
  int stamp = 0;
  bool alias = false;
  std::vector<TypeP> params{}; ///< generic variables
  TypeP body{};                ///< alias body (in terms of params)
  std::vector<CtorInfoP> ctors{};
};

struct CtorInfo
{
  std::string name{};
  int index = 0;
  int arity = 0;
  TypeP scheme{}; ///< generic: arg1 -> ... -> T params
  TypeDefP owner{};
};

/// Runtime cell for a top-level binding (static binding for redefinition).
struct Cell;

struct VarInfo
{
  TypeP scheme{};
  CellP cell{};
};

struct SigDef
{
  std::string name{};
  SigExprP expr{};
  ScopeP env{};
};

struct FunctorInfo
{
  std::string name{};
  ItemP item{};
  ScopeP env{};
};

/// A lexical scope; a module signature is a Scope whose own entries are its
/// exports.
struct Scope
{
  ScopeP parent{};
  std::string prefix{}; ///< display prefix for types declared here ("M.")
  OrderedMap<VarInfo> vars{};
  OrderedMap<TypeDefP> types{};
  OrderedMap<CtorInfoP> ctors{};
  OrderedMap<ScopeP> modules{};
  OrderedMap<FunctorInfoP> functors{};
  OrderedMap<SigDefP> sigs{};
  OrderedMap<ItemP> pending{}; ///< signatures awaiting a definition

  static ScopeP
  child (const ScopeP &parent, std::string prefix = {})
  {
    auto s = std::make_shared<Scope> ();
    s->parent = parent;
    s->prefix = prefix.empty () && parent ? parent->prefix : std::move (prefix);
    return s;
  }

  template <class Get>
  auto
  lookup (const std::string &k, Get get) const -> decltype (get (*this, k))
  {
    for (const Scope *s = this; s; s = s->parent.get ())
      if (auto r = get (*s, k))
        return r;
    return nullptr;
  }
  const VarInfo *
  var (const std::string &k) const
  {
    return lookup (k, [] (const Scope &s, const std::string &n) { return s.vars.find (n); });
  }
  const TypeDefP *
  type (const std::string &k) const
  {
    return lookup (k, [] (const Scope &s, const std::string &n) { return s.types.find (n); });
  }
  const CtorInfoP *
  ctor (const std::string &k) const
  {
    return lookup (k, [] (const Scope &s, const std::string &n) { return s.ctors.find (n); });
  }
  const ScopeP *
  module (const std::string &k) const
  {
    return lookup (k,
                   [] (const Scope &s, const std::string &n) { return s.modules.find (n); });
  }
  const FunctorInfoP *
  functor (const std::string &k) const
  {
    return lookup (k,
                   [] (const Scope &s, const std::string &n) { return s.functors.find (n); });
  }
  const SigDefP *
  sig (const std::string &k) const
  {
    return lookup (k, [] (const Scope &s, const std::string &n) { return s.sigs.find (n); });
  }
};

// ============================================================
//  Runtime values (needed by the checker for top-level cells)
// ============================================================

enum class VK : std::uint8_t
{
  Unit,
  Bool,
  Int,
  Float,
  Str,
  List,
  Tuple,
  Record,
  Ctor,
  Closure,
  Prim,
  CtorFn,
  Compose,
  Module,
  Foreign
};

/// Base of every heap-allocated runtime object.
struct Obj
{
  virtual ~Obj () = default;
};
using ObjP = std::shared_ptr<Obj>;

/// A MetaML runtime value (immediate scalars + shared heap object).
struct Value
{
  VK kind = VK::Unit;
  std::int64_t i = 0; ///< Int, Bool, constructor index
  double f = 0.0;     ///< Float
  ObjP obj{};

  static Value
  unit ()
  {
    return {};
  }
  static Value
  boolean (bool b)
  {
    Value v;
    v.kind = VK::Bool;
    v.i = b ? 1 : 0;
    return v;
  }
  static Value
  integer (std::int64_t n)
  {
    Value v;
    v.kind = VK::Int;
    v.i = n;
    return v;
  }
  static Value
  floating (double d)
  {
    Value v;
    v.kind = VK::Float;
    v.f = d;
    return v;
  }
};

/// A top-level binding cell (static binding across REPL redefinitions).
struct Cell
{
  std::string name{};
  bool defined = false;
  Value value{};
};

// ============================================================
//  Type checker (Hindley–Milner; type-directed concat resolution; modules)
// ============================================================


/// What the REPL reports for a checked unit.
struct UnitInfo
{
  std::vector<std::pair<std::string, TypeP>> defs{};
  TypeP expr_type{};
};

class Checker
{
public:
  Unifier u{};
  int level = 0;
  int next_id = 0;
  int next_stamp = stamps::First;
  ScopeP global{};
  std::vector<Diagnostic> warnings{};
  std::vector<std::string> disabled{};
  bool quiet = false; ///< suppress warnings (functor re-checks, stdlib)
  std::function<Primitive *(const std::string &)> find_prim{};
  std::function<TypeP (Primitive *, const Span &)> prim_type{};

  Checker ()
  {
    u.fresh_var = [this] (int lvl) { return fresh_at (lvl); };
    global = std::make_shared<Scope> ();
    auto builtin = [&] (const std::string &n, int stamp, int arity)
      {
        auto d = std::make_shared<TypeDef> ();
        d->name = n;
        d->display = n;
        d->stamp = stamp;
        for (int i = 0; i < arity; ++i)
          d->params.push_back (generic_var ());
        global->types.set (n, d);
      };
    builtin ("Int", stamps::Int, 0);
    builtin ("Float", stamps::Float, 0);
    builtin ("String", stamps::String, 0);
    builtin ("Bool", stamps::Bool, 0);
    builtin ("Unit", stamps::Unit, 0);
    builtin ("List", stamps::List, 1);
  }

  // ---- variables & schemes ----
  TypeP
  fresh_at (int lvl)
  {
    auto t = std::make_shared<TypeNode> ();
    t->kind = TK::Var;
    t->id = ++next_id;
    t->level = lvl;
    return t;
  }
  TypeP
  fresh ()
  {
    return fresh_at (level);
  }
  TypeP
  fresh_num ()
  {
    auto t = fresh ();
    t->num = true;
    return t;
  }
  TypeP
  generic_var ()
  {
    auto t = fresh_at (GENERIC);
    return t;
  }

  void
  generalize (const TypeP &t0, int lvl)
  {
    auto t = prune (t0);
    switch (t->kind)
      {
      case TK::Var:
        if (t->level > lvl && t->level != GENERIC)
          u.set_level (t.get (), GENERIC);
        return;
      case TK::Con:
        for (auto &a : t->args)
          generalize (a, lvl);
        return;
      case TK::RowExt:
        generalize (t->field, lvl);
        generalize (t->rest, lvl);
        return;
      case TK::RowEmpty:
        return;
      }
  }

  TypeP
  copy_generic (const TypeP &t0, std::map<const TypeNode *, TypeP> &m, bool rigid)
  {
    auto t = prune (t0);
    switch (t->kind)
      {
      case TK::Var:
        {
          if (t->level != GENERIC)
            return t;
          auto it = m.find (t.get ());
          if (it != m.end ())
            return it->second;
          auto v = fresh ();
          v->num = t->num;
          v->rigid = rigid;
          v->hint = t->hint;
          m[t.get ()] = v;
          return v;
        }
      case TK::Con:
        {
          std::vector<TypeP> args;
          for (auto &a : t->args)
            args.push_back (copy_generic (a, m, rigid));
          return t_con (t->name, t->stamp, std::move (args));
        }
      case TK::RowExt:
        {
          auto f = copy_generic (t->field, m, rigid);
          return row_ext (t->label, f, copy_generic (t->rest, m, rigid));
        }
      case TK::RowEmpty:
        return t;
      }
    return t;
  }
  TypeP
  instantiate (const TypeP &t)
  {
    std::map<const TypeNode *, TypeP> m;
    return copy_generic (t, m, false);
  }
  TypeP
  skolemize (const TypeP &t)
  {
    std::map<const TypeNode *, TypeP> m;
    return copy_generic (t, m, true);
  }
  /// Substitutes generic parameters by arguments (alias expansion).
  TypeP
  subst (const TypeP &t0, const std::map<const TypeNode *, TypeP> &m)
  {
    auto t = prune (t0);
    switch (t->kind)
      {
      case TK::Var:
        {
          auto it = m.find (t.get ());
          return it != m.end () ? it->second : t;
        }
      case TK::Con:
        {
          std::vector<TypeP> args;
          for (auto &a : t->args)
            args.push_back (subst (a, m));
          return t_con (t->name, t->stamp, std::move (args));
        }
      case TK::RowExt:
        {
          auto f = subst (t->field, m);
          return row_ext (t->label, f, subst (t->rest, m));
        }
      case TK::RowEmpty:
        return t;
      }
    return t;
  }

  // ---- errors ----
  [[noreturn]] void
  error (std::string code, std::string msg, const Span &sp,
         std::vector<std::string> suggestions = {})
  {
    Diagnostic d = detail::make_diag (std::move (code), std::move (msg), sp);
    d.suggestions = std::move (suggestions);
    throw detail::MetaError{ d };
  }
  void
  warn (const std::string &code, const std::string &msg, const Span &sp)
  {
    if (quiet)
      return;
    if (std::find (disabled.begin (), disabled.end (), code) != disabled.end ())
      return;
    warnings.push_back (detail::make_diag (code, msg, sp, Severity::Warning));
  }

  static std::string
  conversion_hint (const std::string &expected, const std::string &found)
  {
    if (expected == "Float" && found == "Int")
      return "consider calling `Float.of_int`";
    if (expected == "Int" && found == "Float")
      return "consider calling `Float.truncate`";
    if (expected == "String" && found == "Int")
      return "consider calling `Int.to_string`";
    if (expected == "String" && found == "Float")
      return "consider calling `Float.to_string`";
    if (expected == "Unit" && found != "Unit")
      return "use `let _ := ...;` to discard a value";
    return "";
  }

  void
  unify_or_error (const TypeP &expected, const TypeP &found, const Span &sp,
                  const std::string &context = "")
  {
    auto snap = u.snapshot ();
    if (u.unify (expected, found))
      return;
    std::string code = u.reason_code, reason = u.reason;
    u.rollback (snap);
    TypePrinter p;
    std::string e = p.print_bare (expected), f = p.print_bare (found);
    std::string msg = "expected `" + e + "` but found `" + f + "`";
    if (code == "M0306" && !reason.empty ())
      msg = reason;
    else if ((code == "M0302" || code == "M0303" || code == "M0309") && !reason.empty ())
      msg += "; " + reason;
    else
      code = "M0301";
    if (!context.empty ())
      msg = context + ": " + msg;
    auto hint = conversion_hint (e, f);
    if (!hint.empty ())
      msg += "; " + hint;
    error (code, msg, sp);
  }

  // ---- names & suggestions ----
  static std::size_t
  edit_distance (const std::string &a, const std::string &b)
  {
    std::vector<std::size_t> prev (b.size () + 1), cur (b.size () + 1);
    for (std::size_t j = 0; j <= b.size (); ++j)
      prev[j] = j;
    for (std::size_t i = 1; i <= a.size (); ++i)
      {
        cur[0] = i;
        for (std::size_t j = 1; j <= b.size (); ++j)
          cur[j] = std::min ({ prev[j] + 1, cur[j - 1] + 1,
                               prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1) });
        std::swap (prev, cur);
      }
    return prev[b.size ()];
  }
  static std::string
  closest (const std::string &name, const std::vector<std::string> &cands)
  {
    std::string best;
    std::size_t bd = 3;
    for (auto &c : cands)
      {
        if (c.empty () || c[0] == '$' || c[0] == '#' || c == name)
          continue;
        auto d = edit_distance (name, c);
        if (d < bd || (d == bd && !best.empty () && c < best))
          {
            bd = d;
            best = c;
          }
      }
    if (bd > 2 || bd >= name.size ())
      return "";
    return best;
  }
  std::vector<std::string>
  visible_vars (const ScopeP &scope) const
  {
    std::vector<std::string> out;
    for (const Scope *s = scope.get (); s; s = s->parent.get ())
      for (auto &k : s->vars.keys ())
        out.push_back (k);
    return out;
  }

  // ---- module paths ----
  ScopeP
  resolve_mpath (const std::vector<std::string> &path, const ScopeP &scope,
                 const Span &sp)
  {
    auto m = scope->module (path[0]);
    if (!m)
      error ("M0501", "unbound module `" + path[0] + "`", sp);
    ScopeP cur = *m;
    for (std::size_t i = 1; i < path.size (); ++i)
      {
        auto n = cur->modules.find (path[i]);
        if (!n)
          error ("M0501", "module `" + detail::join (std::vector<std::string> (
                                                          path.begin (), path.begin () + i),
                                                      ".")
                              + "` has no submodule `" + path[i] + "`",
                 sp);
        cur = *n;
      }
    return cur;
  }

  // ---- type expressions ----
  struct TVarMap
  {
    std::map<std::string, TypeP> vars{};
    bool allow_new = true;
  };

  TypeP
  apply_typedef (const TypeDefP &d, std::vector<TypeP> args, const Span &sp)
  {
    if (args.size () != d->params.size ())
      error ("M0210", "type `" + d->display + "` expects "
                          + std::to_string (d->params.size ()) + " argument(s) but was given "
                          + std::to_string (args.size ()),
             sp);
    if (d->alias)
      {
        if (!d->body)
          error ("M0210", "type alias `" + d->display + "` refers to itself", sp);
        std::map<const TypeNode *, TypeP> m;
        for (std::size_t i = 0; i < args.size (); ++i)
          m[d->params[i].get ()] = args[i];
        return subst (d->body, m);
      }
    return t_con (d->display, d->stamp, std::move (args));
  }

  TypeP
  tvar_named (const std::string &name, TVarMap &m, const Span &sp)
  {
    auto it = m.vars.find (name);
    if (it != m.vars.end ())
      return it->second;
    if (!m.allow_new)
      error ("M0210", "unbound type variable `" + name + "`", sp);
    auto v = fresh ();
    v->hint = name;
    m.vars[name] = v;
    return v;
  }

  TypeP
  resolve_type (const TypeExprP &te, const ScopeP &scope, TVarMap &m)
  {
    for (auto &n : te->num_vars)
      tvar_named (n, m, te->span)->num = true;
    switch (te->kind)
      {
      case TEK::Var:
        return tvar_named (te->name, m, te->span);
      case TEK::Unit:
        return t_unit ();
      case TEK::Fun:
        {
          auto a = resolve_type (te->kids[0], scope, m);
          return t_fun (a, resolve_type (te->kids[1], scope, m));
        }
      case TEK::Tuple:
        {
          std::vector<TypeP> items;
          for (auto &k : te->kids)
            items.push_back (resolve_type (k, scope, m));
          return t_tuple (std::move (items));
        }
      case TEK::List:
        return t_list (resolve_type (te->kids[0], scope, m));
      case TEK::Record:
        {
          TypeP row = te->row.empty () ? row_empty () : tvar_named (te->row, m, te->span);
          std::set<std::string> seen;
          for (std::size_t i = te->kids.size (); i-- > 0;)
            {
              if (!seen.insert (te->labels[i]).second)
                error ("M0208", "duplicate field `" + te->labels[i] + "` in record type",
                       te->span);
              auto ft = resolve_type (te->kids[i], scope, m);
              row = row_ext (te->labels[i], ft, row);
            }
          return t_record (row);
        }
      case TEK::Name:
        {
          std::vector<TypeP> args;
          for (auto &k : te->kids)
            args.push_back (resolve_type (k, scope, m));
          if (!te->path.empty ())
            {
              auto ms = resolve_mpath (te->path, scope, te->span);
              auto d = ms->types.find (te->name);
              if (!d)
                error ("M0210", "module `" + detail::join (te->path, ".")
                                    + "` has no type `" + te->name + "`",
                       te->span);
              return apply_typedef (*d, std::move (args), te->span);
            }
          if (auto d = scope->type (te->name))
            return apply_typedef (*d, std::move (args), te->span);
          if (detail::is_lower (te->name[0]) && args.empty ())
            return tvar_named (te->name, m, te->span);
          std::vector<std::string> cands;
          for (const Scope *s = scope.get (); s; s = s->parent.get ())
            for (auto &k : s->types.keys ())
              cands.push_back (k);
          auto near = closest (te->name, cands);
          error ("M0210", "unbound type `" + te->name + "`", te->span,
                 near.empty () ? std::vector<std::string>{}
                               : std::vector<std::string>{ "did you mean `" + near + "`?" });
        }
      }
    return t_unit ();
  }

  /// Resolves a signature type into a generic scheme.
  TypeP
  sig_scheme (const TypeExprP &te, const ScopeP &scope)
  {
    TVarMap m;
    ++level;
    auto t = resolve_type (te, scope, m);
    --level;
    generalize (t, level);
    return t;
  }

  // ---- builtin operators ----
  TypeP
  builtin_op_type (const std::string &op)
  {
    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%")
      {
        auto a = fresh_num ();
        return t_fun (a, t_fun (a, a));
      }
    if (op == "==" || op == "!=" || op == "=" || op == "<>" || op == "<" || op == ">"
        || op == "<=" || op == ">=")
      {
        auto a = fresh ();
        return t_fun (a, t_fun (a, t_bool ()));
      }
    if (op == "&&" || op == "||")
      return t_fun (t_bool (), t_fun (t_bool (), t_bool ()));
    if (op == "^")
      return t_fun (t_string (), t_fun (t_string (), t_string ()));
    if (op == "@")
      {
        auto a = t_list (fresh ());
        return t_fun (a, t_fun (a, a));
      }
    if (op == "::")
      {
        auto a = fresh ();
        return t_fun (a, t_fun (t_list (a), t_list (a)));
      }
    auto a = fresh (), b = fresh (), c = fresh ();
    if (op == ">>")
      return t_fun (t_fun (a, b), t_fun (t_fun (b, c), t_fun (a, c)));
    if (op == "|>")
      return t_fun (a, t_fun (t_fun (a, b), b));
    // << and ∘
    return t_fun (t_fun (b, c), t_fun (t_fun (a, b), t_fun (a, c)));
  }

  // ---- patterns ----
  void
  bind_local (const ScopeP &scope, const std::string &name, const TypeP &t)
  {
    scope->vars.set (name, VarInfo{ t, nullptr });
  }

  void
  check_pat (const PatP &p, const TypeP &t, const ScopeP &scope,
             std::set<std::string> &names)
  {
    switch (p->kind)
      {
      case PK::Wild:
        return;
      case PK::Var:
        if (!names.insert (p->text).second)
          error ("M0208", "the name `" + p->text + "` is bound twice in this pattern",
                 p->span);
        bind_local (scope, p->text, t);
        return;
      case PK::Int:
        unify_or_error (t, t_int (), p->span, "in this pattern");
        return;
      case PK::Float:
        unify_or_error (t, t_float (), p->span, "in this pattern");
        return;
      case PK::Str:
        unify_or_error (t, t_string (), p->span, "in this pattern");
        return;
      case PK::Bool:
        unify_or_error (t, t_bool (), p->span, "in this pattern");
        return;
      case PK::Unit:
        unify_or_error (t, t_unit (), p->span, "in this pattern");
        return;
      case PK::Tuple:
        {
          std::vector<TypeP> items;
          for (std::size_t i = 0; i < p->kids.size (); ++i)
            items.push_back (fresh ());
          unify_or_error (t, t_tuple (items), p->span, "in this pattern");
          for (std::size_t i = 0; i < p->kids.size (); ++i)
            check_pat (p->kids[i], items[i], scope, names);
          return;
        }
      case PK::List:
        {
          auto el = fresh ();
          unify_or_error (t, t_list (el), p->span, "in this pattern");
          for (auto &k : p->kids)
            check_pat (k, el, scope, names);
          return;
        }
      case PK::Cons:
        {
          auto el = fresh ();
          unify_or_error (t, t_list (el), p->span, "in this pattern");
          check_pat (p->kids[0], el, scope, names);
          check_pat (p->kids[1], t_list (el), scope, names);
          return;
        }
      case PK::Ctor:
        {
          auto ci = lookup_ctor (p->text, p->path, scope, p->span);
          p->ctor_index = ci->index;
          p->ctor_arity = ci->arity;
          auto ty = instantiate (ci->scheme);
          std::vector<TypeP> args;
          for (int i = 0; i < ci->arity; ++i)
            {
              auto f = prune (ty);
              args.push_back (f->args[0]);
              ty = f->args[1];
            }
          std::vector<PatP> kids = p->kids;
          if (ci->arity == 1 && kids.size () > 1)
            {
              auto tup = std::make_shared<Pat> ();
              tup->kind = PK::Tuple;
              tup->span = p->span;
              tup->kids = kids;
              kids = { tup };
              p->kids = kids;
            }
          if (static_cast<int> (kids.size ()) != ci->arity)
            error ("M0304", "constructor `" + p->text + "` expects "
                                + std::to_string (ci->arity)
                                + " argument(s) but the pattern has "
                                + std::to_string (kids.size ()),
                   p->span);
          unify_or_error (t, ty, p->span, "in this pattern");
          for (std::size_t i = 0; i < kids.size (); ++i)
            check_pat (kids[i], args[i], scope, names);
          return;
        }
      case PK::Record:
        {
          TypeP row = fresh ();
          std::vector<TypeP> fts;
          for (std::size_t i = p->kids.size (); i-- > 0;)
            {
              auto ft = fresh ();
              fts.insert (fts.begin (), ft);
              row = row_ext (p->labels[i], ft, row);
            }
          unify_or_error (t, t_record (row), p->span, "in this pattern");
          for (std::size_t i = 0; i < p->kids.size (); ++i)
            check_pat (p->kids[i], fts[i], scope, names);
          return;
        }
      case PK::Annot:
        {
          TVarMap m;
          auto at = resolve_type (p->annot, scope, m);
          unify_or_error (at, t, p->span, "in this annotated pattern");
          check_pat (p->kids[0], at, scope, names);
          return;
        }
      }
  }

  CtorInfoP
  lookup_ctor (const std::string &name, const std::vector<std::string> &path,
               const ScopeP &scope, const Span &sp)
  {
    if (!path.empty ())
      {
        auto ms = resolve_mpath (path, scope, sp);
        if (auto c = ms->ctors.find (name))
          return *c;
        error ("M0209", "module `" + detail::join (path, ".") + "` has no constructor `"
                            + name + "`",
               sp);
      }
    if (auto c = scope->ctor (name))
      return *c;
    if (scope->module (name))
      error ("M0209", "module `" + name + "` cannot be used as a value", sp);
    std::vector<std::string> cands;
    for (const Scope *s = scope.get (); s; s = s->parent.get ())
      for (auto &k : s->ctors.keys ())
        cands.push_back (k);
    auto near = closest (name, cands);
    error ("M0209", "unbound constructor `" + name + "`", sp,
           near.empty () ? std::vector<std::string>{}
                         : std::vector<std::string>{ "did you mean `" + near + "`?" });
  }

  // ---- expressions ----
  static bool
  is_concat (const ExprP &e)
  {
    if ((e->kind == EK::App || e->kind == EK::Pipe) && e->resolution != Resolution::Apply)
      return true;
    return e->kind == EK::BinOp
           && (e->text == ">>" || e->text == "<<" || e->text == "\xE2\x88\x98");
  }

  TypeP
  infer_var (const ExprP &e, const ScopeP &scope)
  {
    const std::string &n = e->kind == EK::Tacit ? e->text : e->text;
    auto vi = scope->var (n);
    if (!vi)
      {
        if (e->kind == EK::Tacit)
          error ("M0206", "tacit parameter `" + n + "` is not bound here", e->span);
        auto near = closest (n, visible_vars (scope));
        std::vector<std::string> sugg;
        if (!near.empty ())
          sugg.push_back ("did you mean `" + near + "`?");
        error ("M0204", "unbound variable `" + n + "`", e->span, sugg);
      }
    e->cell = vi->cell;
    return instantiate (vi->scheme);
  }

  TypeP
  infer_path (const ExprP &e, const ScopeP &scope)
  {
    std::vector<std::string> mp (e->path.begin (), e->path.end () - 1);
    const std::string &field = e->path.back ();
    e->prim = nullptr;
    e->cell = nullptr;
    auto m = scope->module (mp[0]);
    std::string full = detail::join (e->path, ".");
    if (m)
      {
        ScopeP cur = *m;
        bool ok = true;
        for (std::size_t i = 1; i < mp.size (); ++i)
          {
            auto n = cur->modules.find (mp[i]);
            if (!n)
              {
                ok = false;
                break;
              }
            cur = *n;
          }
        if (ok)
          if (auto vi = cur->vars.find (field))
            return instantiate (vi->scheme);
      }
    if (find_prim)
      if (Primitive *p = find_prim (full))
        {
          e->prim = p;
          return instantiate (prim_type (p, e->span));
        }
    if (!m)
      error ("M0501", "unbound module `" + mp[0] + "` (and no host primitive `" + full + "`)",
             e->span);
    resolve_mpath (mp, scope, e->span);
    auto ms = resolve_mpath (mp, scope, e->span);
    auto near = closest (field, ms->vars.keys ());
    error ("M0506", "module `" + detail::join (mp, ".") + "` has no member `" + field + "`",
           e->span,
           near.empty () ? std::vector<std::string>{}
                         : std::vector<std::string>{ "did you mean `" + near + "`?" });
  }

  void
  lambda_warnings (const ExprP &e)
  {
    if (quiet || e->tacit_count > 0)
      return;
    std::set<std::string> fv;
    detail::ParserImpl::free_vars (e->kids[0], {}, fv);
    for (auto &p : e->pats)
      {
        std::set<std::string> names;
        detail::ParserImpl::pat_names (p, names);
        for (auto &n : names)
          if (n[0] != '_' && n[0] != '#' && n[0] != '$' && !fv.count (n))
            warn ("M0307", "unused parameter `" + n + "`", p->span);
      }
  }

  /// Binds lambda parameters (optionally against known types) in `inner`.
  std::vector<TypeP>
  bind_params (const ExprP &e, const ScopeP &inner, const std::vector<TypeP> &known)
  {
    std::set<std::string> names;
    std::vector<TypeP> pts;
    for (std::size_t i = 0; i < e->pats.size (); ++i)
      {
        auto pt = i < known.size () ? known[i] : fresh ();
        check_pat (e->pats[i], pt, inner, names);
        pts.push_back (pt);
      }
    for (int k = 1; k <= e->tacit_count && k <= static_cast<int> (pts.size ()); ++k)
      bind_local (inner, "$" + std::to_string (k), pts[static_cast<std::size_t> (k - 1)]);
    return pts;
  }

  TypeP
  infer (const ExprP &e, const ScopeP &scope)
  {
    switch (e->kind)
      {
      case EK::Int:
        return t_int ();
      case EK::Float:
        return t_float ();
      case EK::Str:
        return t_string ();
      case EK::Bool:
        return t_bool ();
      case EK::Unit:
        return t_unit ();
      case EK::Var:
        return infer_var (e, scope);
      case EK::Tacit:
        {
          e->text = "$" + std::to_string (e->ival);
          return infer_var (e, scope);
        }
      case EK::Path:
        return infer_path (e, scope);
      case EK::Ctor:
        {
          auto ci = lookup_ctor (e->text, e->path, scope, e->span);
          e->ctor_index = ci->index;
          e->ctor_arity = ci->arity;
          return instantiate (ci->scheme);
        }
      case EK::App:
        {
          auto tf = infer (e->kids[0], scope);
          auto tx = infer (e->kids[1], scope);
          return resolve_juxtaposition (e, tf, tx, e->kids[0], e->kids[1], false);
        }
      case EK::Pipe:
        {
          auto ta = infer (e->kids[0], scope);
          auto tb = infer (e->kids[1], scope);
          return resolve_juxtaposition (e, tb, ta, e->kids[1], e->kids[0], true);
        }
      case EK::BinOp:
        {
          TypeP ft;
          if (detail::is_builtin_op (e->text))
            ft = builtin_op_type (e->text);
          else
            {
              auto vi = scope->var (e->text);
              if (!vi)
                error ("M0204", "unbound operator `" + e->text + "`", e->span);
              ft = instantiate (vi->scheme);
            }
          auto ta = infer (e->kids[0], scope);
          auto tb = infer (e->kids[1], scope);
          auto r = fresh ();
          auto f = prune (ft);
          if (is_fun (f))
            {
              unify_or_error (f->args[0], ta, e->kids[0]->span,
                              "in the left operand of `" + e->text + "`");
              auto g = prune (f->args[1]);
              if (is_fun (g))
                {
                  unify_or_error (g->args[0], tb, e->kids[1]->span,
                                  "in the right operand of `" + e->text + "`");
                  return g->args[1];
                }
            }
          unify_or_error (ft, t_fun (ta, t_fun (tb, r)), e->span,
                          "in the operator `" + e->text + "`");
          return r;
        }
      case EK::Neg:
        {
          auto t = infer (e->kids[0], scope);
          auto n = fresh_num ();
          unify_or_error (n, t, e->kids[0]->span, "in this negation");
          return n;
        }
      case EK::Lambda:
        {
          auto inner = Scope::child (scope);
          auto pts = bind_params (e, inner, {});
          lambda_warnings (e);
          TypeP t = infer (e->kids[0], inner);
          for (std::size_t i = pts.size (); i-- > 0;)
            t = t_fun (pts[i], t);
          return t;
        }
      case EK::If:
        {
          unify_or_error (t_bool (), infer (e->kids[0], scope), e->kids[0]->span,
                          "in the condition of `if`");
          auto a = infer (e->kids[1], scope);
          auto b = infer (e->kids[2], scope);
          unify_or_error (a, b, e->kids[2]->span,
                          "the `else` branch must have the same type as the `then` branch");
          return a;
        }
      case EK::Match:
        {
          auto ts = infer (e->kids[0], scope);
          auto r = fresh ();
          for (std::size_t i = 0; i < e->pats.size (); ++i)
            {
              auto inner = Scope::child (scope);
              std::set<std::string> names;
              check_pat (e->pats[i], ts, inner, names);
              if (e->guards[i])
                unify_or_error (t_bool (), infer (e->guards[i], inner), e->guards[i]->span,
                                "in this guard");
              auto tb = infer (e->kids[i + 1], inner);
              unify_or_error (r, tb, e->kids[i + 1]->span,
                              "all branches of `match` must have the same type");
            }
          return r;
        }
      case EK::Let:
        {
          ++level;
          auto t1 = infer (e->kids[0], scope);
          --level;
          auto inner = Scope::child (scope);
          std::set<std::string> names;
          check_pat (e->pats[0], t1, inner, names);
          for (auto &n : inner->vars.keys ())
            generalize (inner->vars.find (n)->scheme, level);
          return infer (e->kids[1], inner);
        }
      case EK::LetRec:
        {
          auto inner = Scope::child (scope);
          ++level;
          std::vector<TypeP> monos;
          for (auto &p : e->pats)
            {
              auto m = fresh ();
              monos.push_back (m);
              bind_local (inner, p->text, m);
            }
          for (std::size_t i = 0; i < e->pats.size (); ++i)
            unify_or_error (monos[i], infer (e->kids[i], inner), e->kids[i]->span,
                            "in the recursive binding `" + e->pats[i]->text + "`");
          --level;
          for (auto &m : monos)
            generalize (m, level);
          return infer (e->kids.back (), inner);
        }
      case EK::List:
        {
          auto el = fresh ();
          for (auto &k : e->kids)
            unify_or_error (el, infer (k, scope), k->span,
                            "list elements must all have the same type");
          return t_list (el);
        }
      case EK::Comp:
        {
          auto inner = Scope::child (scope);
          for (auto &c : e->clauses)
            {
              if (!c.pat)
                {
                  unify_or_error (t_bool (), infer (c.expr, inner), c.expr->span,
                                  "in this comprehension guard");
                  continue;
                }
              auto el = fresh ();
              unify_or_error (t_list (el), infer (c.expr, inner), c.expr->span,
                              "a comprehension generator must be a list");
              auto next = Scope::child (inner);
              std::set<std::string> names;
              check_pat (c.pat, el, next, names);
              inner = next;
            }
          return t_list (infer (e->kids[0], inner));
        }
      case EK::Range:
        unify_or_error (t_int (), infer (e->kids[0], scope), e->kids[0]->span,
                        "in the range start");
        unify_or_error (t_int (), infer (e->kids[1], scope), e->kids[1]->span,
                        "in the range end");
        return t_list (t_int ());
      case EK::Tuple:
        {
          std::vector<TypeP> items;
          for (auto &k : e->kids)
            items.push_back (infer (k, scope));
          return t_tuple (std::move (items));
        }
      case EK::Record:
        {
          TypeP row = row_empty ();
          std::vector<TypeP> fts;
          for (auto &k : e->kids)
            fts.push_back (infer (k, scope));
          for (std::size_t i = e->kids.size (); i-- > 0;)
            row = row_ext (e->labels[i], fts[i], row);
          return t_record (row);
        }
      case EK::Update:
        {
          auto tb = infer (e->kids[0], scope);
          for (std::size_t i = 0; i < e->labels.size (); ++i)
            {
              auto ft = fresh ();
              unify_or_error (t_record (row_ext (e->labels[i], ft, fresh ())), tb,
                              e->kids[0]->span, "in this record update");
              unify_or_error (ft, infer (e->kids[i + 1], scope), e->kids[i + 1]->span,
                              "in the field `" + e->labels[i] + "`");
            }
          return tb;
        }
      case EK::Project:
        {
          auto t = infer (e->kids[0], scope);
          auto ft = fresh ();
          unify_or_error (t_record (row_ext (e->text, ft, fresh ())), t, e->kids[0]->span,
                          "in the projection `." + e->text + "`");
          return ft;
        }
      case EK::OpRef:
        {
          if (detail::is_builtin_op (e->text))
            return builtin_op_type (e->text);
          auto vi = scope->var (e->text);
          if (!vi)
            error ("M0204", "unbound operator `" + e->text + "`", e->span);
          e->cell = vi->cell;
          return instantiate (vi->scheme);
        }
      case EK::Annot:
        {
          TVarMap m;
          auto at = resolve_type (e->annot, scope, m);
          unify_or_error (at, infer (e->kids[0], scope), e->kids[0]->span,
                          "in this type annotation");
          return at;
        }
      }
    return t_unit ();
  }

  /// §5.4: `f x` is application unless types require composition.
  TypeP
  resolve_juxtaposition (const ExprP &e, const TypeP &tf, const TypeP &tx,
                         const ExprP &fe, const ExprP &xe, bool pipe)
  {
    {
      auto snap = u.snapshot ();
      auto r = fresh ();
      if (u.unify (tf, t_fun (tx, r)))
        {
          e->resolution = Resolution::Apply;
          return r;
        }
      u.rollback (snap);
    }
    if (is_fun (tf) && is_fun (tx))
      {
        // forward: left-hand side first (pipe: x then f; juxtaposition: f then x)
        const TypeP &first = pipe ? tx : tf;
        const TypeP &second = pipe ? tf : tx;
        auto a = fresh (), b = fresh (), c = fresh ();
        auto snap = u.snapshot ();
        if (u.unify (first, t_fun (a, b)) && u.unify (second, t_fun (b, c)))
          {
            e->resolution = Resolution::ComposeFwd;
            return t_fun (a, c);
          }
        u.rollback (snap);
        if (!pipe)
          {
            snap = u.snapshot ();
            if (u.unify (second, t_fun (a, b)) && u.unify (first, t_fun (b, c)))
              {
                e->resolution = Resolution::ComposeBwd;
                return t_fun (a, c);
              }
            u.rollback (snap);
          }
      }
    auto f = prune (tf);
    if (is_fun (f))
      {
        unify_or_error (f->args[0], tx, xe->span,
                        pipe ? "in the value piped by `|>`" : "in this argument");
        error ("M0301", "cannot apply this function", e->span);
      }
    TypePrinter p;
    error ("M0301",
           std::string (pipe ? "the right side of `|>`" : "this expression")
               + " has type `" + p.print_bare (tf) + "` and cannot be applied; it is not a function",
           fe->span);
  }

  // ---- definitions ----
  [[noreturn]] void
  sig_mismatch (const ItemP &def, const std::string &decl_text, const TypeP &actual)
  {
    std::string code = u.reason_code, reason = u.reason;
    TypePrinter p;
    std::string msg = "`" + def->name + "` is declared as `" + decl_text
                      + "` but its definition has type `" + p.print (actual) + "`";
    if ((code == "M0302" || code == "M0303" || code == "M0306" || code == "M0309")
        && !reason.empty ())
      msg += "; " + reason;
    error ("M0305", msg, def->span);
  }

  /// Checks a definition against its declared signature (bidirectional;
  /// concat bodies may take the stack top as an extra argument, §5.4).
  TypeP
  check_def_with_sig (const ItemP &def, const TypeP &scheme, const ScopeP &scope,
                      const TypeExprP &sig_te)
  {
    auto ts = skolemize (scheme);
    const ExprP &v = def->value;
    std::string decl_text;
    {
      TypePrinter p;
      decl_text = p.print (ts);
    }
    (void)sig_te;
    if (v->kind == EK::Lambda)
      {
        auto inner = Scope::child (scope);
        std::vector<TypeP> known;
        TypeP rest = ts;
        bool short_sig = false;
        for (std::size_t i = 0; i < v->pats.size (); ++i)
          {
            auto r = prune (rest);
            if (is_fun (r))
              {
                known.push_back (r->args[0]);
                rest = r->args[1];
              }
            else
              {
                short_sig = true;
                break;
              }
          }
        auto pts = bind_params (v, inner, known);
        lambda_warnings (v);
        auto body = infer (v->kids[0], inner);
        TypeP full = body;
        for (std::size_t i = pts.size (); i-- > 0;)
          full = t_fun (pts[i], full);
        auto snap = u.snapshot ();
        if (!short_sig)
          {
            if (u.unify (rest, body))
              return ts;
            u.rollback (snap);
            auto bp = prune (body);
            if (is_concat (v->kids[0]) && is_fun (bp))
              {
                if (u.unify (rest, bp->args[1]))
                  return full;
                u.rollback (snap);
              }
            u.unify (rest, body);
            u.rollback (snap);
            sig_mismatch (def, decl_text, full);
          }
        if (u.unify (ts, full))
          return ts;
        u.rollback (snap);
        u.unify (ts, full);
        u.rollback (snap);
        sig_mismatch (def, decl_text, full);
      }
    auto vt = infer (v, scope);
    auto snap = u.snapshot ();
    if (u.unify (ts, vt))
      return ts;
    u.rollback (snap);
    auto vp = prune (vt);
    if (is_concat (v) && is_fun (vp))
      {
        if (u.unify (ts, vp->args[1]))
          return vt;
        u.rollback (snap);
      }
    u.unify (ts, vt);
    u.rollback (snap);
    sig_mismatch (def, decl_text, vt);
  }

  // ---- units & items ----
  /// Splits items into checking units (def segments, type groups, singles).
  static std::vector<std::pair<std::size_t, std::size_t>>
  units (const std::vector<ItemP> &items)
  {
    std::vector<std::pair<std::size_t, std::size_t>> out;
    std::size_t i = 0;
    while (i < items.size ())
      {
        IK k = items[i]->kind;
        std::size_t j = i + 1;
        if (k == IK::Def || k == IK::Sig)
          {
            std::set<std::string> defs;
            if (k == IK::Def)
              defs.insert (items[i]->name);
            while (j < items.size ()
                   && (items[j]->kind == IK::Def || items[j]->kind == IK::Sig))
              {
                if (items[j]->kind == IK::Def && !defs.insert (items[j]->name).second)
                  break;
                if (items[j]->kind == IK::Sig && defs.count (items[j]->name))
                  break;
                ++j;
              }
          }
        else if (k == IK::Type)
          while (j < items.size () && items[j]->kind == IK::Type)
            ++j;
        out.push_back ({ i, j });
        i = j;
      }
    return out;
  }

  /// Checks one unit. `exports` receives exported members (structures);
  /// `global` creates runtime cells for top-level bindings.
  UnitInfo
  check_unit (const std::vector<ItemP> &items, std::size_t b, std::size_t e,
              const ScopeP &scope, const ScopeP &exports, bool global)
  {
    UnitInfo info;
    IK k = items[b]->kind;
    if (k == IK::Def || k == IK::Sig)
      return check_defs (items, b, e, scope, exports, global);
    if (k == IK::Type)
      {
        check_types (items, b, e, scope, exports);
        return info;
      }
    const ItemP &it = items[b];
    switch (k)
      {
      case IK::Module:
        {
          std::string prefix = scope->prefix + it->name + ".";
          auto s = check_mexpr (it->mexpr, scope, prefix);
          if (it->mexpr->kind == MK::Apply)
            {
              // a functor application inherits the functor's sealing
              auto fi = find_functor (it->mexpr->path, scope, it->mexpr->span);
              (void)fi;
            }
          if (it->asc != Asc::None)
            {
              s = check_ascription (s, it->sig, scope, it->asc, prefix, it->span);
              it->sealed = it->asc == Asc::Opaque;
              it->visible_values = s->vars.keys ();
              it->visible_modules = s->modules.keys ();
            }
          scope->modules.set (it->name, s);
          if (exports)
            exports->modules.set (it->name, s);
          return info;
        }
      case IK::Functor:
        {
          auto param = sig_to_module (it->psig, scope, it->pname + ".", it->span);
          auto env2 = Scope::child (scope, scope->prefix + it->name + ".");
          env2->modules.set (it->pname, param);
          auto s = check_struct (it->mexpr, env2, scope->prefix + it->name + ".");
          if (it->asc != Asc::None)
            {
              s = check_ascription (s, it->sig, env2, it->asc, scope->prefix + it->name + ".",
                                    it->span);
              it->sealed = it->asc == Asc::Opaque;
              it->visible_values = s->vars.keys ();
              it->visible_modules = s->modules.keys ();
            }
          auto fi = std::make_shared<FunctorInfo> ();
          fi->name = it->name;
          fi->item = it;
          fi->env = scope;
          scope->functors.set (it->name, fi);
          if (exports)
            exports->functors.set (it->name, fi);
          return info;
        }
      case IK::ModType:
        {
          auto sd = std::make_shared<SigDef> ();
          sd->name = it->name;
          sd->expr = it->sig;
          sd->env = scope;
          // validate eagerly against an abstract instance
          sig_to_module (it->sig, scope, it->name + ".", it->span);
          scope->sigs.set (it->name, sd);
          if (exports)
            exports->sigs.set (it->name, sd);
          return info;
        }
      case IK::Include:
      case IK::Open:
        {
          auto s = resolve_mpath (it->path, scope, it->span);
          bool inc = k == IK::Include;
          it->cells.clear ();
          for (auto &n : s->vars.keys ())
            {
              VarInfo vi = *s->vars.find (n);
              vi.cell = nullptr;
              if (global)
                {
                  vi.cell = std::make_shared<Cell> ();
                  vi.cell->name = n;
                  it->cells.push_back (vi.cell);
                }
              scope->vars.set (n, vi);
              if (inc && exports)
                exports->vars.set (n, vi);
            }
          for (auto &n : s->types.keys ())
            {
              scope->types.set (n, *s->types.find (n));
              if (inc && exports)
                exports->types.set (n, *s->types.find (n));
            }
          for (auto &n : s->ctors.keys ())
            {
              scope->ctors.set (n, *s->ctors.find (n));
              if (inc && exports)
                exports->ctors.set (n, *s->ctors.find (n));
            }
          for (auto &n : s->modules.keys ())
            {
              scope->modules.set (n, *s->modules.find (n));
              if (inc && exports)
                exports->modules.set (n, *s->modules.find (n));
            }
          for (auto &n : s->functors.keys ())
            {
              scope->functors.set (n, *s->functors.find (n));
              if (inc && exports)
                exports->functors.set (n, *s->functors.find (n));
            }
          for (auto &n : s->sigs.keys ())
            scope->sigs.set (n, *s->sigs.find (n));
          return info;
        }
      case IK::Import:
        if (!global)
          error ("M0505", "`import` is only allowed at the top level", it->span);
        return info;
      case IK::Directive:
        if (!global)
          error ("M0400", "directive `#" + it->name + "` is not allowed inside a structure",
                 it->span);
        return info;
      case IK::Expr:
        {
          ++level;
          auto t = infer (it->body, scope);
          --level;
          generalize (t, level);
          info.expr_type = t;
          return info;
        }
      default:
        break;
      }
    return info;
  }

  UnitInfo
  check_defs (const std::vector<ItemP> &items, std::size_t b, std::size_t e,
              const ScopeP &scope, const ScopeP &exports, bool global)
  {
    UnitInfo info;
    std::vector<ItemP> defs;
    std::map<std::string, ItemP> sigs;
    for (std::size_t i = b; i < e; ++i)
      {
        if (items[i]->kind == IK::Sig)
          sigs[items[i]->name] = items[i];
        else
          defs.push_back (items[i]);
      }
    std::set<std::string> defined;
    for (auto &d : defs)
      defined.insert (d->name);
    for (std::size_t i = b; i < e; ++i)
      if (items[i]->kind == IK::Sig && !defined.count (items[i]->name))
        {
          // validate now, attach to a later definition
          sig_scheme (items[i]->type, scope);
          scope->pending.set (items[i]->name, items[i]);
        }
    // signature type expressions per definition
    std::vector<TypeExprP> sig_te (defs.size ());
    for (std::size_t i = 0; i < defs.size (); ++i)
      {
        auto &d = defs[i];
        if (d->type)
          sig_te[i] = d->type;
        else if (sigs.count (d->name))
          sig_te[i] = sigs[d->name]->type;
        else
          for (const Scope *s = scope.get (); s; s = s->parent.get ())
            if (auto p = s->pending.find (d->name))
              {
                sig_te[i] = (*p)->type;
                const_cast<Scope *> (s)->pending.erase (d->name);
                break;
              }
      }
    // dependency graph & strongly connected components (DSLtk DirectedGraph)
    dsl::DirectedGraph<std::string, std::string> g;
    std::map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < defs.size (); ++i)
      {
        g.add_node (defs[i]->name);
        index[defs[i]->name] = i;
      }
    std::vector<std::set<std::size_t>> deps (defs.size ());
    for (std::size_t i = 0; i < defs.size (); ++i)
      {
        std::set<std::string> fv;
        detail::ParserImpl::free_vars (defs[i]->value, {}, fv);
        for (auto &n : fv)
          {
            auto it = index.find (n);
            if (it != index.end ())
              {
                g.add_edge (i, it->second);
                deps[i].insert (it->second);
              }
          }
      }
    auto comps = g.strongly_connected_components ();
    std::vector<std::size_t> comp_of (defs.size ());
    std::vector<std::vector<std::size_t>> groups;
    for (auto &c : comps)
      {
        std::vector<std::size_t> grp (c.begin (), c.end ());
        std::sort (grp.begin (), grp.end ());
        for (auto n : grp)
          comp_of[n] = groups.size ();
        groups.push_back (grp);
      }
    std::vector<bool> done (groups.size (), false);
    std::vector<std::vector<std::size_t>> order;
    for (std::size_t round = 0; round < groups.size (); ++round)
      {
        std::size_t pick = groups.size ();
        for (std::size_t gi = 0; gi < groups.size (); ++gi)
          {
            if (done[gi])
              continue;
            bool ready = true;
            for (auto n : groups[gi])
              for (auto d : deps[n])
                if (comp_of[d] != gi && !done[comp_of[d]])
                  ready = false;
            if (ready && (pick == groups.size () || groups[gi][0] < groups[pick][0]))
              pick = gi;
          }
        done[pick] = true;
        order.push_back (groups[pick]);
      }
    std::vector<TypeP> final_types (defs.size ());
    for (auto &grp : order)
      {
        ++level;
        std::vector<TypeP> schemes (defs.size ()), monos (defs.size ());
        for (auto i : grp)
          {
            auto &d = defs[i];
            CellP cell;
            if (global)
              {
                cell = std::make_shared<Cell> ();
                cell->name = d->name;
                d->cells = { cell };
              }
            if (sig_te[i])
              {
                --level;
                schemes[i] = sig_scheme (sig_te[i], scope);
                ++level;
                scope->vars.set (d->name, VarInfo{ schemes[i], cell });
              }
            else
              {
                monos[i] = fresh ();
                scope->vars.set (d->name, VarInfo{ monos[i], cell });
              }
          }
        std::vector<TypeP> results (defs.size ());
        for (auto i : grp)
          {
            auto &d = defs[i];
            if (sig_te[i])
              results[i] = check_def_with_sig (d, schemes[i], scope, sig_te[i]);
            else
              {
                auto t = infer (d->value, scope);
                unify_or_error (monos[i], t, d->span,
                                "in the recursive use of `" + d->name + "`");
                results[i] = monos[i];
              }
          }
        --level;
        for (auto i : grp)
          {
            generalize (results[i], level);
            auto cell = global ? defs[i]->cells[0] : nullptr;
            scope->vars.set (defs[i]->name, VarInfo{ results[i], cell });
            if (exports)
              exports->vars.set (defs[i]->name, VarInfo{ results[i], nullptr });
            final_types[i] = results[i];
          }
      }
    for (std::size_t i = 0; i < defs.size (); ++i)
      info.defs.push_back ({ defs[i]->name, final_types[i] });
    return info;
  }

  void
  check_types (const std::vector<ItemP> &items, std::size_t b, std::size_t e,
               const ScopeP &scope, const ScopeP &exports)
  {
    std::vector<TypeDefP> defs;
    for (std::size_t i = b; i < e; ++i)
      {
        auto &it = items[i];
        auto d = std::make_shared<TypeDef> ();
        d->name = it->name;
        d->display = scope->prefix + it->name;
        d->stamp = next_stamp++;
        d->alias = it->tkind == 1;
        std::set<std::string> seen;
        for (auto &p : it->tparams)
          {
            if (!seen.insert (p).second)
              error ("M0208", "duplicate type parameter `" + p + "`", it->span);
            auto v = generic_var ();
            v->hint = p;
            d->params.push_back (v);
          }
        scope->types.set (it->name, d);
        if (exports)
          exports->types.set (it->name, d);
        defs.push_back (d);
      }
    for (std::size_t i = b; i < e; ++i)
      {
        auto &it = items[i];
        auto &d = defs[i - b];
        TVarMap m;
        m.allow_new = false;
        for (std::size_t k = 0; k < it->tparams.size (); ++k)
          m.vars[it->tparams[k]] = d->params[k];
        if (it->tkind == 1)
          d->body = resolve_type (it->type, scope, m);
        else if (it->tkind == 2)
          {
            std::set<std::string> seen;
            for (std::size_t c = 0; c < it->ctors.size (); ++c)
              {
                auto &cd = it->ctors[c];
                if (!seen.insert (cd.name).second)
                  error ("M0208", "duplicate constructor `" + cd.name + "`", cd.span);
                std::vector<TypeP> args;
                for (auto &a : cd.args)
                  args.push_back (resolve_type (a, scope, m));
                TypeP t = t_con (d->display, d->stamp, d->params);
                for (std::size_t k = args.size (); k-- > 0;)
                  t = t_fun (args[k], t);
                auto ci = std::make_shared<CtorInfo> ();
                ci->name = cd.name;
                ci->index = static_cast<int> (c);
                ci->arity = static_cast<int> (args.size ());
                ci->scheme = t;
                ci->owner = d;
                d->ctors.push_back (ci);
                scope->ctors.set (cd.name, ci);
                if (exports)
                  exports->ctors.set (cd.name, ci);
              }
          }
      }
  }

  // ---- modules ----
  ScopeP
  check_struct (const ModExprP &m, const ScopeP &env, const std::string &prefix)
  {
    auto inner = Scope::child (env, prefix);
    auto exports = std::make_shared<Scope> ();
    exports->prefix = prefix;
    for (auto &[b, e] : units (m->items))
      check_unit (m->items, b, e, inner, exports, false);
    for (auto &n : inner->pending.keys ())
      error ("M0502", "`" + n + "` is declared in this structure but never defined",
             (*inner->pending.find (n))->span);
    return exports;
  }

  FunctorInfoP
  find_functor (const std::vector<std::string> &path, const ScopeP &scope, const Span &sp)
  {
    if (path.size () == 1)
      {
        if (auto f = scope->functor (path[0]))
          return *f;
        if (scope->module (path[0]))
          error ("M0503", "module `" + path[0] + "` is not a functor", sp);
        error ("M0503", "unbound functor `" + path[0] + "`", sp);
      }
    auto ms = resolve_mpath (std::vector<std::string> (path.begin (), path.end () - 1),
                             scope, sp);
    if (auto f = ms->functors.find (path.back ()))
      return *f;
    error ("M0503", "unbound functor `" + detail::join (path, ".") + "`", sp);
  }

  ScopeP
  check_mexpr (const ModExprP &m, const ScopeP &scope, const std::string &prefix)
  {
    switch (m->kind)
      {
      case MK::Struct:
        return check_struct (m, scope, prefix);
      case MK::Path:
        return resolve_mpath (m->path, scope, m->span);
      case MK::Apply:
        {
          auto fi = find_functor (m->path, scope, m->span);
          auto arg = check_mexpr (m->arg, scope, prefix);
          auto &fit = fi->item;
          check_ascription (arg, fit->psig, fi->env, Asc::Transparent, prefix, m->arg->span);
          auto env2 = Scope::child (fi->env, prefix);
          env2->modules.set (fit->pname, arg);
          bool q = quiet;
          quiet = true;
          ScopeP s;
          try
            {
              s = check_struct (fit->mexpr, env2, prefix);
              if (fit->asc != Asc::None)
                s = check_ascription (s, fit->sig, env2, fit->asc, prefix, fit->span);
            }
          catch (...)
            {
              quiet = q;
              throw;
            }
          quiet = q;
          return s;
        }
      }
    return nullptr;
  }

  std::pair<std::vector<ItemP>, ScopeP>
  get_sig (const SigExprP &s, const ScopeP &env, const Span &sp)
  {
    if (!s->named)
      return { s->items, env };
    const SigDefP *sd = nullptr;
    if (s->path.size () == 1)
      sd = env->sig (s->path[0]);
    else
      {
        auto ms = resolve_mpath (std::vector<std::string> (s->path.begin (), s->path.end () - 1),
                                 env, sp);
        sd = ms->sigs.find (s->path.back ());
      }
    if (!sd)
      error ("M0504", "unbound signature `" + detail::join (s->path, ".") + "`", s->span);
    return { (*sd)->expr->items, (*sd)->env };
  }

  /// A module matching `sig` with abstract types (functor parameters).
  ScopeP
  sig_to_module (const SigExprP &sig, const ScopeP &env, const std::string &prefix,
                 const Span &sp)
  {
    auto [items, senv] = get_sig (sig, env, sp);
    auto local = Scope::child (senv, prefix);
    auto r = std::make_shared<Scope> ();
    r->prefix = prefix;
    for (auto &it : items)
      {
        if (it->kind == IK::Type)
          {
            auto d = std::make_shared<TypeDef> ();
            d->name = it->name;
            d->display = prefix + it->name;
            d->stamp = next_stamp++;
            for (auto &p : it->tparams)
              {
                auto v = generic_var ();
                v->hint = p;
                d->params.push_back (v);
              }
            TVarMap m;
            m.allow_new = false;
            for (std::size_t k = 0; k < it->tparams.size (); ++k)
              m.vars[it->tparams[k]] = d->params[k];
            if (it->tkind == 1)
              {
                d->alias = true;
                d->body = resolve_type (it->type, local, m);
              }
            else if (it->tkind == 2)
              {
                for (std::size_t c = 0; c < it->ctors.size (); ++c)
                  {
                    auto &cd = it->ctors[c];
                    std::vector<TypeP> args;
                    for (auto &a : cd.args)
                      args.push_back (resolve_type (a, local, m));
                    TypeP t = t_con (d->display, d->stamp, d->params);
                    for (std::size_t k = args.size (); k-- > 0;)
                      t = t_fun (args[k], t);
                    auto ci = std::make_shared<CtorInfo> ();
                    ci->name = cd.name;
                    ci->index = static_cast<int> (c);
                    ci->arity = static_cast<int> (args.size ());
                    ci->scheme = t;
                    ci->owner = d;
                    d->ctors.push_back (ci);
                    r->ctors.set (cd.name, ci);
                    local->ctors.set (cd.name, ci);
                  }
              }
            local->types.set (it->name, d);
            r->types.set (it->name, d);
          }
        else if (it->kind == IK::Sig)
          r->vars.set (it->name, VarInfo{ sig_scheme (it->type, local), nullptr });
      }
    return r;
  }

  /// Checks that module `s` matches `sig`; seals it for `:>`.
  ScopeP
  check_ascription (const ScopeP &s, const SigExprP &sig, const ScopeP &env, Asc asc,
                    const std::string &prefix, const Span &sp)
  {
    auto [items, senv] = get_sig (sig, env, sp);
    auto actual = Scope::child (senv, prefix);
    auto opaque = Scope::child (senv, prefix);
    auto r = std::make_shared<Scope> ();
    r->prefix = prefix;
    for (auto &it : items)
      {
        if (it->kind == IK::Type)
          {
            auto sd = s->types.find (it->name);
            if (!sd)
              error ("M0502", "the module does not define the type `" + it->name
                                  + "` required by the signature",
                     sp);
            if ((*sd)->params.size () != it->tparams.size ())
              error ("M0502", "type `" + it->name + "` has "
                                  + std::to_string ((*sd)->params.size ())
                                  + " parameter(s) but the signature expects "
                                  + std::to_string (it->tparams.size ()),
                     sp);
            if (it->tkind == 1)
              {
                TVarMap m;
                std::vector<TypeP> ps;
                for (auto &p : it->tparams)
                  {
                    auto v = fresh ();
                    m.vars[p] = v;
                    ps.push_back (v);
                  }
                auto want = resolve_type (it->type, actual, m);
                auto have = apply_typedef (*sd, ps, sp);
                auto snap = u.snapshot ();
                if (!u.unify (want, have))
                  {
                    u.rollback (snap);
                    TypePrinter p;
                    std::string hs = p.print_bare (have);
                    std::string ws = p.print_bare (want);
                    error ("M0502", "type `" + it->name + "` is `" + hs
                                        + "` in the module but the signature requires `" + ws
                                        + "`",
                           sp);
                  }
              }
            if (it->tkind == 2)
              {
                bool ok = (*sd)->ctors.size () == it->ctors.size ();
                for (std::size_t c = 0; ok && c < it->ctors.size (); ++c)
                  ok = (*sd)->ctors[c]->name == it->ctors[c].name;
                if (!ok)
                  error ("M0502", "the constructors of type `" + it->name
                                      + "` do not match the signature",
                         sp);
              }
            actual->types.set (it->name, *sd);
            if (asc == Asc::Opaque)
              {
                if (it->tkind == 0)
                  {
                    auto od = std::make_shared<TypeDef> ();
                    od->name = it->name;
                    od->display = prefix + it->name;
                    od->stamp = next_stamp++;
                    for (std::size_t k = 0; k < it->tparams.size (); ++k)
                      {
                        auto v = generic_var ();
                        v->hint = it->tparams[k];
                        od->params.push_back (v);
                      }
                    r->types.set (it->name, od);
                    opaque->types.set (it->name, od);
                  }
                else
                  {
                    r->types.set (it->name, *sd);
                    opaque->types.set (it->name, *sd);
                    for (auto &ci : (*sd)->ctors)
                      r->ctors.set (ci->name, ci);
                  }
              }
          }
        else if (it->kind == IK::Sig)
          {
            auto vi = s->vars.find (it->name);
            if (!vi)
              error ("M0502", "the module does not define the value `" + it->name
                                  + "` required by the signature",
                     sp);
            auto want = skolemize (sig_scheme (it->type, actual));
            auto have = instantiate (vi->scheme);
            auto snap = u.snapshot ();
            if (!u.unify (want, have))
              {
                u.rollback (snap);
                TypePrinter p1, p2;
                std::string hs = p1.print (instantiate (vi->scheme));
                std::string ws = p2.print (skolemize (sig_scheme (it->type, actual)));
                error ("M0502", "value `" + it->name + "` has type `" + hs
                                    + "` in the module but the signature requires `" + ws + "`",
                       sp);
              }
            if (asc == Asc::Opaque)
              r->vars.set (it->name, VarInfo{ sig_scheme (it->type, opaque), nullptr });
          }
      }
    return asc == Asc::Opaque ? r : s;
  }

  // ---- top-level transactions ----
  /// Moves the entries of a unit scope into `into` (REPL commit).
  static void
  commit_scope (const ScopeP &from, const ScopeP &into)
  {
    for (auto &k : from->vars.keys ())
      into->vars.set (k, *from->vars.find (k));
    for (auto &k : from->types.keys ())
      into->types.set (k, *from->types.find (k));
    for (auto &k : from->ctors.keys ())
      into->ctors.set (k, *from->ctors.find (k));
    for (auto &k : from->modules.keys ())
      into->modules.set (k, *from->modules.find (k));
    for (auto &k : from->functors.keys ())
      into->functors.set (k, *from->functors.find (k));
    for (auto &k : from->sigs.keys ())
      into->sigs.set (k, *from->sigs.find (k));
    for (auto &k : from->pending.keys ())
      into->pending.set (k, *from->pending.find (k));
  }
};

// ============================================================
//  Runtime objects, frames and the evaluator
// ============================================================

class Session;
class Interp;

struct StrObj : Obj
{
  std::string s;
  explicit StrObj (std::string v) : s (std::move (v)) {}
};

struct ConsObj : Obj
{
  Value head;
  std::shared_ptr<ConsObj> tail;
  ConsObj (Value h, std::shared_ptr<ConsObj> t) : head (std::move (h)), tail (std::move (t)) {}
  ~ConsObj () override
  {
    // iterative destruction: long lists must not overflow the C++ stack
    auto t = std::move (tail);
    while (t && t.use_count () == 1)
      {
        auto next = std::move (t->tail);
        t = std::move (next);
      }
  }
};
using ConsP = std::shared_ptr<ConsObj>;

using Labels = std::shared_ptr<const std::vector<std::string>>;

/// Tuples, records and constructor applications.
struct AggObj : Obj
{
  std::string tag{};          ///< constructor name
  std::vector<Value> items{};
  Labels labels{};            ///< record labels (sorted)
};

struct Frame;
using FrameP = std::shared_ptr<Frame>;
struct ModuleObj;
using ModuleP = std::shared_ptr<ModuleObj>;

struct FunctorObj : Obj
{
  ItemP item;
  FrameP env;
};
using FunctorP = std::shared_ptr<FunctorObj>;

/// Runtime environment frame; later bindings shadow earlier ones.
struct Frame
{
  std::vector<std::pair<std::string, Value>> vars{};
  std::vector<std::pair<std::string, ModuleP>> mods{};
  std::vector<std::pair<std::string, FunctorP>> functors{};
  FrameP parent{};

  static FrameP
  child (const FrameP &p)
  {
    auto f = std::make_shared<Frame> ();
    f->parent = p;
    return f;
  }
  const Value *
  find_var (const std::string &n) const
  {
    for (const Frame *f = this; f; f = f->parent.get ())
      for (std::size_t i = f->vars.size (); i-- > 0;)
        if (f->vars[i].first == n)
          return &f->vars[i].second;
    return nullptr;
  }
  ModuleP
  find_mod (const std::string &n) const
  {
    for (const Frame *f = this; f; f = f->parent.get ())
      for (std::size_t i = f->mods.size (); i-- > 0;)
        if (f->mods[i].first == n)
          return f->mods[i].second;
    return nullptr;
  }
  FunctorP
  find_functor (const std::string &n) const
  {
    for (const Frame *f = this; f; f = f->parent.get ())
      for (std::size_t i = f->functors.size (); i-- > 0;)
        if (f->functors[i].first == n)
          return f->functors[i].second;
    return nullptr;
  }
};

struct ClosureObj : Obj
{
  ExprP lam;
  FrameP env;
  std::vector<Value> args{};
};

struct Primitive;

struct PrimObj : Obj
{
  Primitive *prim = nullptr;
  std::vector<Value> args{};
  std::shared_ptr<Primitive> owned{}; ///< dynamic host functions (make_function)
};

struct CtorFnObj : Obj
{
  std::string name{};
  int index = 0;
  int arity = 0;
  std::vector<Value> args{};
};

/// A concatenative composition: apply `first`, then `second` (§5.4, §7.3).
struct ComposeObj : Obj
{
  Value first;
  Value second;
};

struct ModuleObj : Obj
{
  enum class Src
  {
    Struct,
    Alias,
    Apply
  };
  std::string name{};
  Src src = Src::Struct;
  ModExprP mexpr{};
  FrameP env{};
  bool forced = false;
  bool forcing = false;
  FrameP frame{};
  std::vector<std::pair<std::string, Value>> fields{};
  std::vector<std::pair<std::string, ModuleP>> mods{};
  std::vector<std::pair<std::string, FunctorP>> functors{};
  std::vector<ItemP> seals{};

  template <class T>
  static void
  upsert (std::vector<std::pair<std::string, T>> &v, const std::string &k, T x)
  {
    for (auto &kv : v)
      if (kv.first == k)
        {
          kv.second = std::move (x);
          return;
        }
    v.push_back ({ k, std::move (x) });
  }
};

/// Base for host-defined opaque values (handles).
struct ForeignObj : Obj
{
  std::string type_name{};
  explicit ForeignObj (std::string t = "Foreign") : type_name (std::move (t)) {}
  virtual std::string
  describe () const
  {
    return "<" + type_name + ">";
  }
};

// ---- value constructors & accessors ----

inline Value
make_str (std::string s)
{
  Value v;
  v.kind = VK::Str;
  v.obj = std::make_shared<StrObj> (std::move (s));
  return v;
}
inline const std::string &
str_of (const Value &v)
{
  static const std::string empty;
  if (v.kind != VK::Str || !v.obj)
    return empty;
  return static_cast<const StrObj *> (v.obj.get ())->s;
}
inline Value
make_list (const std::vector<Value> &items)
{
  ConsP acc;
  for (std::size_t i = items.size (); i-- > 0;)
    acc = std::make_shared<ConsObj> (items[i], acc);
  Value v;
  v.kind = VK::List;
  v.obj = acc;
  return v;
}
inline Value
cons_value (Value h, const Value &tail)
{
  Value v;
  v.kind = VK::List;
  v.obj = std::make_shared<ConsObj> (std::move (h),
                                     std::static_pointer_cast<ConsObj> (tail.obj));
  return v;
}
inline const ConsObj *
cons_of (const Value &v)
{
  return static_cast<const ConsObj *> (v.obj.get ());
}
inline std::vector<Value>
list_items (const Value &v)
{
  std::vector<Value> out;
  for (const ConsObj *c = cons_of (v); c; c = c->tail.get ())
    out.push_back (c->head);
  return out;
}
inline Value
make_tuple (std::vector<Value> items)
{
  auto a = std::make_shared<AggObj> ();
  a->items = std::move (items);
  Value v;
  v.kind = VK::Tuple;
  v.obj = a;
  return v;
}
inline const AggObj *
agg_of (const Value &v)
{
  return static_cast<const AggObj *> (v.obj.get ());
}
inline Value
make_ctor (const std::string &name, int index, std::vector<Value> args = {})
{
  auto a = std::make_shared<AggObj> ();
  a->tag = name;
  a->items = std::move (args);
  Value v;
  v.kind = VK::Ctor;
  v.i = index;
  v.obj = a;
  return v;
}
/// Builds a record from (label, value) pairs; labels are sorted.
inline Value
make_record (std::vector<std::pair<std::string, Value>> fields)
{
  std::stable_sort (fields.begin (), fields.end (),
                    [] (const auto &a, const auto &b) { return a.first < b.first; });
  auto labels = std::make_shared<std::vector<std::string>> ();
  auto a = std::make_shared<AggObj> ();
  for (auto &f : fields)
    {
      labels->push_back (f.first);
      a->items.push_back (f.second);
    }
  a->labels = labels;
  Value v;
  v.kind = VK::Record;
  v.obj = a;
  return v;
}
inline const Value *
record_field (const Value &v, const std::string &label)
{
  if (v.kind != VK::Record)
    return nullptr;
  auto a = agg_of (v);
  for (std::size_t i = 0; i < a->labels->size (); ++i)
    if ((*a->labels)[i] == label)
      return &a->items[i];
  return nullptr;
}
inline Value
make_option (std::optional<Value> v)
{
  if (!v)
    return make_ctor ("None", 0);
  return make_ctor ("Some", 1, { std::move (*v) });
}
inline Value
make_ok (Value v)
{
  return make_ctor ("Ok", 0, { std::move (v) });
}
inline Value
make_err (Value v)
{
  return make_ctor ("Err", 1, { std::move (v) });
}
inline bool
is_callable (const Value &v)
{
  return v.kind == VK::Closure || v.kind == VK::Prim || v.kind == VK::CtorFn
         || v.kind == VK::Compose;
}

/// Canonical value rendering (REPL, `show`, to_string).
inline std::string
show_value (const Value &v)
{
  switch (v.kind)
    {
    case VK::Unit:
      return "()";
    case VK::Bool:
      return v.i ? "true" : "false";
    case VK::Int:
      return std::to_string (v.i);
    case VK::Float:
      return detail::format_float (v.f);
    case VK::Str:
      return detail::quote_string (str_of (v));
    case VK::List:
      {
        std::string s = "[";
        bool first = true;
        for (const ConsObj *c = cons_of (v); c; c = c->tail.get ())
          {
            if (!first)
              s += ", ";
            first = false;
            s += show_value (c->head);
          }
        return s + "]";
      }
    case VK::Tuple:
      {
        std::string s = "(";
        auto a = agg_of (v);
        for (std::size_t i = 0; i < a->items.size (); ++i)
          s += (i ? ", " : "") + show_value (a->items[i]);
        return s + ")";
      }
    case VK::Record:
      {
        auto a = agg_of (v);
        if (a->items.empty ())
          return "{}";
        std::string s = "{ ";
        for (std::size_t i = 0; i < a->items.size (); ++i)
          s += (i ? ", " : "") + (*a->labels)[i] + " = " + show_value (a->items[i]);
        return s + " }";
      }
    case VK::Ctor:
      {
        auto a = agg_of (v);
        std::string s = a->tag;
        for (auto &x : a->items)
          {
            bool paren = (x.kind == VK::Ctor && !agg_of (x)->items.empty ())
                         || (x.kind == VK::Int && x.i < 0)
                         || (x.kind == VK::Float && std::signbit (x.f));
            s += " " + (paren ? "(" + show_value (x) + ")" : show_value (x));
          }
        return s;
      }
    case VK::Closure:
    case VK::Prim:
    case VK::CtorFn:
    case VK::Compose:
      return "<fn>";
    case VK::Module:
      return "<module " + static_cast<const ModuleObj *> (v.obj.get ())->name + ">";
    case VK::Foreign:
      return static_cast<const ForeignObj *> (v.obj.get ())->describe ();
    }
  return "?";
}

/// Unquoted rendering: strings print raw.
inline std::string
display_value (const Value &v)
{
  return v.kind == VK::Str ? str_of (v) : show_value (v);
}

namespace detail
{
inline bool
is_function_value (const Value &v)
{
  return is_callable (v);
}
} // namespace detail

/// Structural equality (IEEE for floats; functions are incomparable).
inline bool
values_equal (const Value &a, const Value &b, const Span &sp = {})
{
  if ((a.kind == VK::Int || a.kind == VK::Float) && (b.kind == VK::Int || b.kind == VK::Float)
      && a.kind != b.kind)
    {
      double x = a.kind == VK::Int ? static_cast<double> (a.i) : a.f;
      double y = b.kind == VK::Int ? static_cast<double> (b.i) : b.f;
      return x == y;
    }
  if (is_callable (a) || is_callable (b))
    detail::fail ("M0405", "functions cannot be compared for equality", sp);
  if (a.kind != b.kind)
    return false;
  switch (a.kind)
    {
    case VK::Unit:
      return true;
    case VK::Bool:
    case VK::Int:
      return a.i == b.i;
    case VK::Float:
      return a.f == b.f;
    case VK::Str:
      return str_of (a) == str_of (b);
    case VK::List:
      {
        const ConsObj *x = cons_of (a), *y = cons_of (b);
        for (; x && y; x = x->tail.get (), y = y->tail.get ())
          if (!values_equal (x->head, y->head, sp))
            return false;
        return !x && !y;
      }
    case VK::Tuple:
    case VK::Record:
    case VK::Ctor:
      {
        auto x = agg_of (a), y = agg_of (b);
        if (x->tag != y->tag || x->items.size () != y->items.size ())
          return false;
        if (a.kind == VK::Record && *x->labels != *y->labels)
          return false;
        for (std::size_t i = 0; i < x->items.size (); ++i)
          if (!values_equal (x->items[i], y->items[i], sp))
            return false;
        return true;
      }
    case VK::Module:
    case VK::Foreign:
      return a.obj == b.obj;
    default:
      return false;
    }
}

/// Structural total order (-1, 0, 1); NaN compares equal to everything.
inline int
compare_values (const Value &a, const Value &b, const Span &sp = {})
{
  auto num = [] (const Value &v) { return v.kind == VK::Int ? static_cast<double> (v.i) : v.f; };
  if ((a.kind == VK::Int || a.kind == VK::Float) && (b.kind == VK::Int || b.kind == VK::Float))
    {
      if (a.kind == VK::Int && b.kind == VK::Int)
        return a.i < b.i ? -1 : a.i > b.i ? 1 : 0;
      double x = num (a), y = num (b);
      return x < y ? -1 : x > y ? 1 : 0;
    }
  if (is_callable (a) || is_callable (b))
    detail::fail ("M0405", "functions cannot be compared", sp);
  if (a.kind != b.kind)
    return static_cast<int> (a.kind) < static_cast<int> (b.kind) ? -1 : 1;
  switch (a.kind)
    {
    case VK::Unit:
      return 0;
    case VK::Bool:
      return a.i < b.i ? -1 : a.i > b.i ? 1 : 0;
    case VK::Str:
      {
        int c = str_of (a).compare (str_of (b));
        return c < 0 ? -1 : c > 0 ? 1 : 0;
      }
    case VK::List:
      {
        const ConsObj *x = cons_of (a), *y = cons_of (b);
        for (; x && y; x = x->tail.get (), y = y->tail.get ())
          if (int c = compare_values (x->head, y->head, sp))
            return c;
        return x ? 1 : y ? -1 : 0;
      }
    case VK::Tuple:
    case VK::Record:
    case VK::Ctor:
      {
        if (a.kind == VK::Ctor && a.i != b.i)
          return a.i < b.i ? -1 : 1;
        auto x = agg_of (a), y = agg_of (b);
        std::size_t n = std::min (x->items.size (), y->items.size ());
        for (std::size_t i = 0; i < n; ++i)
          if (int c = compare_values (x->items[i], y->items[i], sp))
            return c;
        return x->items.size () < y->items.size () ? -1
               : x->items.size () > y->items.size () ? 1
                                                     : 0;
      }
    default:
      detail::fail ("M0405", "modules and host handles cannot be ordered", sp);
    }
}

// ---- primitives ----

/// Context handed to host primitives.
struct PrimContext
{
  Session &session;
  Interp &interp;
  Span span{};
  Value apply (const Value &f, std::vector<Value> args);
  [[noreturn]] void
  error (std::string code, std::string msg)
  {
    detail::fail (std::move (code), std::move (msg), span);
  }
  void print (std::string_view text);
};

using PrimFn = std::function<Value (PrimContext &, std::vector<Value> &)>;

struct Primitive
{
  std::string name{};
  std::string type_src{};
  int arity = 1;
  PrimFn fn{};
  bool core = false;
  TypeP scheme{};
  bool resolving = false;
};

// ---- the evaluator ----

class Interp
{
public:
  int max_depth = 10000;
  int call_depth = 0;
  int eval_depth = 0;
  std::vector<Value> stack{}; ///< implicit concatenative value stack (§7.3)
  bool tracing = false;
  int trace_limit = 32;
  int trace_base = 0;
  std::function<void (const std::string &)> trace_sink{};
  std::function<bool (Primitive *)> allowed{};
  std::function<bool ()> cancelled{};
  std::function<const Cell *(const std::string &)> global_lookup{};
  Session *session = nullptr;
  std::recursive_mutex *module_lock = nullptr; ///< serialises lazy module init

  void check_cancelled (const Span &sp = {})
  {
    if (cancelled && cancelled ())
      fail ("M0410", "task cancelled", sp);
  }

  [[noreturn]] void
  fail (std::string code, std::string msg, const Span &sp)
  {
    detail::fail (std::move (code), std::move (msg), sp);
  }

  Value
  prim_value (Primitive *p)
  {
    auto o = std::make_shared<PrimObj> ();
    o->prim = p;
    Value v;
    v.kind = VK::Prim;
    v.obj = o;
    return v;
  }

  Primitive *
  op_prim (const std::string &op)
  {
    auto it = op_prims_.find (op);
    if (it != op_prims_.end ())
      return it->second.get ();
    auto p = std::make_unique<Primitive> ();
    p->name = "(" + op + ")";
    p->arity = 2;
    p->core = true;
    std::string o = op;
    p->fn = [this, o] (PrimContext &ctx, std::vector<Value> &a)
      { return binop (o, a[0], a[1], ctx.span); };
    auto raw = p.get ();
    op_prims_[op] = std::move (p);
    return raw;
  }

  // ---- arithmetic ----
  Value
  binop (const std::string &op, const Value &a, const Value &b, const Span &sp)
  {
    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%")
      {
        if (a.kind == VK::Int && b.kind == VK::Int)
          {
            std::int64_t x = a.i, y = b.i;
            if (op == "+")
              return Value::integer (detail::wrap_add (x, y));
            if (op == "-")
              return Value::integer (detail::wrap_sub (x, y));
            if (op == "*")
              return Value::integer (detail::wrap_mul (x, y));
            if (y == 0)
              fail ("M0402", "division by zero", sp);
            if (y == -1)
              return Value::integer (op == "/" ? detail::wrap_neg (x) : 0);
            return Value::integer (op == "/" ? x / y : x % y);
          }
        if ((a.kind != VK::Int && a.kind != VK::Float)
            || (b.kind != VK::Int && b.kind != VK::Float))
          fail ("M0404", "arithmetic on non-numeric values", sp);
        double x = a.kind == VK::Int ? static_cast<double> (a.i) : a.f;
        double y = b.kind == VK::Int ? static_cast<double> (b.i) : b.f;
        if (op == "+")
          return Value::floating (x + y);
        if (op == "-")
          return Value::floating (x - y);
        if (op == "*")
          return Value::floating (x * y);
        if (op == "/")
          return Value::floating (x / y);
        return Value::floating (std::fmod (x, y));
      }
    if (op == "==" || op == "=")
      return Value::boolean (values_equal (a, b, sp));
    if (op == "!=" || op == "<>")
      return Value::boolean (!values_equal (a, b, sp));
    if (op == "<" || op == ">" || op == "<=" || op == ">=")
      {
        if (a.kind == VK::Float || b.kind == VK::Float)
          {
            double x = a.kind == VK::Int ? static_cast<double> (a.i) : a.f;
            double y = b.kind == VK::Int ? static_cast<double> (b.i) : b.f;
            bool r = op == "<" ? x < y : op == ">" ? x > y : op == "<=" ? x <= y : x >= y;
            return Value::boolean (r);
          }
        int c = compare_values (a, b, sp);
        bool r = op == "<" ? c < 0 : op == ">" ? c > 0 : op == "<=" ? c <= 0 : c >= 0;
        return Value::boolean (r);
      }
    if (op == "&&")
      return Value::boolean (a.i && b.i);
    if (op == "||")
      return Value::boolean (a.i || b.i);
    if (op == "^")
      return make_str (str_of (a) + str_of (b));
    if (op == "@")
      {
        auto xs = list_items (a);
        Value acc = b;
        for (std::size_t i = xs.size (); i-- > 0;)
          acc = cons_value (xs[i], acc);
        return acc;
      }
    if (op == "::")
      return cons_value (a, b);
    if (op == ">>")
      return compose (a, b);
    if (op == "<<" || op == "\xE2\x88\x98")
      return compose (b, a);
    if (op == "|>")
      return apply (b, { a }, sp);
    fail ("M0404", "unknown operator `" + op + "`", sp);
  }

  static Value
  compose (const Value &first, const Value &second)
  {
    auto c = std::make_shared<ComposeObj> ();
    c->first = first;
    c->second = second;
    Value v;
    v.kind = VK::Compose;
    v.obj = c;
    return v;
  }

  // ---- patterns ----
  bool
  match (const PatP &p, const Value &v, Frame &f)
  {
    switch (p->kind)
      {
      case PK::Wild:
        return true;
      case PK::Var:
        f.vars.push_back ({ p->text, v });
        return true;
      case PK::Int:
        return v.kind == VK::Int && v.i == p->ival;
      case PK::Float:
        return v.kind == VK::Float && v.f == p->fval;
      case PK::Str:
        return v.kind == VK::Str && str_of (v) == p->text;
      case PK::Bool:
        return v.kind == VK::Bool && v.i == p->ival;
      case PK::Unit:
        return v.kind == VK::Unit;
      case PK::Tuple:
        {
          if (v.kind != VK::Tuple)
            return false;
          auto a = agg_of (v);
          if (a->items.size () != p->kids.size ())
            return false;
          for (std::size_t i = 0; i < p->kids.size (); ++i)
            if (!match (p->kids[i], a->items[i], f))
              return false;
          return true;
        }
      case PK::List:
        {
          if (v.kind != VK::List)
            return false;
          const ConsObj *c = cons_of (v);
          for (auto &k : p->kids)
            {
              if (!c || !match (k, c->head, f))
                return false;
              c = c->tail.get ();
            }
          return c == nullptr;
        }
      case PK::Cons:
        {
          if (v.kind != VK::List || !v.obj)
            return false;
          const ConsObj *c = cons_of (v);
          Value tail;
          tail.kind = VK::List;
          tail.obj = c->tail;
          return match (p->kids[0], c->head, f) && match (p->kids[1], tail, f);
        }
      case PK::Ctor:
        {
          if (v.kind != VK::Ctor)
            return false;
          auto a = agg_of (v);
          if (a->tag != p->text || a->items.size () != p->kids.size ())
            return false;
          for (std::size_t i = 0; i < p->kids.size (); ++i)
            if (!match (p->kids[i], a->items[i], f))
              return false;
          return true;
        }
      case PK::Record:
        for (std::size_t i = 0; i < p->kids.size (); ++i)
          {
            auto fv = record_field (v, p->labels[i]);
            if (!fv || !match (p->kids[i], *fv, f))
              return false;
          }
        return true;
      case PK::Annot:
        return match (p->kids[0], v, f);
      }
    return false;
  }

  // ---- modules ----
  ModuleP
  make_module (const ItemP &it, const FrameP &env)
  {
    auto m = std::make_shared<ModuleObj> ();
    m->name = it->name;
    m->mexpr = it->mexpr;
    m->env = env;
    m->src = it->mexpr->kind == MK::Struct  ? ModuleObj::Src::Struct
             : it->mexpr->kind == MK::Path ? ModuleObj::Src::Alias
                                           : ModuleObj::Src::Apply;
    if (it->sealed)
      m->seals.push_back (it);
    return m;
  }

  ModuleP
  module_from (const ModExprP &me, const FrameP &env, const std::string &name)
  {
    if (me->kind == MK::Path)
      return resolve_module (me->path, env, me->span);
    auto m = std::make_shared<ModuleObj> ();
    m->name = name;
    m->mexpr = me;
    m->env = env;
    m->src = me->kind == MK::Struct ? ModuleObj::Src::Struct : ModuleObj::Src::Apply;
    return m;
  }

  ModuleP
  resolve_module (const std::vector<std::string> &path, const FrameP &env, const Span &sp)
  {
    std::unique_lock<std::recursive_mutex> lk;
    if (module_lock) lk = std::unique_lock<std::recursive_mutex> (*module_lock);
    auto m = env->find_mod (path[0]);
    if (!m)
      fail ("M0501", "unbound module `" + path[0] + "` at run time", sp);
    for (std::size_t i = 1; i < path.size (); ++i)
      {
        force (m, sp);
        ModuleP next;
        for (std::size_t k = m->mods.size (); k-- > 0;)
          if (m->mods[k].first == path[i])
            {
              next = m->mods[k].second;
              break;
            }
        if (!next)
          fail ("M0501", "module `" + m->name + "` has no submodule `" + path[i] + "`", sp);
        m = next;
      }
    return m;
  }

  FunctorP
  resolve_functor (const std::vector<std::string> &path, const FrameP &env, const Span &sp)
  {
    std::unique_lock<std::recursive_mutex> lk;
    if (module_lock) lk = std::unique_lock<std::recursive_mutex> (*module_lock);
    if (path.size () == 1)
      {
        auto f = env->find_functor (path[0]);
        if (!f)
          fail ("M0503", "unbound functor `" + path[0] + "` at run time", sp);
        return f;
      }
    auto m = resolve_module (std::vector<std::string> (path.begin (), path.end () - 1), env, sp);
    force (m, sp);
    for (std::size_t k = m->functors.size (); k-- > 0;)
      if (m->functors[k].first == path.back ())
        return m->functors[k].second;
    fail ("M0503", "unbound functor `" + detail::join (path, ".") + "` at run time", sp);
  }

  void
  apply_seal (ModuleObj &m, const ItemP &seal)
  {
    std::vector<std::pair<std::string, Value>> fields;
    for (auto &n : seal->visible_values)
      for (std::size_t k = m.fields.size (); k-- > 0;)
        if (m.fields[k].first == n)
          {
            fields.push_back (m.fields[k]);
            break;
          }
    m.fields = std::move (fields);
    std::vector<std::pair<std::string, ModuleP>> mods;
    for (auto &n : seal->visible_modules)
      for (std::size_t k = m.mods.size (); k-- > 0;)
        if (m.mods[k].first == n)
          {
            mods.push_back (m.mods[k]);
            break;
          }
    m.mods = std::move (mods);
    m.functors.clear ();
  }

  /// Evaluates a module body once, on first access (§7.4).
  void
  force (const ModuleP &m, const Span &sp)
  {
    std::unique_lock<std::recursive_mutex> lk;
    if (module_lock)
      lk = std::unique_lock<std::recursive_mutex> (*module_lock);
    if (m->forced || m->forcing)
      return;
    m->forcing = true;
    try
      {
        switch (m->src)
          {
          case ModuleObj::Src::Struct:
            m->frame = Frame::child (m->env);
            exec_struct (m->mexpr->items, m->frame, *m);
            break;
          case ModuleObj::Src::Alias:
            {
              auto t = resolve_module (m->mexpr->path, m->env, sp);
              force (t, sp);
              m->fields = t->fields;
              m->mods = t->mods;
              m->functors = t->functors;
              break;
            }
          case ModuleObj::Src::Apply:
            {
              auto f = resolve_functor (m->mexpr->path, m->env, sp);
              auto a = module_from (m->mexpr->arg, m->env, m->name + ".arg");
              force (a, sp);
              m->frame = Frame::child (f->env);
              m->frame->mods.push_back ({ f->item->pname, a });
              exec_struct (f->item->mexpr->items, m->frame, *m);
              if (f->item->sealed)
                apply_seal (*m, f->item);
              break;
            }
          }
        for (auto &s : m->seals)
          apply_seal (*m, s);
      }
    catch (...)
      {
        m->forcing = false;
        throw;
      }
    m->forcing = false;
    m->forced = true;
  }

  Value
  module_field (const ModuleP &m, const std::string &name, const Span &sp)
  {
    force (m, sp);
    if (m->forcing && m->frame)
      if (auto v = m->frame->find_var (name))
        return *v;
    for (std::size_t k = m->fields.size (); k-- > 0;)
      if (m->fields[k].first == name)
        return m->fields[k].second;
    if (m->forcing)
      fail ("M0507", "module `" + m->name + "` is still being initialised; `" + name
                         + "` is not available yet",
            sp);
    fail ("M0506", "module `" + m->name + "` has no member `" + name + "`", sp);
  }

  void
  exec_struct (const std::vector<ItemP> &items, const FrameP &frame, ModuleObj &mod)
  {
    for (auto &it : items)
      switch (it->kind)
        {
        case IK::Def:
          {
            Value v = eval (it->value, frame);
            frame->vars.push_back ({ it->name, v });
            ModuleObj::upsert (mod.fields, it->name, v);
            break;
          }
        case IK::Module:
          {
            auto m = make_module (it, frame);
            frame->mods.push_back ({ it->name, m });
            ModuleObj::upsert (mod.mods, it->name, m);
            break;
          }
        case IK::Functor:
          {
            auto f = std::make_shared<FunctorObj> ();
            f->item = it;
            f->env = frame;
            frame->functors.push_back ({ it->name, f });
            ModuleObj::upsert (mod.functors, it->name, f);
            break;
          }
        case IK::Open:
        case IK::Include:
          {
            auto src = resolve_module (it->path, frame, it->span);
            force (src, it->span);
            for (auto &kv : src->fields)
              {
                frame->vars.push_back (kv);
                if (it->kind == IK::Include)
                  ModuleObj::upsert (mod.fields, kv.first, kv.second);
              }
            for (auto &kv : src->mods)
              {
                frame->mods.push_back (kv);
                if (it->kind == IK::Include)
                  ModuleObj::upsert (mod.mods, kv.first, kv.second);
              }
            for (auto &kv : src->functors)
              {
                frame->functors.push_back (kv);
                if (it->kind == IK::Include)
                  ModuleObj::upsert (mod.functors, kv.first, kv.second);
              }
            break;
          }
        case IK::Expr:
          eval (it->body, frame);
          break;
        default:
          break;
        }
  }

  // ---- application ----
  void
  trace_call (const ClosureObj &c, const std::vector<Value> &args, int depth)
  {
    if (!tracing || !trace_sink)
      return;
    int d = depth - trace_base;
    if (d < 0)
      d = 0;
    if (trace_limit > 0 && d >= trace_limit)
      return;
    std::string line (static_cast<std::size_t> (d) * 2, ' ');
    line += c.lam->name.empty () ? "<fn>" : c.lam->name;
    for (auto &a : args)
      {
        std::string s = show_value (a);
        bool paren = s.find (' ') != std::string::npos && s[0] != '[' && s[0] != '('
                     && s[0] != '{' && s[0] != '"';
        line += " " + (paren ? "(" + s + ")" : s);
      }
    trace_sink (line);
  }

  void
  enter_call (const Span &sp)
  {
    if (++call_depth > max_depth)
      {
        call_depth = 0;
        fail ("M0406", "stack overflow: recursion deeper than "
                           + std::to_string (max_depth) + " calls",
              sp);
      }
  }

  /// Binds a saturated closure call into a fresh frame.
  FrameP
  bind_call (const ClosureObj &c, const std::vector<Value> &args, const Span &sp)
  {
    auto f = Frame::child (c.env);
    const auto &pats = c.lam->pats;
    for (std::size_t i = 0; i < pats.size (); ++i)
      if (!match (pats[i], args[i], *f))
        fail ("M0401", "the argument `" + show_value (args[i])
                           + "` does not match the parameter pattern",
              sp);
    for (int k = 1; k <= c.lam->tacit_count; ++k)
      f->vars.push_back ({ "$" + std::to_string (k), args[static_cast<std::size_t> (k - 1)] });
    return f;
  }

  Value
  apply (Value f, std::vector<Value> args, const Span &sp = {})
  {
    while (true)
      {
        check_cancelled (sp);
        if (args.empty ())
          return f;
        switch (f.kind)
          {
          case VK::Closure:
            {
              auto c = static_cast<const ClosureObj *> (f.obj.get ());
              std::size_t n = c->lam->pats.size ();
              std::size_t have = c->args.size ();
              if (have + args.size () < n)
                {
                  auto nc = std::make_shared<ClosureObj> (*c);
                  for (auto &a : args)
                    nc->args.push_back (a);
                  Value v;
                  v.kind = VK::Closure;
                  v.obj = nc;
                  return v;
                }
              std::vector<Value> full = c->args;
              std::size_t take = n - have;
              for (std::size_t i = 0; i < take; ++i)
                full.push_back (args[i]);
              std::vector<Value> rest (args.begin () + static_cast<std::ptrdiff_t> (take),
                                       args.end ());
              trace_call (*c, full, call_depth);
              auto frame = bind_call (*c, full, sp);
              enter_call (sp);
              Value r = eval (c->lam->kids[0], frame, true);
              f = r;
              args = std::move (rest);
              continue;
            }
          case VK::Prim:
            {
              auto po = static_cast<const PrimObj *> (f.obj.get ());
              std::size_t n = static_cast<std::size_t> (po->prim->arity);
              if (po->args.size () + args.size () < n)
                {
                  auto np = std::make_shared<PrimObj> (*po);
                  for (auto &a : args)
                    np->args.push_back (a);
                  Value v;
                  v.kind = VK::Prim;
                  v.obj = np;
                  return v;
                }
              std::vector<Value> full = po->args;
              std::size_t take = n - po->args.size ();
              for (std::size_t i = 0; i < take; ++i)
                full.push_back (args[i]);
              std::vector<Value> rest (args.begin () + static_cast<std::ptrdiff_t> (take),
                                       args.end ());
              if (allowed && !allowed (po->prim))
                fail ("M0403", "unknown primitive `" + po->prim->name
                                   + "` (not in the primitive allowlist)",
                      sp);
              PrimContext ctx{ *session, *this, sp };
              try
                { f = po->prim->fn (ctx, full); }
              catch (const detail::MetaError &) { throw; }
              catch (const std::exception &ex)
                { fail ("M0409", "host primitive `" + po->prim->name + "` failed: " + ex.what (), sp); }
              args = std::move (rest);
              continue;
            }
          case VK::CtorFn:
            {
              auto co = static_cast<const CtorFnObj *> (f.obj.get ());
              auto nc = std::make_shared<CtorFnObj> (*co);
              std::size_t i = 0;
              while (i < args.size () && static_cast<int> (nc->args.size ()) < nc->arity)
                nc->args.push_back (args[i++]);
              std::vector<Value> rest (args.begin () + static_cast<std::ptrdiff_t> (i),
                                       args.end ());
              if (static_cast<int> (nc->args.size ()) < nc->arity)
                {
                  Value v;
                  v.kind = VK::CtorFn;
                  v.obj = nc;
                  return v;
                }
              f = make_ctor (nc->name, nc->index, nc->args);
              args = std::move (rest);
              continue;
            }
          case VK::Compose:
            {
              auto co = static_cast<const ComposeObj *> (f.obj.get ());
              Value first = co->first, second = co->second;
              stack.push_back (args[0]);
              Value top = stack.back ();
              stack.pop_back ();
              Value mid = apply (first, { top }, sp);
              stack.push_back (mid);
              top = stack.back ();
              stack.pop_back ();
              Value r = apply (second, { top }, sp);
              args.erase (args.begin ());
              f = r;
              continue;
            }
          default:
            fail ("M0404", "`" + show_value (f) + "` is not a function and cannot be applied",
                  sp);
          }
      }
  }

  // ---- evaluation (with tail calls) ----
  Value
  eval (ExprP e, FrameP env, bool counted = false)
  {
    struct Guard
    {
      Interp &in;
      bool &counted;
      Guard (Interp &i, bool &c) : in (i), counted (c) { ++in.eval_depth; }
      ~Guard ()
      {
        --in.eval_depth;
        if (counted && in.call_depth > 0)
          --in.call_depth;
      }
    } guard (*this, counted);
    if (eval_depth > max_depth * 8 + 1000)
      fail ("M0406", "stack overflow: expression nesting too deep", e->span);
    while (true)
      {
        check_cancelled (e->span);
        switch (e->kind)
          {
          case EK::Int:
            return Value::integer (e->ival);
          case EK::Float:
            return Value::floating (e->fval);
          case EK::Str:
            return make_str (e->text);
          case EK::Bool:
            return Value::boolean (e->ival != 0);
          case EK::Unit:
            return Value::unit ();
          case EK::Var:
          case EK::Tacit:
            {
              if (e->cell)
                {
                  if (!e->cell->defined)
                    fail ("M0408", "`" + e->cell->name + "` is used before its definition",
                          e->span);
                  return e->cell->value;
                }
              const std::string &n = e->kind == EK::Tacit ? e->text : e->text;
              if (auto v = env->find_var (n))
                return *v;
              if (global_lookup)
                if (auto c = global_lookup (n))
                  if (c->defined)
                    return c->value;
              fail ("M0204", "unbound variable `" + n + "` at run time", e->span);
            }
          case EK::Path:
            {
              if (e->prim)
                return prim_value (e->prim);
              std::vector<std::string> mp (e->path.begin (), e->path.end () - 1);
              auto m = resolve_module (mp, env, e->span);
              return module_field (m, e->path.back (), e->span);
            }
          case EK::Ctor:
            {
              if (e->ctor_arity == 0)
                return make_ctor (e->text, e->ctor_index);
              auto c = std::make_shared<CtorFnObj> ();
              c->name = e->text;
              c->index = e->ctor_index;
              c->arity = e->ctor_arity;
              Value v;
              v.kind = VK::CtorFn;
              v.obj = c;
              return v;
            }
          case EK::App:
            {
              if (e->resolution != Resolution::Apply)
                {
                  Value a = eval (e->kids[0], env);
                  Value b = eval (e->kids[1], env);
                  return e->resolution == Resolution::ComposeFwd ? compose (a, b)
                                                                 : compose (b, a);
                }
              std::vector<const Expr *> spine;
              const Expr *h = e.get ();
              while (h->kind == EK::App && h->resolution == Resolution::Apply)
                {
                  spine.push_back (h);
                  h = h->kids[0].get ();
                }
              Value fv = eval (spine.back ()->kids[0], env);
              std::vector<Value> args;
              for (std::size_t i = spine.size (); i-- > 0;)
                args.push_back (eval (spine[i]->kids[1], env));
              if (tail_call (fv, args, e, env, counted))
                continue;
              return apply (fv, std::move (args), e->span);
            }
          case EK::Pipe:
            {
              Value a = eval (e->kids[0], env);
              Value b = eval (e->kids[1], env);
              if (e->resolution != Resolution::Apply)
                return compose (a, b);
              std::vector<Value> args{ a };
              if (tail_call (b, args, e, env, counted))
                continue;
              return apply (b, std::move (args), e->span);
            }
          case EK::BinOp:
            {
              const std::string &op = e->text;
              if (op == "&&" || op == "||")
                {
                  Value a = eval (e->kids[0], env);
                  if ((op == "&&") != (a.i != 0))
                    return a;
                  e = e->kids[1];
                  continue;
                }
              Value a = eval (e->kids[0], env);
              Value b = eval (e->kids[1], env);
              if (detail::is_builtin_op (op))
                return binop (op, a, b, e->span);
              Value fn;
              if (e->cell && e->cell->defined)
                fn = e->cell->value;
              else if (auto v = env->find_var (op))
                fn = *v;
              else if (global_lookup && global_lookup (op))
                fn = global_lookup (op)->value;
              else
                fail ("M0204", "unbound operator `" + op + "` at run time", e->span);
              return apply (fn, { a, b }, e->span);
            }
          case EK::Neg:
            {
              Value a = eval (e->kids[0], env);
              if (a.kind == VK::Int)
                return Value::integer (detail::wrap_neg (a.i));
              return Value::floating (-a.f);
            }
          case EK::Lambda:
            {
              auto c = std::make_shared<ClosureObj> ();
              c->lam = e;
              c->env = env;
              Value v;
              v.kind = VK::Closure;
              v.obj = c;
              return v;
            }
          case EK::If:
            {
              Value c = eval (e->kids[0], env);
              e = c.i ? e->kids[1] : e->kids[2];
              continue;
            }
          case EK::Match:
            {
              Value v = eval (e->kids[0], env);
              bool found = false;
              for (std::size_t i = 0; i < e->pats.size (); ++i)
                {
                  auto f = Frame::child (env);
                  if (!match (e->pats[i], v, *f))
                    continue;
                  if (e->guards[i] && !eval (e->guards[i], f).i)
                    continue;
                  env = f;
                  e = e->kids[i + 1];
                  found = true;
                  break;
                }
              if (!found)
                fail ("M0401", "no pattern matched the value `" + show_value (v) + "`",
                      e->span);
              continue;
            }
          case EK::Let:
            {
              Value v = eval (e->kids[0], env);
              auto f = Frame::child (env);
              if (!match (e->pats[0], v, *f))
                fail ("M0401", "the value `" + show_value (v)
                                   + "` does not match the `let` pattern",
                      e->pats[0]->span);
              env = f;
              e = e->kids[1];
              continue;
            }
          case EK::LetRec:
            {
              auto f = Frame::child (env);
              for (std::size_t i = 0; i < e->pats.size (); ++i)
                {
                  Value v = eval (e->kids[i], f);
                  f->vars.push_back ({ e->pats[i]->text, v });
                }
              env = f;
              e = e->kids.back ();
              continue;
            }
          case EK::List:
            {
              std::vector<Value> items;
              for (auto &k : e->kids)
                items.push_back (eval (k, env));
              return make_list (items);
            }
          case EK::Comp:
            {
              std::vector<Value> out;
              comprehension (e, 0, env, out);
              return make_list (out);
            }
          case EK::Range:
            {
              Value a = eval (e->kids[0], env), b = eval (e->kids[1], env);
              std::vector<Value> items;
              if (b.i >= a.i && static_cast<std::uint64_t> (b.i)
                    - static_cast<std::uint64_t> (a.i) > 50000000ULL)
                fail ("M0404", "range is too large", e->span);
              for (std::int64_t k = a.i; k <= b.i; ++k)
                {
                  items.push_back (Value::integer (k));
                  if (k == INT64_MAX)
                    break;
                }
              return make_list (items);
            }
          case EK::Tuple:
            {
              std::vector<Value> items;
              for (auto &k : e->kids)
                items.push_back (eval (k, env));
              return make_tuple (std::move (items));
            }
          case EK::Record:
            {
              std::vector<std::pair<std::string, Value>> fields;
              for (std::size_t i = 0; i < e->kids.size (); ++i)
                fields.push_back ({ e->labels[i], eval (e->kids[i], env) });
              return make_record (std::move (fields));
            }
          case EK::Update:
            {
              Value base = eval (e->kids[0], env);
              if (base.kind != VK::Record)
                fail ("M0404", "record update on a non-record value", e->span);
              auto a = agg_of (base);
              auto n = std::make_shared<AggObj> (*a);
              for (std::size_t i = 0; i < e->labels.size (); ++i)
                {
                  Value nv = eval (e->kids[i + 1], env);
                  for (std::size_t k = 0; k < n->labels->size (); ++k)
                    if ((*n->labels)[k] == e->labels[i])
                      n->items[k] = nv;
                }
              Value v;
              v.kind = VK::Record;
              v.obj = n;
              return v;
            }
          case EK::Project:
            {
              Value r = eval (e->kids[0], env);
              auto fv = record_field (r, e->text);
              if (!fv)
                fail ("M0404", "the value `" + show_value (r) + "` has no field `" + e->text
                                   + "`",
                      e->span);
              return *fv;
            }
          case EK::OpRef:
            {
              if (detail::is_builtin_op (e->text))
                return prim_value (op_prim (e->text));
              if (e->cell && e->cell->defined)
                return e->cell->value;
              if (auto v = env->find_var (e->text))
                return *v;
              fail ("M0204", "unbound operator `" + e->text + "` at run time", e->span);
            }
          case EK::Annot:
            e = e->kids[0];
            continue;
          }
        return Value::unit ();
      }
  }

private:
  std::map<std::string, std::unique_ptr<Primitive>> op_prims_{};

  /// If `f args` saturates a closure exactly, rebinds e/env for a tail call.
  bool
  tail_call (const Value &f, const std::vector<Value> &args, ExprP &e, FrameP &env,
             bool &counted)
  {
    if (f.kind != VK::Closure)
      return false;
    auto c = static_cast<const ClosureObj *> (f.obj.get ());
    if (c->args.size () + args.size () != c->lam->pats.size ())
      return false;
    std::vector<Value> full = c->args;
    for (auto &a : args)
      full.push_back (a);
    if (!counted)
      {
        trace_call (*c, full, call_depth);
        auto frame = bind_call (*c, full, e->span);
        enter_call (e->span);
        counted = true;
        env = frame;
      }
    else
      {
        trace_call (*c, full, call_depth - 1);
        env = bind_call (*c, full, e->span);
      }
    e = c->lam->kids[0];
    return true;
  }

  void
  comprehension (const ExprP &e, std::size_t idx, const FrameP &env, std::vector<Value> &out)
  {
    if (idx == e->clauses.size ())
      {
        out.push_back (eval (e->kids[0], env));
        return;
      }
    const auto &c = e->clauses[idx];
    if (!c.pat)
      {
        if (eval (c.expr, env).i)
          comprehension (e, idx + 1, env, out);
        return;
      }
    Value src = eval (c.expr, env);
    for (const ConsObj *n = cons_of (src); n; n = n->tail.get ())
      {
        auto f = Frame::child (env);
        if (match (c.pat, n->head, *f))
          comprehension (e, idx + 1, f, out);
      }
  }
};

// ============================================================
//  Typed host-primitive registration (§10.3)
// ============================================================

namespace host
{

template <class T, class = void> struct Conv;

struct TypeVars
{
  int next = 0;
  std::string
  fresh ()
  {
    std::string s = "'";
    s += static_cast<char> ('a' + next % 26);
    if (next >= 26)
      s += std::to_string (next / 26);
    ++next;
    return s;
  }
};

template <class T>
struct Conv<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>>
{
  static std::string
  type (TypeVars &)
  {
    return "Int";
  }
  static T
  from (const Value &v)
  {
    return static_cast<T> (v.kind == VK::Float ? detail::float_to_int (v.f) : v.i);
  }
  static Value
  to (T x)
  {
    return Value::integer (static_cast<std::int64_t> (x));
  }
};
template <class T> struct Conv<T, std::enable_if_t<std::is_floating_point_v<T>>>
{
  static std::string
  type (TypeVars &)
  {
    return "Float";
  }
  static T
  from (const Value &v)
  {
    return static_cast<T> (v.kind == VK::Int ? static_cast<double> (v.i) : v.f);
  }
  static Value
  to (T x)
  {
    return Value::floating (static_cast<double> (x));
  }
};
template <> struct Conv<bool>
{
  static std::string
  type (TypeVars &)
  {
    return "Bool";
  }
  static bool
  from (const Value &v)
  {
    return v.i != 0;
  }
  static Value
  to (bool b)
  {
    return Value::boolean (b);
  }
};
template <> struct Conv<std::string>
{
  static std::string
  type (TypeVars &)
  {
    return "String";
  }
  static std::string
  from (const Value &v)
  {
    return str_of (v);
  }
  static Value
  to (std::string s)
  {
    return make_str (std::move (s));
  }
};
template <> struct Conv<std::string_view>
{
  static std::string
  type (TypeVars &)
  {
    return "String";
  }
  static std::string_view
  from (const Value &v)
  {
    return str_of (v);
  }
  static Value
  to (std::string_view s)
  {
    return make_str (std::string (s));
  }
};
template <> struct Conv<Value>
{
  static std::string
  type (TypeVars &tv)
  {
    return tv.fresh ();
  }
  static Value
  from (const Value &v)
  {
    return v;
  }
  static Value
  to (Value v)
  {
    return v;
  }
};
template <class T> struct Conv<std::optional<T>>
{
  static std::string
  type (TypeVars &tv)
  {
    return "Option (" + Conv<T>::type (tv) + ")";
  }
  static std::optional<T>
  from (const Value &v)
  {
    if (v.kind == VK::Ctor && agg_of (v)->tag == "Some")
      return Conv<T>::from (agg_of (v)->items[0]);
    return std::nullopt;
  }
  static Value
  to (std::optional<T> x)
  {
    if (!x)
      return make_option (std::nullopt);
    return make_option (Conv<T>::to (std::move (*x)));
  }
};
template <class T> struct Conv<std::vector<T>>
{
  static std::string
  type (TypeVars &tv)
  {
    return "[" + Conv<T>::type (tv) + "]";
  }
  static std::vector<T>
  from (const Value &v)
  {
    std::vector<T> out;
    for (auto &x : list_items (v))
      out.push_back (Conv<T>::from (x));
    return out;
  }
  static Value
  to (std::vector<T> xs)
  {
    std::vector<Value> items;
    for (auto &x : xs)
      items.push_back (Conv<T>::to (std::move (x)));
    return make_list (items);
  }
};
template <class A, class B> struct Conv<std::pair<A, B>>
{
  static std::string
  type (TypeVars &tv)
  {
    auto a = Conv<A>::type (tv);
    return "(" + a + ", " + Conv<B>::type (tv) + ")";
  }
  static std::pair<A, B>
  from (const Value &v)
  {
    auto t = agg_of (v);
    return { Conv<A>::from (t->items[0]), Conv<B>::from (t->items[1]) };
  }
  static Value
  to (std::pair<A, B> p)
  {
    return make_tuple ({ Conv<A>::to (std::move (p.first)), Conv<B>::to (std::move (p.second)) });
  }
};
template <class T> struct Conv<dsl::Result<T, std::string>>
{
  static std::string
  type (TypeVars &tv)
  {
    return "Result (" + Conv<T>::type (tv) + ") String";
  }
  static Value
  to (const dsl::Result<T, std::string> &r)
  {
    if (r.is_ok ())
      return make_ok (Conv<T>::to (r.unwrap ()));
    return make_err (make_str (r.unwrap_err ()));
  }
};

template <class F> struct FnTraits : FnTraits<decltype (&F::operator())>
{
};
template <class C, class R, class... A> struct FnTraits<R (C::*) (A...) const>
{
  using Ret = R;
  using Args = std::tuple<A...>;
};
template <class C, class R, class... A> struct FnTraits<R (C::*) (A...)>
{
  using Ret = R;
  using Args = std::tuple<A...>;
};
template <class R, class... A> struct FnTraits<R (*) (A...)>
{
  using Ret = R;
  using Args = std::tuple<A...>;
};

template <class Tuple, std::size_t... I>
std::string
arg_types (TypeVars &tv, std::index_sequence<I...>)
{
  std::string s;
  ((s += Conv<std::decay_t<std::tuple_element_t<I, Tuple>>>::type (tv) + " -> "), ...);
  return s;
}

template <class F, class Tuple, std::size_t... I>
Value
invoke (F &f, std::vector<Value> &args, std::index_sequence<I...>)
{
  using R = typename FnTraits<F>::Ret;
  if constexpr (std::is_void_v<R>)
    {
      f (Conv<std::decay_t<std::tuple_element_t<I, Tuple>>>::from (args[I])...);
      return Value::unit ();
    }
  else
    return Conv<std::decay_t<R>>::to (
        f (Conv<std::decay_t<std::tuple_element_t<I, Tuple>>>::from (args[I])...));
}

} // namespace host

// ============================================================
//  Session — the embedding API (§10)
// ============================================================

using EvalResult = dsl::Result<Value, std::vector<Diagnostic>>;

enum class RunMode
{
  Repl,  ///< print summaries, push results on the stack
  Quiet, ///< files and imports: side effects only
  Eval   ///< embedding: return the last value
};

struct RunReport
{
  std::string text{};
  std::vector<Diagnostic> errors{};
  std::vector<Diagnostic> warnings{};
  std::optional<Value> value{};
  TypeP type{};
  bool
  ok () const
  {
    return errors.empty ();
  }
};

class Session
{
public:
  Session ()
  {
    checker_.find_prim = [this] (const std::string &n) { return find_primitive (n); };
    checker_.prim_type = [this] (Primitive *p, const Span &sp) { return primitive_type (p, sp); };
    interp_.session = this;
    interp_.allowed = [this] (Primitive *p) { return primitive_allowed (p); };
    interp_.global_lookup = [this] (const std::string &n) -> const Cell *
      {
        auto it = globals_.find (n);
        return it == globals_.end () ? nullptr : it->second.get ();
      };
    root_ = std::make_shared<Frame> ();
    interp_.module_lock = &module_lock_;
    output_ = [] (std::string_view s) { std::cout << s; };
    register_core ();
    auto r = run_source (builtin_source (), "<builtin>", RunMode::Quiet);
    (void)r;
  }

  Session (const Session &) = delete;
  Session &operator= (const Session &) = delete;
  ~Session () { close (); }

  /// Resource shutdown is two-phase: wake/cancel everything before joining
  /// workers. Callbacks must be non-throwing and must not re-enter evaluation.
  void on_close (std::function<void ()> stop, std::function<void ()> wait = {})
  {
    std::lock_guard<std::mutex> g (resource_mutex_);
    if (closing_) detail::fail ("M0404", "the session is closing");
    resources_.push_back ({ std::move (stop), std::move (wait) });
  }
  void close () noexcept
  {
    std::vector<std::pair<std::function<void ()>, std::function<void ()>>> resources;
    {
      std::lock_guard<std::mutex> g (resource_mutex_);
      if (closing_) return;
      closing_ = true;
      resources.swap (resources_);
    }
    for (auto &r : resources) try { if (r.first) r.first (); } catch (...) {}
    for (auto &r : resources) try { if (r.second) r.second (); } catch (...) {}
  }

  /// MetaML source evaluated by every fresh session (identical in MetaML.d).
  static std::string
  builtin_source ()
  {
    return "type Option 'a := | None | Some 'a\n"
           "type Result 'a 'e := | Ok 'a | Err 'e\n";
  }

  // ---------------- standard library ----------------
  std::vector<std::string>
  stdlib_candidates () const
  {
    std::vector<std::string> c;
    if (const char *s = std::getenv ("METAML_STDLIB"))
      c.push_back (s);
    if (const char *p = std::getenv ("METAML_PREFIX"))
      c.push_back (std::string (p) + "/stdlib/" + dialect);
    auto exe = executable_dir ();
    if (!exe.empty ())
      {
        c.push_back (exe + "/../share/metaml/stdlib/" + dialect);
        c.push_back (exe + "/../metalib");
      }
#ifdef METAML_DEFAULT_STDLIB
    c.push_back (METAML_DEFAULT_STDLIB);
#endif
    c.push_back ("metalib");
    return c;
  }

  /// Loads metalib (the prelude and everything it imports).
  dsl::Result<std::string, std::vector<Diagnostic>>
  load_stdlib (const std::string &dir = "")
  {
    using R = dsl::Result<std::string, std::vector<Diagnostic>>;
    std::string found;
    std::vector<std::string> cands = dir.empty () ? stdlib_candidates ()
                                                  : std::vector<std::string>{ dir };
    for (auto &c : cands)
      {
        std::error_code ec;
        if (std::filesystem::is_regular_file (std::filesystem::path (c) / "prelude" / "mod.ml",
                                              ec))
          {
            found = c;
            break;
          }
      }
    if (found.empty ())
      return R::from_err ({ detail::make_diag (
          "M0505", "cannot locate metalib (set METAML_STDLIB to the metalib directory)", {}) });
    // Restore temporary configuration even if an embedding callback throws.
    auto previous_dir = stdlib_dir_;
    bool loaded = false;
    struct Restore
    {
      Session &s;
      std::string &directory;
      bool &loaded;
      bool quiet;
      ~Restore ()
      {
        s.checker_.quiet = quiet;
        if (!loaded) s.stdlib_dir_.swap (directory);
      }
    } restore{ *this, previous_dir, loaded, checker_.quiet };
    stdlib_dir_ = found;
    checker_.quiet = true;
    auto r = import_module ("prelude", "");
    if (!r.ok ())
      return R::from_err (r.errors);
    stdlib_loaded_ = true;
    loaded = true;
    return R::from_ok (found);
  }
  const std::string &
  stdlib_dir () const
  {
    return stdlib_dir_;
  }
  bool
  stdlib_loaded () const
  {
    return stdlib_loaded_;
  }

  // ---------------- evaluation ----------------
  /// Evaluates source; returns the value of the last item.
  EvalResult
  eval (std::string_view source, const std::string &file = "<eval>")
  {
    auto r = run_source (source, file, RunMode::Eval);
    if (!r.ok ())
      return EvalResult::from_err (r.errors);
    return EvalResult::from_ok (r.value ? *r.value : Value::unit ());
  }

  /// Infers the type of an expression without evaluating it.
  dsl::Result<std::string, std::vector<Diagnostic>>
  type_of (std::string_view expr)
  {
    using R = dsl::Result<std::string, std::vector<Diagnostic>>;
    sources_["<type>"] = std::string (expr);
    auto pe = parse_expression (expr, "<type>");
    if (pe.is_err ())
      return R::from_err (pe.unwrap_err ());
    try
      {
        auto t = check_expr (pe.unwrap ());
        checker_.u.commit ();
        return R::from_ok (show_type (t));
      }
    catch (const detail::MetaError &e)
      {
        checker_.u.commit ();
        return R::from_err ({ e.diag });
      }
  }

  dsl::Result<bool, std::vector<Diagnostic>>
  load_file (const std::string &path)
  {
    using R = dsl::Result<bool, std::vector<Diagnostic>>;
    auto r = run_file (path, RunMode::Quiet);
    if (!r.ok ())
      return R::from_err (r.errors);
    return R::from_ok (true);
  }

  RunReport
  run_file (const std::string &path, RunMode mode)
  {
    RunReport rep;
    auto text = dsl::load_parse_input_file (path);
    if (text.is_err ())
      {
        rep.errors.push_back (
            detail::make_diag ("M0603", "cannot read file `" + path + "`", {}));
        return rep;
      }
    std::error_code ec;
    auto canon = std::filesystem::weakly_canonical (path, ec).string ();
    auto key = ec ? path : canon;
    bool was_imported = imported_.count (key) != 0;
    imported_.insert (key);
    auto result = run_source (text.unwrap (), path, mode);
    if (!result.ok () && !was_imported)
      imported_.erase (key);
    return result;
  }

  RunReport
  run_source (std::string_view source, const std::string &file, RunMode mode)
  {
    sources_[file] = std::string (source);
    auto parsed = parse (source, file);
    if (!parsed.ok ())
      {
        RunReport rep;
        rep.errors = parsed.diagnostics;
        return rep;
      }
    return run_program (parsed.program, mode);
  }

  /// Checks and executes a parsed program unit by unit.
  RunReport
  run_program (const Program &prog, RunMode mode)
  {
    RunReport rep;
    auto keep = std::make_shared<Program> (prog);
    programs_.push_back (keep);
    sources_[prog.file] = prog.source;
    const auto &items = keep->items;
    for (auto [b, e] : Checker::units (items))
      {
        const ItemP &it = items[b];
        if (it->kind == IK::Import)
          {
            auto r = import_module (it->text, prog.file, it->span);
            if (!r.ok ())
              {
                append_errors (rep, r.errors);
                return rep;
              }
            continue;
          }
        if (it->kind == IK::Directive)
          {
            auto r = directive (it->name, it->text);
            rep.text += r.text;
            if (!r.ok ())
              {
                append_errors (rep, r.errors);
                return rep;
              }
            continue;
          }
        auto unit_scope = Scope::child (checker_.global);
        checker_.warnings.clear ();
        UnitInfo info;
        try
          {
            info = checker_.check_unit (items, b, e, unit_scope, nullptr, true);
          }
        catch (const detail::MetaError &err)
          {
            checker_.u.commit ();
            for (auto &w : checker_.warnings)
              rep.warnings.push_back (w);
            rep.errors.push_back (err.diag);
            return rep;
          }
        checker_.u.commit ();
        Checker::commit_scope (unit_scope, checker_.global);
        for (auto &w : checker_.warnings)
          {
            rep.warnings.push_back (w);
            if (mode != RunMode::Eval)
              rep.text += render (w);
          }
        checker_.warnings.clear ();
        try
          {
            exec_unit (items, b, e, info, mode, rep);
          }
        catch (const detail::MetaError &err)
          {
            interp_.stack.clear ();
            stack_types_.clear ();
            interp_.call_depth = 0;
            interp_.eval_depth = 0;
            rep.errors.push_back (err.diag);
            return rep;
          }
      }
    return rep;
  }

  // ---------------- primitives ----------------
  /// Registers a host primitive with an explicit MetaML type (trusted).
  void
  register_primitive (const std::string &name, const std::string &type, int arity, PrimFn fn)
  {
    add_primitive (name, type, arity, std::move (fn), false);
    allow_.push_back (name);
  }

  /// Registers a typed host function; the MetaML type is derived from the
  /// C++ signature (Int, Float, Bool, String, Option, lists, pairs, Value).
  template <class F>
  void
  register_primitive (const std::string &name, F f)
  {
    using Traits = host::FnTraits<std::decay_t<F>>;
    using Args = typename Traits::Args;
    using Ret = typename Traits::Ret;
    constexpr std::size_t N = std::tuple_size_v<Args>;
    host::TypeVars tv;
    std::string type = host::arg_types<Args> (tv, std::make_index_sequence<N> ());
    if constexpr (N == 0)
      type = "Unit -> ";
    if constexpr (std::is_void_v<Ret>)
      type += "Unit";
    else
      type += host::Conv<std::decay_t<Ret>>::type (tv);
    auto fn = std::make_shared<std::decay_t<F>> (std::move (f));
    PrimFn pf = [fn] (PrimContext &, std::vector<Value> &args) -> Value
      {
        return host::invoke<std::decay_t<F>, Args> (*fn, args, std::make_index_sequence<N> ());
      };
    register_primitive (name, type, N == 0 ? 1 : static_cast<int> (N), std::move (pf));
  }

  /// Registers a sandboxed library primitive (metalib host modules): visible
  /// only when allowed by the allowlist (§10.4).
  void
  install_primitive (const std::string &name, const std::string &type, int arity, PrimFn fn)
  {
    add_primitive (name, type, arity, std::move (fn), false);
  }

  /// Declares MetaML source (types, helpers) on behalf of a host module.
  RunReport
  declare (std::string_view source, const std::string &file)
  {
    struct RestoreQuiet
    {
      bool &value;
      bool saved;
      ~RestoreQuiet () { value = saved; }
    } restore{ checker_.quiet, checker_.quiet };
    checker_.quiet = true;
    auto r = run_source (source, file, RunMode::Quiet);
    return r;
  }

  void
  set_primitive_allowlist (std::vector<std::string> names)
  {
    allow_ = std::move (names);
    allow_all_ = false;
  }
  void
  allow_all_primitives (bool on = true)
  {
    allow_all_ = on;
  }
  bool
  primitive_allowed (const Primitive *p) const
  {
    if (p->core || allow_all_)
      return true;
    for (auto &a : allow_)
      {
        if (a == "*" || a == p->name)
          return true;
        if (a.size () >= 2 && a.compare (a.size () - 2, 2, ".*") == 0
            && p->name.compare (0, a.size () - 1, a, 0, a.size () - 1) == 0)
          return true;
      }
    return false;
  }
  Primitive *
  find_primitive (const std::string &name)
  {
    auto it = prims_.find (name);
    return it == prims_.end () ? nullptr : it->second.get ();
  }
  std::vector<std::string>
  primitive_names () const
  {
    std::vector<std::string> out;
    for (auto &kv : prims_)
      out.push_back (kv.first);
    return out;
  }

  // ---------------- configuration ----------------
  void
  set_output (std::function<void (std::string_view)> out)
  {
    output_ = std::move (out);
  }
  void
  write (std::string_view s)
  {
    std::lock_guard<std::mutex> g (out_mutex_);
    output_ (s);
  }
  /// Flushes the process's standard output (used before handing the
  /// terminal to a child process or exiting).
  void
  flush_output ()
  {
    std::lock_guard<std::mutex> g (out_mutex_);
    std::cout << std::flush;
  }

  /// Wraps a host function as a MetaML function value.
  Value
  make_function (const std::string &name, int arity, PrimFn fn)
  {
    auto p = std::make_shared<Primitive> ();
    p->name = name;
    p->arity = arity < 1 ? 1 : arity;
    p->fn = std::move (fn);
    p->core = true;
    auto o = std::make_shared<PrimObj> ();
    o->prim = p.get ();
    o->owned = p;
    Value v;
    v.kind = VK::Prim;
    v.obj = o;
    return v;
  }

  /// A fresh interpreter sharing this session (for host threads).
  std::unique_ptr<Interp>
  spawn_interp (bool background = false)
  {
    auto in = std::make_unique<Interp> ();
    in->session = this;
    auto allow = allow_;
    bool all = allow_all_;
    in->allowed = [allow, all, background] (Primitive *p)
      {
        // Checker/loader APIs mutate or inspect the foreground session.
        if (p->name.compare (0, 7, "Loader.") == 0 || p->name.compare (0, 7, "Module.") == 0)
          return false;
        if (background && (p->name == "Threading.spawn" || p->name == "Fiber.create")) return false;
        if (p->core || all) return true;
        for (const auto &n : allow)
          if (n == "*" || n == p->name || (n.size () >= 2 && n.substr (n.size () - 2) == ".*"
              && p->name.compare (0, n.size () - 1, n, 0, n.size () - 1) == 0)) return true;
        return false;
      };
    auto globals = std::make_shared<decltype (globals_)> (globals_);
    in->global_lookup = [globals] (const std::string &n) -> const Cell *
      {
        auto it = globals->find (n);
        return it == globals->end () ? nullptr : it->second.get ();
      };
    in->max_depth = interp_.max_depth < 20000 ? interp_.max_depth : 20000;
    in->module_lock = &module_lock_;
    return in;
  }

  /// Command-line arguments visible to scripts (Process.args).
  std::vector<std::string> args{};

  std::vector<std::string>
  imported_files () const
  {
    return std::vector<std::string> (imported_.begin (), imported_.end ());
  }
  std::vector<std::string>
  search_paths () const
  {
    std::vector<std::string> out = search_path_;
    if (!stdlib_dir_.empty ())
      out.push_back (stdlib_dir_);
    return out;
  }
  /// Where `import "spec"` would resolve (canonical path), if anywhere.
  std::optional<std::string>
  resolve_import (const std::string &spec, const std::string &from = "") const
  {
    auto f = find_import (spec, from);
    if (f.empty ())
      return std::nullopt;
    return f;
  }
  void
  disable_warning (const std::string &code)
  {
    checker_.disabled.push_back (code);
  }
  void
  set_max_depth (int d)
  {
    interp_.max_depth = d;
  }
  void
  add_search_path (const std::string &dir)
  {
    search_path_.push_back (dir);
  }
  Checker &
  checker ()
  {
    return checker_;
  }
  Interp &
  interp ()
  {
    return interp_;
  }
  const FrameP &
  root () const
  {
    return root_;
  }

  /// Applies a MetaML function value from host code.
  EvalResult
  call (const Value &f, std::vector<Value> args)
  {
    try
      {
        return EvalResult::from_ok (interp_.apply (f, std::move (args)));
      }
    catch (const detail::MetaError &e)
      {
        interp_.call_depth = 0;
        interp_.eval_depth = 0;
        return EvalResult::from_err ({ e.diag });
      }
  }

  /// Value bound to a global name (if any).
  std::optional<Value>
  global (const std::string &name) const
  {
    auto it = globals_.find (name);
    if (it == globals_.end () || !it->second->defined)
      return std::nullopt;
    return it->second->value;
  }

  // ---------------- diagnostics ----------------
  void
  remember_source (const std::string &file, std::string_view text)
  {
    sources_[file] = std::string (text);
  }
  std::string
  render (const Diagnostic &d) const
  {
    auto it = sources_.find (d.primary_span.file);
    return dsl::render_diagnostic (d, it == sources_.end () ? std::string_view{}
                                                            : std::string_view (it->second));
  }

  // ---------------- REPL directives (§5.10) ----------------
  RunReport
  directive (const std::string &name, const std::string &arg)
  {
    auto rep = directive_impl (name, arg);
    for (auto &d : rep.errors)
      if (d.primary_span.file.empty ())
        d.primary_span.file = "<directive>";
    return rep;
  }

  RunReport
  directive_impl (const std::string &name, const std::string &arg)
  {
    RunReport rep;
    try
      {
        if (name == "type")
          {
            auto r = type_of (arg);
            if (r.is_err ())
              rep.errors = r.unwrap_err ();
            else
              rep.text = "- : " + r.unwrap () + "\n";
            return rep;
          }
        if (name == "trace")
          return trace (arg);
        if (name == "load")
          {
            std::string path = unquote (arg);
            if (path.empty ())
              {
                rep.errors.push_back (detail::make_diag (
                    "M0603", "`#load` expects a file name, e.g. `#load \"file.ml\"`", {}));
                return rep;
              }
            last_loaded_ = path;
            return run_file (path, RunMode::Quiet);
          }
        if (name == "reload")
          {
            if (last_loaded_.empty ())
              {
                rep.errors.push_back (
                    detail::make_diag ("M0602", "nothing to reload; use `#load` first", {}));
                return rep;
              }
            return run_file (last_loaded_, RunMode::Quiet);
          }
        if (name == "stack")
          {
            if (interp_.stack.empty ())
              rep.text = "(stack is empty)\n";
            for (std::size_t i = 0; i < interp_.stack.size (); ++i)
              rep.text += "  [" + std::to_string (i) + "] " + show_value (interp_.stack[i])
                          + (i < stack_types_.size () ? " : " + stack_types_[i] : "") + "\n";
            return rep;
          }
        if (name == "clear")
          {
            interp_.stack.clear ();
            stack_types_.clear ();
            return rep;
          }
        if (name == "ast")
          {
            sources_["<ast>"] = arg;
            auto p = parse (arg, "<ast>");
            if (!p.ok ())
              rep.errors = p.diagnostics;
            else
              rep.text = dump_ast (p.program) + "\n";
            return rep;
          }
        if (name == "quit")
          {
            quit_requested_ = true;
            return rep;
          }
        rep.errors.push_back (detail::make_diag (
            "M0601", "unknown directive `#" + name + "`; try `#help`", {}));
      }
    catch (const detail::MetaError &e)
      {
        rep.errors.push_back (e.diag);
      }
    return rep;
  }

  bool
  quit_requested () const
  {
    return quit_requested_;
  }
  const std::string &
  last_loaded () const
  {
    return last_loaded_;
  }

  /// Completion candidates: identifiers, module members, keywords.
  std::vector<std::string>
  completions (std::string_view prefix) const
  {
    std::vector<std::string> out;
    std::string p (prefix);
    auto starts = [] (const std::string &s, const std::string &pre)
      { return s.compare (0, pre.size (), pre) == 0; };
    auto dot = p.rfind ('.');
    if (dot != std::string::npos)
      {
        std::string mpath = p.substr (0, dot), rest = p.substr (dot + 1);
        std::vector<std::string> segs;
        std::size_t s = 0;
        while (true)
          {
            auto d = mpath.find ('.', s);
            segs.push_back (mpath.substr (s, d == std::string::npos ? d : d - s));
            if (d == std::string::npos)
              break;
            s = d + 1;
          }
        const ScopeP *m = checker_.global->module (segs[0]);
        ScopeP cur = m ? *m : nullptr;
        for (std::size_t i = 1; cur && i < segs.size (); ++i)
          {
            auto n = cur->modules.find (segs[i]);
            cur = n ? *n : nullptr;
          }
        if (cur)
          {
            for (auto &k : cur->vars.keys ())
              if (starts (k, rest))
                out.push_back (mpath + "." + k);
            for (auto &k : cur->modules.keys ())
              if (starts (k, rest))
                out.push_back (mpath + "." + k);
            for (auto &k : cur->ctors.keys ())
              if (starts (k, rest))
                out.push_back (mpath + "." + k);
          }
        for (auto &kv : prims_)
          if (starts (kv.first, p) && primitive_allowed (kv.second.get ()))
            out.push_back (kv.first);
      }
    else
      {
        for (auto &k : checker_.global->vars.keys ())
          if (starts (k, p))
            out.push_back (k);
        for (auto &k : checker_.global->modules.keys ())
          if (starts (k, p))
            out.push_back (k);
        for (auto &k : checker_.global->ctors.keys ())
          if (starts (k, p))
            out.push_back (k);
        static const char *kws[]
            = { "module", "type", "sig", "struct", "end", "val", "let", "rec", "in", "if",
                "then", "else", "match", "with", "fn", "fun", "import", "include", "open",
                "do", "true", "false", "when", "for" };
        for (const char *k : kws)
          if (starts (k, p))
            out.push_back (k);
      }
    std::sort (out.begin (), out.end ());
    out.erase (std::unique (out.begin (), out.end ()), out.end ());
    return out;
  }

  void
  set_trace_depth (int d)
  {
    interp_.trace_limit = d;
  }

private:
  Checker checker_{};
  Interp interp_{};
  FrameP root_{};
  std::map<std::string, std::unique_ptr<Primitive>> prims_{};
  // ASTs and first-class functions retain raw primitive pointers.
  std::vector<std::unique_ptr<Primitive>> retired_prims_{};
  std::unordered_map<std::string, CellP> globals_{};
  std::vector<std::shared_ptr<Program>> programs_{};
  std::map<std::string, std::string> sources_{};
  std::set<std::string> imported_{};
  std::vector<std::string> search_path_{};
  std::vector<std::string> allow_{};
  bool allow_all_ = false;
  std::string stdlib_dir_{};
  bool stdlib_loaded_ = false;
  std::string last_loaded_{};
  bool quit_requested_ = false;
  std::vector<std::string> stack_types_{};
  std::function<void (std::string_view)> output_{};
  std::mutex out_mutex_{};
  std::recursive_mutex module_lock_{};
  std::mutex resource_mutex_{};
  bool closing_ = false;
  std::vector<std::pair<std::function<void ()>, std::function<void ()>>> resources_{};

  static std::string
  executable_dir ()
  {
#if defined(__linux__)
    std::error_code ec;
    auto p = std::filesystem::read_symlink ("/proc/self/exe", ec);
    if (!ec)
      return p.parent_path ().string ();
#endif
    return "";
  }

  static std::string
  unquote (const std::string &a)
  {
    if (a.size () >= 2 && a.front () == '"' && a.back () == '"')
      return a.substr (1, a.size () - 2);
    return a;
  }

  void
  append_errors (RunReport &rep, const std::vector<Diagnostic> &errs)
  {
    for (auto &d : errs)
      rep.errors.push_back (d);
  }

  void
  add_primitive (const std::string &name, const std::string &type, int arity, PrimFn fn,
                 bool core)
  {
    if (name.empty () || type.empty () || !fn)
      throw std::invalid_argument ("primitive requires a name, type and callback");
    auto p = std::make_unique<Primitive> ();
    p->name = name;
    p->type_src = type;
    p->arity = arity < 1 ? 1 : arity;
    p->fn = std::move (fn);
    p->core = core;
    if (auto old = prims_.find (name); old != prims_.end ())
      retired_prims_.push_back (std::move (old->second));
    prims_[name] = std::move (p);
  }
  void
  core (const std::string &name, const std::string &type, int arity, PrimFn fn)
  {
    add_primitive (name, type, arity, std::move (fn), true);
  }

  TypeP
  primitive_type (Primitive *p, const Span &sp)
  {
    if (p->scheme)
      return p->scheme;
    if (p->resolving)
      checker_.error ("M0404", "primitive `" + p->name + "` has a recursive type", sp);
    p->resolving = true;
    struct RestoreResolution
    {
      Primitive &primitive;
      int &level;
      int saved;
      ~RestoreResolution () { primitive.resolving = false; level = saved; }
    } restore{ *p, checker_.level, checker_.level };
    auto te = parse_type_expr (p->type_src, "<primitive " + p->name + ">");
    if (te.is_err ())
      {
        p->resolving = false;
        checker_.error ("M0404", "primitive `" + p->name + "` has an invalid type `"
                                     + p->type_src + "`",
                        sp);
      }
    int saved = checker_.level;
    try
      {
        checker_.level = 0;
        p->scheme = checker_.sig_scheme (te.unwrap (), checker_.global);
        checker_.level = saved;
      }
    catch (const detail::MetaError &e)
      {
        checker_.level = saved;
        p->resolving = false;
        checker_.error ("M0404", "primitive `" + p->name + "` has an invalid type: "
                                     + e.diag.message,
                        sp);
      }
    p->resolving = false;
    return p->scheme;
  }

  TypeP
  check_expr (const ExprP &e)
  {
    auto scope = Scope::child (checker_.global);
    auto it = std::make_shared<Item> ();
    it->kind = IK::Expr;
    it->body = e;
    it->span = e->span;
    std::vector<ItemP> items{ it };
    auto info = checker_.check_unit (items, 0, 1, scope, nullptr, true);
    return info.expr_type;
  }

  RunReport
  trace (const std::string &arg)
  {
    RunReport rep;
    sources_["<trace>"] = arg;
    auto pe = parse_expression (arg, "<trace>");
    if (pe.is_err ())
      {
        rep.errors = pe.unwrap_err ();
        return rep;
      }
    TypeP t;
    try
      {
        t = check_expr (pe.unwrap ());
        checker_.u.commit ();
      }
    catch (const detail::MetaError &e)
      {
        checker_.u.commit ();
        rep.errors.push_back (e.diag);
        return rep;
      }
    std::string lines;
    interp_.tracing = true;
    interp_.trace_base = interp_.call_depth;
    interp_.trace_sink = [&lines] (const std::string &l) { lines += l + "\n"; };
    try
      {
        Value v = interp_.eval (pe.unwrap (), root_);
        interp_.tracing = false;
        rep.text = lines + "- : " + show_type (t) + " = " + show_value (v) + "\n";
      }
    catch (const detail::MetaError &e)
      {
        interp_.tracing = false;
        interp_.call_depth = 0;
        interp_.eval_depth = 0;
        rep.text = lines;
        rep.errors.push_back (e.diag);
      }
    return rep;
  }

  RunReport
  import_module (const std::string &spec, const std::string &from, const Span &sp = {})
  {
    RunReport rep;
    std::string found = find_import (spec, from);
    if (found.empty ())
      {
        rep.errors.push_back (
            detail::make_diag ("M0505", "cannot find the module file `" + spec + "`", sp));
        return rep;
      }
    if (imported_.count (found))
      return rep;
    imported_.insert (found);
    auto text = dsl::load_parse_input_file (found);
    if (text.is_err ())
      {
        imported_.erase (found);
        rep.errors.push_back (
            detail::make_diag ("M0505", "cannot read the module file `" + found + "`", sp));
        return rep;
      }
    std::string label = found;
    if (!stdlib_dir_.empty ())
      {
        std::error_code ec;
        auto base = std::filesystem::weakly_canonical (stdlib_dir_, ec).string ();
        if (!ec && found.compare (0, base.size (), base) == 0)
          label = "metalib" + found.substr (base.size ());
      }
    auto result = run_source (text.unwrap (), label, RunMode::Quiet);
    if (!result.ok ())
      imported_.erase (found);
    return result;
  }

  std::string
  find_import (const std::string &spec, const std::string &from) const
  {
    std::vector<std::filesystem::path> bases;
    if (!from.empty () && from[0] != '<')
      bases.push_back (std::filesystem::path (from).parent_path ());
    std::error_code cec;
    bases.push_back (std::filesystem::current_path (cec));
    for (auto &s : search_path_)
      bases.push_back (s);
    if (!stdlib_dir_.empty ())
      bases.push_back (stdlib_dir_);
    for (auto &b : bases)
      for (auto cand : { b / spec, b / (spec + ".ml"), b / (spec + ".mml"), b / spec / "mod.ml", b / spec / "mod.mml" })
        {
          std::error_code ec;
          if (std::filesystem::is_regular_file (cand, ec))
            return std::filesystem::weakly_canonical (cand, ec).string ();
        }
    return "";
  }

  void
  bind_global (const CellP &cell)
  {
    globals_[cell->name] = cell;
  }

  void
  exec_unit (const std::vector<ItemP> &items, std::size_t b, std::size_t e,
             const UnitInfo &info, RunMode mode, RunReport &rep)
  {
    const ItemP &first = items[b];
    bool repl = mode == RunMode::Repl;
    switch (first->kind)
      {
      case IK::Def:
      case IK::Sig:
        {
          bool any = false;
          for (std::size_t i = b; i < e; ++i)
            {
              auto &it = items[i];
              if (it->kind != IK::Def)
                continue;
              any = true;
              Value v = interp_.eval (it->value, root_);
              auto &cell = it->cells[0];
              cell->value = v;
              cell->defined = true;
              bind_global (cell);
              rep.value = v;
            }
          if (any && repl)
            {
              interp_.stack.clear ();
              stack_types_.clear ();
            }
          if (repl)
            for (auto &[name, t] : info.defs)
              {
                std::string line = "val " + name + " : " + show_type (t);
                auto g = globals_.find (name);
                if (g != globals_.end () && !is_callable (g->second->value))
                  line += " = " + show_value (g->second->value);
                rep.text += line + "\n";
              }
          if (!info.defs.empty ())
            rep.type = info.defs.back ().second;
          return;
        }
      case IK::Type:
        if (repl)
          for (std::size_t i = b; i < e; ++i)
            {
              std::string line = "type " + items[i]->name;
              for (auto &p : items[i]->tparams)
                line += " " + (p[0] == '\'' ? p : "'" + p);
              rep.text += line + "\n";
            }
        return;
      case IK::Module:
        {
        std::lock_guard<std::recursive_mutex> guard (module_lock_);
        root_->mods.push_back ({ first->name, interp_.make_module (first, root_) });
        if (repl)
          rep.text += "module " + first->name + "\n";
        return;
        }
      case IK::Functor:
        {
          std::lock_guard<std::recursive_mutex> guard (module_lock_);
          auto f = std::make_shared<FunctorObj> ();
          f->item = first;
          f->env = root_;
          root_->functors.push_back ({ first->name, f });
          if (repl)
            rep.text += "functor " + first->name + "\n";
          return;
        }
      case IK::ModType:
        if (repl)
          rep.text += "module type " + first->name + "\n";
        return;
      case IK::Open:
      case IK::Include:
        {
          std::lock_guard<std::recursive_mutex> guard (module_lock_);
          auto m = interp_.resolve_module (first->path, root_, first->span);
          interp_.force (m, first->span);
          for (auto &cell : first->cells)
            {
              for (std::size_t k = m->fields.size (); k-- > 0;)
                if (m->fields[k].first == cell->name)
                  {
                    cell->value = m->fields[k].second;
                    cell->defined = true;
                    break;
                  }
              bind_global (cell);
            }
          for (auto &kv : m->mods)
            root_->mods.push_back (kv);
          for (auto &kv : m->functors)
            root_->functors.push_back (kv);
          return;
        }
      case IK::Expr:
        {
          Value v = interp_.eval (first->body, root_);
          rep.value = v;
          rep.type = info.expr_type;
          if (repl)
            {
              interp_.stack.push_back (v);
              stack_types_.push_back (show_type (info.expr_type));
              auto t = prune (info.expr_type);
              if (!(t->kind == TK::Con && t->stamp == stamps::Unit))
                rep.text += "- : " + show_type (info.expr_type) + " = " + show_value (v) + "\n";
            }
          return;
        }
      default:
        return;
      }
  }

  void register_core ();
};

inline Value
PrimContext::apply (const Value &f, std::vector<Value> args)
{
  return interp.apply (f, std::move (args), span);
}
inline void
PrimContext::print (std::string_view text)
{
  session.write (text);
}

/// REPL-style rendering of an evaluation result (or its diagnostics).
inline std::string
to_string (const Value &v)
{
  return show_value (v);
}
inline std::string
to_string (const EvalResult &r)
{
  if (r.is_ok ())
    return show_value (r.unwrap ());
  std::string out;
  for (auto &d : r.unwrap_err ())
    out += d.headline () + "\n";
  return out;
}

// ============================================================
//  Core host primitives (registered by both dialects, §18.2)
// ============================================================

namespace detail
{

inline std::int64_t
fnv1a (std::string_view s)
{
  std::uint64_t h = 14695981039346656037ULL;
  for (unsigned char c : s)
    {
      h ^= c;
      h *= 1099511628211ULL;
    }
  return static_cast<std::int64_t> (h);
}

inline std::string
ascii_map (std::string s, bool upper)
{
  for (auto &c : s)
    {
      if (upper && c >= 'a' && c <= 'z')
        c = static_cast<char> (c - 32);
      else if (!upper && c >= 'A' && c <= 'Z')
        c = static_cast<char> (c + 32);
    }
  return s;
}

inline std::string
trim_ws (const std::string &s)
{
  std::size_t b = 0, e = s.size ();
  auto ws = [] (char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
  while (b < e && ws (s[b]))
    ++b;
  while (e > b && ws (s[e - 1]))
    --e;
  return s.substr (b, e - b);
}

/// Stable merge sort with a three-way comparator (same algorithm in D).
template <class Cmp>
void
merge_sort (std::vector<Value> &v, Cmp cmp)
{
  if (v.size () < 2)
    return;
  std::vector<Value> tmp (v.size ());
  for (std::size_t width = 1; width < v.size (); width *= 2)
    {
      for (std::size_t lo = 0; lo < v.size (); lo += 2 * width)
        {
          std::size_t mid = std::min (lo + width, v.size ());
          std::size_t hi = std::min (lo + 2 * width, v.size ());
          std::size_t i = lo, j = mid, k = lo;
          while (i < mid && j < hi)
            {
              if (cmp (v[j], v[i]) < 0)
                tmp[k++] = v[j++];
              else
                tmp[k++] = v[i++];
            }
          while (i < mid)
            tmp[k++] = v[i++];
          while (j < hi)
            tmp[k++] = v[j++];
        }
      std::swap (v, tmp);
    }
}

inline std::vector<std::string>
split_string (const std::string &sep, const std::string &s)
{
  std::vector<std::string> out;
  if (sep.empty ())
    {
      for (char c : s)
        out.push_back (std::string (1, c));
      return out;
    }
  std::size_t start = 0;
  while (true)
    {
      auto p = s.find (sep, start);
      if (p == std::string::npos)
        {
          out.push_back (s.substr (start));
          return out;
        }
      out.push_back (s.substr (start, p - start));
      start = p + sep.size ();
    }
}

inline std::string
replace_all (const std::string &from, const std::string &to, const std::string &s)
{
  if (from.empty ())
    return s;
  std::string out;
  std::size_t start = 0;
  while (true)
    {
      auto p = s.find (from, start);
      if (p == std::string::npos)
        {
          out += s.substr (start);
          return out;
        }
      out += s.substr (start, p - start);
      out += to;
      start = p + from.size ();
    }
}

} // namespace detail

inline void
Session::register_core ()
{
  using V = std::vector<Value>;
  using C = PrimContext;
  auto S = [] (const Value &v) -> const std::string & { return str_of (v); };
  auto F = [] (const Value &v) { return v.kind == VK::Int ? static_cast<double> (v.i) : v.f; };

  // ---- Core ----
  core ("Core.show", "'a -> String", 1,
        [] (C &, V &a) { return make_str (show_value (a[0])); });
  core ("Core.error", "String -> 'a", 1,
        [S] (C &c, V &a) -> Value { c.error ("M0407", S (a[0])); });
  core ("Core.compare", "'a -> 'a -> Int", 2,
        [] (C &c, V &a) { return Value::integer (compare_values (a[0], a[1], c.span)); });
  core ("Core.equal", "'a -> 'a -> Bool", 2,
        [] (C &c, V &a) { return Value::boolean (values_equal (a[0], a[1], c.span)); });
  core ("Core.hash", "'a -> Int", 1,
        [] (C &, V &a) { return Value::integer (detail::fnv1a (show_value (a[0]))); });
  core ("Core.ignore", "'a -> Unit", 1, [] (C &, V &) { return Value::unit (); });
  core ("Core.version", "Unit -> String", 1, [] (C &, V &) { return make_str (version); });

  // ---- Int ----
  core ("Int.parse", "String -> Option Int", 1,
        [S] (C &, V &a)
          {
            auto r = detail::parse_int (S (a[0]));
            return make_option (r ? std::optional<Value> (Value::integer (*r)) : std::nullopt);
          });
  core ("Int.render", "Int -> String", 1,
        [] (C &, V &a) { return make_str (std::to_string (a[0].i)); });
  core ("Int.to_float", "Int -> Float", 1,
        [] (C &, V &a) { return Value::floating (static_cast<double> (a[0].i)); });
  core ("Int.abs", "Int -> Int", 1,
        [] (C &, V &a) { return Value::integer (a[0].i < 0 ? detail::wrap_neg (a[0].i) : a[0].i); });
  core ("Int.pow", "Int -> Int -> Int", 2,
        [] (C &c, V &a)
          {
            std::int64_t base = a[0].i, e = a[1].i, r = 1;
            if (e < 0)
              c.error ("M0404", "`Int.pow` expects a non-negative exponent");
            while (e > 0)
              {
                if (e & 1)
                  r = detail::wrap_mul (r, base);
                base = detail::wrap_mul (base, base);
                e >>= 1;
              }
            return Value::integer (r);
          });
  core ("Int.land", "Int -> Int -> Int", 2,
        [] (C &, V &a) { return Value::integer (a[0].i & a[1].i); });
  core ("Int.lor", "Int -> Int -> Int", 2,
        [] (C &, V &a) { return Value::integer (a[0].i | a[1].i); });
  core ("Int.lxor", "Int -> Int -> Int", 2,
        [] (C &, V &a) { return Value::integer (a[0].i ^ a[1].i); });
  core ("Int.shift_left", "Int -> Int -> Int", 2,
        [] (C &, V &a)
          {
            return Value::integer (static_cast<std::int64_t> (
                static_cast<std::uint64_t> (a[0].i) << (a[1].i & 63)));
          });
  core ("Int.shift_right", "Int -> Int -> Int", 2,
        [] (C &, V &a) { return Value::integer (a[0].i >> (a[1].i & 63)); });
  core ("Int.min_value", "Unit -> Int", 1, [] (C &, V &) { return Value::integer (INT64_MIN); });
  core ("Int.max_value", "Unit -> Int", 1, [] (C &, V &) { return Value::integer (INT64_MAX); });
  core ("Int.checked_add", "Int -> Int -> Option Int", 2, [] (C &, V &a) {
    auto x = a[0].i, y = a[1].i;
    if ((y > 0 && x > INT64_MAX - y) || (y < 0 && x < INT64_MIN - y))
      return make_option (std::nullopt);
    return make_option (Value::integer (detail::wrap_add (x, y)));
  });
  core ("Int.checked_sub", "Int -> Int -> Option Int", 2, [] (C &, V &a) {
    auto x = a[0].i, y = a[1].i;
    if ((y < 0 && x > INT64_MAX + y) || (y > 0 && x < INT64_MIN + y))
      return make_option (std::nullopt);
    return make_option (Value::integer (detail::wrap_sub (x, y)));
  });
  core ("Int.checked_mul", "Int -> Int -> Option Int", 2, [] (C &, V &a) {
    auto x = a[0].i, y = a[1].i;
    auto magnitude = [] (std::int64_t n) -> std::uint64_t {
      return n < 0 ? 0ULL - static_cast<std::uint64_t> (n) : static_cast<std::uint64_t> (n);
    };
    auto limit = (x < 0) != (y < 0) ? (1ULL << 63) : static_cast<std::uint64_t> (INT64_MAX);
    if (y != 0 && magnitude (x) > limit / magnitude (y))
      return make_option (std::nullopt);
    return make_option (Value::integer (detail::wrap_mul (x, y)));
  });

  // ---- Float ----
  core ("Float.of_int", "Int -> Float", 1,
        [] (C &, V &a) { return Value::floating (static_cast<double> (a[0].i)); });
  core ("Float.to_string", "Float -> String", 1,
        [F] (C &, V &a) { return make_str (detail::format_float (F (a[0]))); });
  core ("Float.parse", "String -> Option Float", 1,
        [S] (C &, V &a)
          {
            auto r = detail::parse_float (S (a[0]));
            return make_option (r ? std::optional<Value> (Value::floating (*r)) : std::nullopt);
          });
  core ("Float.of_string", "String -> Float", 1,
        [S] (C &c, V &a)
          {
            auto r = detail::parse_float (S (a[0]));
            if (!r)
              c.error ("M0404", "cannot convert " + detail::quote_string (S (a[0]))
                                    + " to a Float; use `Float.parse` for an Option");
            return Value::floating (*r);
          });
  core ("Float.truncate", "Float -> Int", 1,
        [F] (C &, V &a) { return Value::integer (detail::float_to_int (F (a[0]))); });
  struct Unary
  {
    const char *name;
    double (*fn) (double);
  };
  static const Unary unary[] = {
    { "Float.floor", [] (double x) { return std::floor (x); } },
    { "Float.ceil", [] (double x) { return std::ceil (x); } },
    { "Float.round", [] (double x) { return std::round (x); } },
    { "Float.sqrt", [] (double x) { return std::sqrt (x); } },
    { "Float.exp", [] (double x) { return std::exp (x); } },
    { "Float.log", [] (double x) { return std::log (x); } },
    { "Float.sin", [] (double x) { return std::sin (x); } },
    { "Float.cos", [] (double x) { return std::cos (x); } },
    { "Float.tan", [] (double x) { return std::tan (x); } },
    { "Float.abs", [] (double x) { return std::fabs (x); } },
  };
  for (auto &u : unary)
    {
      auto fn = u.fn;
      core (u.name, "Float -> Float", 1,
            [fn, F] (C &, V &a) { return Value::floating (fn (F (a[0]))); });
    }
  core ("Float.pow", "Float -> Float -> Float", 2,
        [F] (C &, V &a) { return Value::floating (std::pow (F (a[0]), F (a[1]))); });
  core ("Float.atan2", "Float -> Float -> Float", 2,
        [F] (C &, V &a) { return Value::floating (std::atan2 (F (a[0]), F (a[1]))); });
  core ("Float.is_nan", "Float -> Bool", 1,
        [F] (C &, V &a) { return Value::boolean (std::isnan (F (a[0]))); });
  core ("Float.is_finite", "Float -> Bool", 1,
        [F] (C &, V &a) { return Value::boolean (std::isfinite (F (a[0]))); });
  core ("Float.is_infinite", "Float -> Bool", 1,
        [F] (C &, V &a) { return Value::boolean (std::isinf (F (a[0]))); });
  core ("Float.infinity", "Unit -> Float", 1,
        [] (C &, V &) { return Value::floating (HUGE_VAL); });
  core ("Float.nan", "Unit -> Float", 1, [] (C &, V &) { return Value::floating (NAN); });

  // ---- String (bytes; UTF-8 helpers live in metalib/cpp/String.h) ----
  core ("String.length", "String -> Int", 1,
        [S] (C &, V &a) { return Value::integer (static_cast<std::int64_t> (S (a[0]).size ())); });
  core ("String.concat", "String -> String -> String", 2,
        [S] (C &, V &a) { return make_str (S (a[0]) + S (a[1])); });
  core ("String.sub", "Int -> Int -> String -> String", 3,
        [S] (C &, V &a)
          {
            const std::string &s = S (a[2]);
            std::int64_t n = static_cast<std::int64_t> (s.size ());
            std::int64_t start = std::clamp<std::int64_t> (a[0].i, 0, n);
            std::int64_t len = std::clamp<std::int64_t> (a[1].i, 0, n - start);
            return make_str (s.substr (static_cast<std::size_t> (start),
                                       static_cast<std::size_t> (len)));
          });
  core ("String.char_at", "Int -> String -> Option String", 2,
        [S] (C &, V &a)
          {
            const std::string &s = S (a[1]);
            if (a[0].i < 0 || a[0].i >= static_cast<std::int64_t> (s.size ()))
              return make_option (std::nullopt);
            return make_option (make_str (std::string (1, s[static_cast<std::size_t> (a[0].i)])));
          });
  core ("String.index_of", "String -> String -> Option Int", 2,
        [S] (C &, V &a)
          {
            auto p = S (a[1]).find (S (a[0]));
            if (p == std::string::npos)
              return make_option (std::nullopt);
            return make_option (Value::integer (static_cast<std::int64_t> (p)));
          });
  core ("String.chars", "String -> [String]", 1,
        [S] (C &, V &a)
          {
            std::vector<Value> out;
            for (char ch : S (a[0]))
              out.push_back (make_str (std::string (1, ch)));
            return make_list (out);
          });
  core ("String.join", "String -> [String] -> String", 2,
        [S] (C &, V &a)
          {
            std::string out;
            bool first = true;
            for (auto &x : list_items (a[1]))
              {
                if (!first)
                  out += S (a[0]);
                first = false;
                out += S (x);
              }
            return make_str (out);
          });
  core ("String.split", "String -> String -> [String]", 2,
        [S] (C &, V &a)
          {
            std::vector<Value> out;
            for (auto &p : detail::split_string (S (a[0]), S (a[1])))
              out.push_back (make_str (p));
            return make_list (out);
          });
  core ("String.trim", "String -> String", 1,
        [S] (C &, V &a) { return make_str (detail::trim_ws (S (a[0]))); });
  core ("String.upper", "String -> String", 1,
        [S] (C &, V &a) { return make_str (detail::ascii_map (S (a[0]), true)); });
  core ("String.lower", "String -> String", 1,
        [S] (C &, V &a) { return make_str (detail::ascii_map (S (a[0]), false)); });
  core ("String.starts_with", "String -> String -> Bool", 2,
        [S] (C &, V &a)
          {
            const auto &p = S (a[0]), &s = S (a[1]);
            return Value::boolean (s.size () >= p.size () && s.compare (0, p.size (), p) == 0);
          });
  core ("String.ends_with", "String -> String -> Bool", 2,
        [S] (C &, V &a)
          {
            const auto &p = S (a[0]), &s = S (a[1]);
            return Value::boolean (s.size () >= p.size ()
                                   && s.compare (s.size () - p.size (), p.size (), p) == 0);
          });
  core ("String.contains", "String -> String -> Bool", 2,
        [S] (C &, V &a) { return Value::boolean (S (a[1]).find (S (a[0])) != std::string::npos); });
  core ("String.replace", "String -> String -> String -> String", 3,
        [S] (C &, V &a) { return make_str (detail::replace_all (S (a[0]), S (a[1]), S (a[2]))); });
  core ("String.repeat", "Int -> String -> String", 2,
        [S] (C &c, V &a)
          {
            std::string out;
            if (a[0].i <= 0 || S (a[1]).empty ())
              return make_str ("");
            if (static_cast<std::uint64_t> (a[0].i) > 100000000ULL / S (a[1]).size ())
              c.error ("M0404", "`String.repeat` result is too large");
            for (std::int64_t k = 0; k < a[0].i; ++k)
              out += S (a[1]);
            return make_str (out);
          });
  core ("String.compare", "String -> String -> Int", 2,
        [] (C &c, V &a) { return Value::integer (compare_values (a[0], a[1], c.span)); });
  core ("String.to_codes", "String -> [Int]", 1,
        [S] (C &, V &a)
          {
            std::vector<Value> out;
            for (unsigned char ch : S (a[0]))
              out.push_back (Value::integer (ch));
            return make_list (out);
          });
  core ("String.of_codes", "[Int] -> String", 1,
        [] (C &, V &a)
          {
            std::string out;
            for (auto &x : list_items (a[0]))
              out += static_cast<char> (x.i & 255);
            return make_str (out);
          });

  // ---- List ----
  core ("List.length", "['a] -> Int", 1,
        [] (C &, V &a)
          {
            std::int64_t n = 0;
            for (const ConsObj *c = cons_of (a[0]); c; c = c->tail.get ())
              ++n;
            return Value::integer (n);
          });
  core ("List.reverse", "['a] -> ['a]", 1,
        [] (C &, V &a)
          {
            Value acc;
            acc.kind = VK::List;
            for (const ConsObj *c = cons_of (a[0]); c; c = c->tail.get ())
              acc = cons_value (c->head, acc);
            return acc;
          });
  core ("List.nth", "Int -> ['a] -> Option 'a", 2,
        [] (C &, V &a)
          {
            std::int64_t k = a[0].i;
            if (k < 0)
              return make_option (std::nullopt);
            for (const ConsObj *c = cons_of (a[1]); c; c = c->tail.get (), --k)
              if (k == 0)
                return make_option (c->head);
            return make_option (std::nullopt);
          });
  core ("List.range", "Int -> Int -> [Int]", 2,
        [] (C &c, V &a)
          {
            std::vector<Value> out;
            if (a[1].i > a[0].i && static_cast<std::uint64_t> (a[1].i)
                  - static_cast<std::uint64_t> (a[0].i) > 50000000ULL)
              c.error ("M0404", "`List.range` is too large");
            for (std::int64_t k = a[0].i; k < a[1].i; ++k)
              out.push_back (Value::integer (k));
            return make_list (out);
          });
  core ("List.sum", "Num 'a => ['a] -> 'a", 1,
        [this] (C &c, V &a)
          {
            Value acc = Value::integer (0);
            for (auto &x : list_items (a[0]))
              acc = interp_.binop ("+", acc, x, c.span);
            return acc;
          });
  core ("List.product", "Num 'a => ['a] -> 'a", 1,
        [this] (C &c, V &a)
          {
            Value acc = Value::integer (1);
            for (auto &x : list_items (a[0]))
              acc = interp_.binop ("*", acc, x, c.span);
            return acc;
          });
  core ("List.sort", "['a] -> ['a]", 1,
        [] (C &c, V &a)
          {
            auto items = list_items (a[0]);
            Span sp = c.span;
            detail::merge_sort (items, [sp] (const Value &x, const Value &y)
                                  { return compare_values (x, y, sp); });
            return make_list (items);
          });
  core ("List.sort_by", "('a -> 'a -> Int) -> ['a] -> ['a]", 2,
        [] (C &c, V &a)
          {
            auto items = list_items (a[1]);
            Value cmp = a[0];
            detail::merge_sort (items, [&c, &cmp] (const Value &x, const Value &y)
                                  { return static_cast<int> (std::clamp<std::int64_t> (
                                        c.apply (cmp, { x, y }).i, -1, 1)); });
            return make_list (items);
          });
  core ("List.concat", "[['a]] -> ['a]", 1,
        [] (C &, V &a)
          {
            std::vector<Value> out;
            for (auto &xs : list_items (a[0]))
              for (auto &x : list_items (xs))
                out.push_back (x);
            return make_list (out);
          });
  core ("List.append", "['a] -> ['a] -> ['a]", 2,
        [this] (C &c, V &a) { return interp_.binop ("@", a[0], a[1], c.span); });

  // ---- IO ----
  core ("IO.print", "String -> Unit", 1,
        [S] (C &c, V &a)
          {
            c.print (S (a[0]));
            return Value::unit ();
          });
  core ("IO.println", "String -> Unit", 1,
        [S] (C &c, V &a)
          {
            c.print (S (a[0]) + "\n");
            return Value::unit ();
          });
  core ("IO.eprint", "String -> Unit", 1,
        [S] (C &, V &a)
          {
            std::cout << std::flush;
            std::cerr << S (a[0]) << std::flush;
            return Value::unit ();
          });
  core ("IO.read_line", "Unit -> Option String", 1,
        [] (C &, V &)
          {
            std::string line;
            if (!std::getline (std::cin, line))
              return make_option (std::nullopt);
            if (!line.empty () && line.back () == '\r')
              line.pop_back ();
            return make_option (make_str (line));
          });
}

// ============================================================
//  REPL Language policy for dsltk::repl::Session (§11)
// ============================================================

struct ReplLanguage
{
  using Program = metaml::Program;
  using Value = std::string;

  Session &session;

  int submission = 0;

  dsl::repl::ParseResult<Program>
  parse (std::string_view source)
  {
    std::string file = "<repl:" + std::to_string (submission + 1) + ">";
    session.remember_source (file, source);
    auto out = metaml::parse (source, file);
    if (out.ok ())
      ++submission;
    if (!out.ok ())
      return out.incomplete ? dsl::repl::ParseResult<Program>::incomplete (out.diagnostics)
                            : dsl::repl::ParseResult<Program>::err (out.diagnostics);
    return dsl::repl::ParseResult<Program>::ok (std::move (out.program));
  }

  dsl::Result<std::string, std::vector<Diagnostic>>
  eval (Program &p)
  {
    using R = dsl::Result<std::string, std::vector<Diagnostic>>;
    auto r = session.run_program (p, RunMode::Repl);
    if (!r.ok ())
      {
        std::vector<Diagnostic> all = r.warnings;
        for (auto &d : r.errors)
          all.push_back (d);
        if (!r.text.empty ())
          session.write (r.text);
        return R::from_err (all);
      }
    return R::from_ok (r.text);
  }

  std::string
  format (const std::string &v)
  {
    return v;
  }

  std::string
  format_error (const Diagnostic &d)
  {
    return session.render (d);
  }
};

/// Registers the MetaML directives (§5.10, §11.2) on a DSLtk REPL session.
inline void
install_repl_directives (dsl::repl::Session<ReplLanguage> &repl, Session &s)
{
  repl.set_unknown_directive_code ("M0601");
  auto run = [&s] (dsl::repl::Session<ReplLanguage> &r, const std::string &name,
                   std::string_view arg)
    {
      auto rep = s.directive (name, std::string (arg));
      if (!rep.text.empty ())
        r.out () << rep.text;
      for (auto &d : rep.errors)
        r.out () << s.render (d);
      if (!rep.ok ())
        r.mark_error ();
    };
  struct D
  {
    const char *name;
    const char *help;
  };
  static const D ds[] = {
    { "type", "print the inferred type of expr" },
    { "load", "load a MetaML source file" },
    { "reload", "reload the last loaded file" },
    { "trace", "evaluate with a reduction trace" },
    { "stack", "print the implicit value stack" },
    { "clear", "clear the value stack" },
    { "ast", "print the AST of source" },
  };
  for (auto &d : ds)
    {
      std::string name = d.name;
      repl.register_directive (name, d.help,
                               [run, name] (dsl::repl::Session<ReplLanguage> &r,
                                            std::string_view arg) { run (r, name, arg); });
    }
  repl.set_completion ([&s] (std::string_view prefix) { return s.completions (prefix); });
}

} // namespace metaml
