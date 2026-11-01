/**
 * @file mulib-microjs.h
 * @brief A small checked JS-like expression/variable interpreter, not
 * ECMAScript. Define MULIB_IMPL_MICROJS once. No module dependencies; link
 * libm on Unix.
 */
#ifndef MULIB_MICROJS_H
#define MULIB_MICROJS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_microjs mulib_microjs_t;
  typedef enum mulib_microjs_type
  {
    MULIB_MICROJS_NULL,
    MULIB_MICROJS_NUMBER,
    MULIB_MICROJS_BOOLEAN,
    MULIB_MICROJS_STRING
  } mulib_microjs_type_t;
  /** @brief Owned output string (only for STRING); initialize outputs to zero,
   * then free. */
  typedef struct mulib_microjs_value
  {
    mulib_microjs_type_t type;
    double number;
    int boolean;
    char *string;
    size_t length;
  } mulib_microjs_value_t;
  typedef enum mulib_microjs_status
  {
    MULIB_MICROJS_OK,
    MULIB_MICROJS_INVALID,
    MULIB_MICROJS_SYNTAX,
    MULIB_MICROJS_TYPE,
    MULIB_MICROJS_NAME,
    MULIB_MICROJS_ARITHMETIC,
    MULIB_MICROJS_LIMIT,
    MULIB_MICROJS_MEMORY
  } mulib_microjs_status_t;
  /** @brief Static message, byte offset and 1-based line/column. */
  typedef struct mulib_microjs_error
  {
    mulib_microjs_status_t code;
    size_t offset, line, column;
    const char *message;
  } mulib_microjs_error_t;
  /** @brief Hard limits; zero is zero. depth must be <=128; string_bytes is
   * per string. */
  typedef struct mulib_microjs_limits
  {
    size_t source_bytes, steps, depth, variables, string_bytes;
  } mulib_microjs_limits_t;
  /** @brief Defaults: 1 MiB source, 100000 steps, depth 64, 256 variables, 64
   * KiB/string. */
  mulib_microjs_limits_t mulib_microjs_default_limits (void);
  /** @brief Allocate a persistent variable environment; NULL limits selects
   * defaults. */
  mulib_microjs_t *mulib_microjs_create (const mulib_microjs_limits_t *limits);
  /** @brief Release the environment. NULL is harmless. */
  void mulib_microjs_destroy (mulib_microjs_t *interpreter);
  /** @brief Release and clear an output value. NULL is harmless. */
  void mulib_microjs_value_free (mulib_microjs_value_t *value);
  /** @brief Execute let declarations, assignments and expressions; last value
   * or null. Completed statements persist on failure; a failing statement does
   * not assign. out is written only on success and must be empty. Source may
   * be NULL at length 0.
   */
  mulib_microjs_status_t mulib_microjs_eval (mulib_microjs_t *interpreter,
                                             const char *source, size_t length,
                                             mulib_microjs_value_t *out,
                                             mulib_microjs_error_t *error);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_MICROJS) && !defined(MULIB_MICROJS_IMPLEMENTED)
#define MULIB_MICROJS_IMPLEMENTED
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_microjs_var_
  {
    char *name;
    size_t length;
    mulib_microjs_value_t value;
  } mulib_microjs_var_;
  struct mulib_microjs
  {
    mulib_microjs_limits_t limits;
    mulib_microjs_var_ *vars;
    size_t count;
  };
  enum
  {
    MULIB_MICROJS_ID_ = 256,
    MULIB_MICROJS_NUM_,
    MULIB_MICROJS_STR_,
    MULIB_MICROJS_EQ_,
    MULIB_MICROJS_NE_,
    MULIB_MICROJS_LE_,
    MULIB_MICROJS_GE_,
    MULIB_MICROJS_AND_,
    MULIB_MICROJS_OR_
  };
  typedef struct mulib_microjs_parser_
  {
    mulib_microjs_t *vm;
    const char *source;
    size_t length, pos, start, end, steps, depth;
    int token;
    mulib_microjs_error_t error;
  } mulib_microjs_parser_;
  mulib_microjs_limits_t
  mulib_microjs_default_limits (void)
  {
    mulib_microjs_limits_t l = { 1048576, 100000, 64, 256, 65536 };
    return l;
  }
  mulib_microjs_t *
  mulib_microjs_create (const mulib_microjs_limits_t *requested)
  {
    mulib_microjs_limits_t l
        = requested ? *requested : mulib_microjs_default_limits ();
    mulib_microjs_t *v;
    if (l.depth > 128 || l.variables > SIZE_MAX / sizeof (mulib_microjs_var_))
      return NULL;
    v = (mulib_microjs_t *)calloc (1, sizeof *v);
    if (!v)
      return NULL;
    v->limits = l;
    if (l.variables)
      v->vars = (mulib_microjs_var_ *)calloc (l.variables, sizeof *v->vars);
    if (l.variables && !v->vars)
      {
        free (v);
        return NULL;
      }
    return v;
  }
  void
  mulib_microjs_value_free (mulib_microjs_value_t *v)
  {
    if (v)
      {
        free (v->string);
        memset (v, 0, sizeof *v);
      }
  }
  void
  mulib_microjs_destroy (mulib_microjs_t *v)
  {
    size_t i;
    if (!v)
      return;
    for (i = 0; i < v->count; ++i)
      {
        free (v->vars[i].name);
        mulib_microjs_value_free (&v->vars[i].value);
      }
    free (v->vars);
    free (v);
  }
  static int
  mulib_microjs_fail_ (mulib_microjs_parser_ *p, mulib_microjs_status_t code,
                       const char *message)
  {
    if (!p->error.code)
      {
        p->error.code = code;
        p->error.offset = p->start;
        p->error.message = message;
      }
    return 0;
  }
  static int
  mulib_microjs_step_ (mulib_microjs_parser_ *p)
  {
    if (p->steps == p->vm->limits.steps)
      return mulib_microjs_fail_ (p, MULIB_MICROJS_LIMIT,
                                  "evaluation step limit");
    ++p->steps;
    return 1;
  }
  static int
  mulib_microjs_alpha_ (unsigned char c)
  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
           || c == '$';
  }
  static int
  mulib_microjs_digit_ (unsigned char c)
  {
    return c >= '0' && c <= '9';
  }
  static int
  mulib_microjs_next_ (mulib_microjs_parser_ *p)
  {
    const char *s = p->source;
    unsigned char c;
    if (p->error.code)
      return 0;
    if (!mulib_microjs_step_ (p))
      return 0;
    for (;;)
      {
        while (p->pos < p->length
               && (s[p->pos] == ' ' || s[p->pos] == '\t' || s[p->pos] == '\r'
                   || s[p->pos] == '\n'))
          ++p->pos;
        if (p->length - p->pos >= 2 && s[p->pos] == '/'
            && s[p->pos + 1] == '/')
          {
            while (p->pos < p->length && s[p->pos] != '\n')
              ++p->pos;
          }
        else
          break;
      }
    p->start = p->pos;
    p->end = p->pos;
    if (p->pos == p->length)
      {
        p->token = 0;
        return 1;
      }
    c = (unsigned char)s[p->pos++];
    p->token = c;
    if (mulib_microjs_alpha_ (c))
      {
        while (p->pos < p->length
               && (mulib_microjs_alpha_ ((unsigned char)s[p->pos])
                   || mulib_microjs_digit_ ((unsigned char)s[p->pos])))
          ++p->pos;
        p->token = MULIB_MICROJS_ID_;
      }
    else if (mulib_microjs_digit_ (c))
      {
        while (p->pos < p->length
               && mulib_microjs_digit_ ((unsigned char)s[p->pos]))
          ++p->pos;
        if (p->pos < p->length && s[p->pos] == '.')
          {
            ++p->pos;
            if (p->pos == p->length
                || !mulib_microjs_digit_ ((unsigned char)s[p->pos]))
              return mulib_microjs_fail_ (p, MULIB_MICROJS_SYNTAX,
                                          "expected fractional digits");
            while (p->pos < p->length
                   && mulib_microjs_digit_ ((unsigned char)s[p->pos]))
              ++p->pos;
          }
        if (p->pos < p->length && (s[p->pos] == 'e' || s[p->pos] == 'E'))
          {
            ++p->pos;
            if (p->pos < p->length && (s[p->pos] == '+' || s[p->pos] == '-'))
              ++p->pos;
            if (p->pos == p->length
                || !mulib_microjs_digit_ ((unsigned char)s[p->pos]))
              return mulib_microjs_fail_ (p, MULIB_MICROJS_SYNTAX,
                                          "expected exponent digits");
            while (p->pos < p->length
                   && mulib_microjs_digit_ ((unsigned char)s[p->pos]))
              ++p->pos;
          }
        p->token = MULIB_MICROJS_NUM_;
      }
    else if (c == '\'' || c == '"')
      {
        int closed = 0;
        while (p->pos < p->length)
          {
            unsigned char ch = (unsigned char)s[p->pos++];
            if (ch == c)
              {
                closed = 1;
                break;
              }
            if (ch < 32 || ch >= 127)
              return mulib_microjs_fail_ (
                  p, MULIB_MICROJS_SYNTAX,
                  "strings require printable ASCII or escapes");
            if (ch == '\\')
              {
                if (p->pos == p->length || !strchr ("\\\"'nrt", s[p->pos])
                    || !s[p->pos])
                  return mulib_microjs_fail_ (p, MULIB_MICROJS_SYNTAX,
                                              "unsupported string escape");
                ++p->pos;
              }
          }
        if (!closed)
          return mulib_microjs_fail_ (p, MULIB_MICROJS_SYNTAX,
                                      "unterminated string");
        p->token = MULIB_MICROJS_STR_;
      }
    else if ((c == '=' || c == '!') && p->length - p->pos >= 2
             && s[p->pos] == '=' && s[p->pos + 1] == '=')
      {
        p->pos += 2;
        p->token = c == '=' ? MULIB_MICROJS_EQ_ : MULIB_MICROJS_NE_;
      }
    else if ((c == '<' || c == '>') && p->pos < p->length && s[p->pos] == '=')
      {
        ++p->pos;
        p->token = c == '<' ? MULIB_MICROJS_LE_ : MULIB_MICROJS_GE_;
      }
    else if ((c == '&' || c == '|') && p->pos < p->length
             && s[p->pos] == (char)c)
      {
        ++p->pos;
        p->token = c == '&' ? MULIB_MICROJS_AND_ : MULIB_MICROJS_OR_;
      }
    else if (!c || !strchr ("()+-*/%!=<>;", c))
      {
        return mulib_microjs_fail_ (p, MULIB_MICROJS_SYNTAX,
                                    "unsupported token");
      }
    p->end = p->pos;
    return 1;
  }
  static int
  mulib_microjs_word_ (const mulib_microjs_parser_ *p, const char *word)
  {
    return p->token == MULIB_MICROJS_ID_ && p->end - p->start == strlen (word)
           && !memcmp (p->source + p->start, word, strlen (word));
  }
  static int
  mulib_microjs_reserved_ (const mulib_microjs_parser_ *p)
  {
    static const char *const words[]
        = { "let",      "true",   "false", "null",  "const",
            "var",      "if",     "else",  "while", "for",
            "function", "return", "new",   "this",  "undefined" };
    size_t i;
    for (i = 0; i < sizeof words / sizeof *words; ++i)
      if (mulib_microjs_word_ (p, words[i]))
        return 1;
    return 0;
  }
  static size_t
  mulib_microjs_find_ (const mulib_microjs_t *v, const char *s, size_t n)
  {
    size_t i;
    for (i = 0; i < v->count; ++i)
      if (v->vars[i].length == n && !memcmp (v->vars[i].name, s, n))
        return i;
    return SIZE_MAX;
  }
  static int
  mulib_microjs_copy_ (mulib_microjs_parser_ *p,
                       const mulib_microjs_value_t *src,
                       mulib_microjs_value_t *out)
  {
    *out = *src;
    out->string = NULL;
    if (src->type == MULIB_MICROJS_STRING)
      {
        out->string = (char *)malloc (src->length + 1);
        if (!out->string)
          return mulib_microjs_fail_ (p, MULIB_MICROJS_MEMORY,
                                      "string allocation failed");
        memcpy (out->string, src->string, src->length + 1);
      }
    return 1;
  }
  static int
  mulib_microjs_precedence_ (int token)
  {
    switch (token)
      {
      case MULIB_MICROJS_OR_:
        return 1;
      case MULIB_MICROJS_AND_:
        return 2;
      case MULIB_MICROJS_EQ_:
      case MULIB_MICROJS_NE_:
        return 3;
      case '<':
      case '>':
      case MULIB_MICROJS_LE_:
      case MULIB_MICROJS_GE_:
        return 4;
      case '+':
      case '-':
        return 5;
      case '*':
      case '/':
      case '%':
        return 6;
      default:
        return 0;
      }
  }
  static int mulib_microjs_expr_ (mulib_microjs_parser_ *p, int minimum,
                                  int execute, mulib_microjs_value_t *out);
  /* Normalize before scaling: neither a long integer nor a tiny fraction
   * should lose its value before a compensating exponent is applied. The lexer
   * has already checked the grammar. Keep bounded precision and signed
   * magnitudes so even arbitrarily long exponent fields cannot overflow an
   * integer counter. */
  static int
  mulib_microjs_number_ (mulib_microjs_parser_ *p, double *out)
  {
    size_t i = p->start, digits = 0, integers = 0, first = SIZE_MAX, kept = 0;
    size_t exponent = 0, magnitude, cap = p->end - p->start;
    uint64_t mantissa = 0;
    int fraction = 0, negative = 0, base_negative;
    long double d, scale;
    size_t margin = (size_t)DBL_MAX_10_EXP + (size_t)DBL_MANT_DIG + 32;
    cap = cap > SIZE_MAX - margin ? SIZE_MAX : cap + margin;
    while (i < p->end && p->source[i] != 'e' && p->source[i] != 'E')
      {
        unsigned digit;
        if (p->source[i] == '.')
          {
            fraction = 1;
            ++i;
            continue;
          }
        digit = (unsigned)(p->source[i++] - '0');
        if (!fraction)
          ++integers;
        if (first == SIZE_MAX && digit)
          first = digits;
        if (first != SIZE_MAX && kept < 19)
          {
            mantissa = mantissa * 10 + digit;
            ++kept;
          }
        ++digits;
      }
    if (i < p->end)
      {
        ++i;
        if (p->source[i] == '-' || p->source[i] == '+')
          {
            negative = p->source[i] == '-';
            ++i;
          }
        while (i < p->end)
          {
            unsigned digit = (unsigned)(p->source[i++] - '0');
            exponent
                = exponent > (cap - digit) / 10 ? cap : exponent * 10 + digit;
          }
      }
    if (first == SIZE_MAX)
      {
        *out = 0;
        return 1;
      }
    base_negative = integers <= first;
    magnitude = base_negative ? first - integers + 1 : integers - first - 1;
    if (base_negative == negative)
      {
        if (exponent > SIZE_MAX - magnitude)
          {
            if (!negative)
              return mulib_microjs_fail_ (p, MULIB_MICROJS_ARITHMETIC,
                                          "numeric literal overflow");
            *out = 0;
            return 1;
          }
        magnitude += exponent;
      }
    else if (magnitude >= exponent)
      {
        magnitude -= exponent;
        negative = base_negative;
      }
    else
      magnitude = exponent - magnitude;
    if (!negative && magnitude > (size_t)DBL_MAX_10_EXP)
      return mulib_microjs_fail_ (p, MULIB_MICROJS_ARITHMETIC,
                                  "numeric literal overflow");
    if (negative
        && magnitude > (size_t)(-DBL_MIN_10_EXP) + (size_t)DBL_MANT_DIG + 8)
      {
        *out = 0;
        return 1;
      }
    d = (long double)mantissa / powl (10.0L, (long double)(kept - 1));
    /* Two half-scales avoid making an unrepresentably small power of ten
     * before multiplying a nonzero significand, including when long double
     * has the same exponent range as double. */
    if (negative)
      {
        scale = powl (10.0L, -(long double)magnitude / 2.0L);
        d *= scale;
        d *= scale;
      }
    else
      d *= powl (10.0L, (long double)magnitude);
    if (!isfinite (d) || d > (long double)DBL_MAX)
      return mulib_microjs_fail_ (p, MULIB_MICROJS_ARITHMETIC,
                                  "numeric literal overflow");
    *out = (double)d;
    if (!isfinite (*out))
      return mulib_microjs_fail_ (p, MULIB_MICROJS_ARITHMETIC,
                                  "numeric literal overflow");
    return 1;
  }
  static int
  mulib_microjs_primary_ (mulib_microjs_parser_ *p, int execute,
                          mulib_microjs_value_t *out)
  {
    int token = p->token;
    if (token == '!' || token == '-' || token == '+')
      {
        if (!mulib_microjs_next_ (p)
            || !mulib_microjs_expr_ (p, 7, execute, out))
          return 0;
        if (!execute)
          return 1;
        if (token == '!')
          {
            if (out->type != MULIB_MICROJS_BOOLEAN)
              return mulib_microjs_fail_ (p, MULIB_MICROJS_TYPE,
                                          "! requires boolean");
            out->boolean = !out->boolean;
          }
        else
          {
            if (out->type != MULIB_MICROJS_NUMBER)
              return mulib_microjs_fail_ (p, MULIB_MICROJS_TYPE,
                                          "unary sign requires number");
            if (token == '-')
              out->number = -out->number;
          }
        return 1;
      }
    if (token == '(')
      {
        if (!mulib_microjs_next_ (p)
            || !mulib_microjs_expr_ (p, 1, execute, out))
          return 0;
        if (p->token != ')')
          return mulib_microjs_fail_ (p, MULIB_MICROJS_SYNTAX,
                                      "expected closing parenthesis");
        return mulib_microjs_next_ (p);
      }
    if (token == MULIB_MICROJS_NUM_)
      {
        if (execute)
          {
            if (!mulib_microjs_number_ (p, &out->number))
              return 0;
            out->type = MULIB_MICROJS_NUMBER;
          }
      }
    else if (token == MULIB_MICROJS_STR_)
      {
        if (execute)
          {
            size_t i, n = 0;
            for (i = p->start + 1; i + 1 < p->end; ++i)
              {
                if (p->source[i] == '\\')
                  ++i;
                ++n;
              }
            if (n > p->vm->limits.string_bytes || n == SIZE_MAX)
              return mulib_microjs_fail_ (p, MULIB_MICROJS_LIMIT,
                                          "string byte limit");
            out->string = (char *)malloc (n + 1);
            if (!out->string)
              return mulib_microjs_fail_ (p, MULIB_MICROJS_MEMORY,
                                          "string allocation failed");
            out->type = MULIB_MICROJS_STRING;
            out->length = n;
            n = 0;
            for (i = p->start + 1; i + 1 < p->end; ++i)
              {
                char c = p->source[i];
                if (c == '\\')
                  {
                    c = p->source[++i];
                    if (c == 'n')
                      c = '\n';
                    else if (c == 'r')
                      c = '\r';
                    else if (c == 't')
                      c = '\t';
                  }
                out->string[n++] = c;
              }
            out->string[n] = 0;
          }
      }
    else if (token == MULIB_MICROJS_ID_)
      {
        if (mulib_microjs_word_ (p, "true")
            || mulib_microjs_word_ (p, "false"))
          {
            if (execute)
              {
                out->type = MULIB_MICROJS_BOOLEAN;
                out->boolean = mulib_microjs_word_ (p, "true");
              }
          }
        else if (mulib_microjs_word_ (p, "null"))
          { /* zero value is null */
          }
        else if (mulib_microjs_reserved_ (p))
          return mulib_microjs_fail_ (p, MULIB_MICROJS_SYNTAX,
                                      "unsupported keyword");
        else if (execute)
          {
            size_t i = mulib_microjs_find_ (p->vm, p->source + p->start,
                                            p->end - p->start);
            if (i == SIZE_MAX)
              return mulib_microjs_fail_ (p, MULIB_MICROJS_NAME,
                                          "unknown variable");
            if (!mulib_microjs_copy_ (p, &p->vm->vars[i].value, out))
              return 0;
          }
      }
    else
      return mulib_microjs_fail_ (p, MULIB_MICROJS_SYNTAX,
                                  "expected expression");
    return mulib_microjs_next_ (p);
  }
  static int
  mulib_microjs_binary_ (mulib_microjs_parser_ *p, int op,
                         mulib_microjs_value_t *a,
                         const mulib_microjs_value_t *b)
  {
    int comparison = op == '<' || op == '>' || op == MULIB_MICROJS_LE_
                     || op == MULIB_MICROJS_GE_;
    if (op == MULIB_MICROJS_EQ_ || op == MULIB_MICROJS_NE_)
      {
        int eq = a->type == b->type;
        if (eq)
          {
            if (a->type == MULIB_MICROJS_NUMBER)
              eq = a->number == b->number;
            else if (a->type == MULIB_MICROJS_BOOLEAN)
              eq = a->boolean == b->boolean;
            else if (a->type == MULIB_MICROJS_STRING)
              eq = a->length == b->length
                   && !memcmp (a->string, b->string, a->length);
          }
        mulib_microjs_value_free (a);
        a->type = MULIB_MICROJS_BOOLEAN;
        a->boolean = op == MULIB_MICROJS_EQ_ ? eq : !eq;
        return 1;
      }
    if (op == MULIB_MICROJS_AND_ || op == MULIB_MICROJS_OR_)
      {
        if (a->type != MULIB_MICROJS_BOOLEAN
            || b->type != MULIB_MICROJS_BOOLEAN)
          return mulib_microjs_fail_ (p, MULIB_MICROJS_TYPE,
                                      "logical operators require booleans");
        a->boolean = b->boolean;
        return 1;
      }
    if (op == '+' && a->type == MULIB_MICROJS_STRING
        && b->type == MULIB_MICROJS_STRING)
      {
        char *s;
        size_t n;
        if (b->length > SIZE_MAX - a->length - 1
            || b->length > p->vm->limits.string_bytes - a->length)
          return mulib_microjs_fail_ (p, MULIB_MICROJS_LIMIT,
                                      "concatenated string byte limit");
        n = a->length + b->length;
        s = (char *)realloc (a->string, n + 1);
        if (!s)
          return mulib_microjs_fail_ (p, MULIB_MICROJS_MEMORY,
                                      "string allocation failed");
        memcpy (s + a->length, b->string, b->length + 1);
        a->string = s;
        a->length = n;
        return 1;
      }
    if (a->type != MULIB_MICROJS_NUMBER || b->type != MULIB_MICROJS_NUMBER)
      return mulib_microjs_fail_ (
          p, MULIB_MICROJS_TYPE,
          "operator requires two numbers (or two strings for +)");
    if (comparison)
      {
        int r = op == '<'                 ? a->number < b->number
                : op == '>'               ? a->number > b->number
                : op == MULIB_MICROJS_LE_ ? a->number <= b->number
                                          : a->number >= b->number;
        a->type = MULIB_MICROJS_BOOLEAN;
        a->boolean = r;
        return 1;
      }
    if ((op == '/' || op == '%') && b->number == 0)
      return mulib_microjs_fail_ (p, MULIB_MICROJS_ARITHMETIC,
                                  "division by zero");
    switch (op)
      {
      case '+':
        a->number += b->number;
        break;
      case '-':
        a->number -= b->number;
        break;
      case '*':
        a->number *= b->number;
        break;
      case '/':
        a->number /= b->number;
        break;
      case '%':
        a->number = fmod (a->number, b->number);
        break;
      default:
        return mulib_microjs_fail_ (p, MULIB_MICROJS_SYNTAX,
                                    "unsupported operator");
      }
    if (!isfinite (a->number))
      return mulib_microjs_fail_ (p, MULIB_MICROJS_ARITHMETIC,
                                  "nonfinite arithmetic result");
    return 1;
  }
  static int
  mulib_microjs_expr_ (mulib_microjs_parser_ *p, int minimum, int execute,
                       mulib_microjs_value_t *out)
  {
    mulib_microjs_value_t a = { 0 }, b = { 0 };
    int ok = 0;
    if (p->depth == p->vm->limits.depth)
      return mulib_microjs_fail_ (p, MULIB_MICROJS_LIMIT,
                                  "expression depth limit");
    ++p->depth;
    if (!mulib_microjs_step_ (p) || !mulib_microjs_primary_ (p, execute, &a))
      goto done;
    while (mulib_microjs_precedence_ (p->token) >= minimum)
      {
        int op = p->token, precedence = mulib_microjs_precedence_ (op),
            skip = 0;
        if (execute && (op == MULIB_MICROJS_AND_ || op == MULIB_MICROJS_OR_))
          {
            if (a.type != MULIB_MICROJS_BOOLEAN)
              {
                mulib_microjs_fail_ (p, MULIB_MICROJS_TYPE,
                                     "logical operator requires boolean");
                goto done;
              }
            skip = op == MULIB_MICROJS_AND_ ? !a.boolean : a.boolean;
          }
        if (!mulib_microjs_next_ (p)
            || !mulib_microjs_expr_ (p, precedence + 1, execute && !skip, &b))
          goto done;
        if (execute && !skip && !mulib_microjs_binary_ (p, op, &a, &b))
          goto done;
        mulib_microjs_value_free (&b);
      }
    *out = a;
    memset (&a, 0, sizeof a);
    ok = 1;
  done:
    mulib_microjs_value_free (&a);
    mulib_microjs_value_free (&b);
    --p->depth;
    return ok;
  }
  mulib_microjs_status_t
  mulib_microjs_eval (mulib_microjs_t *v, const char *s, size_t n,
                      mulib_microjs_value_t *out, mulib_microjs_error_t *error)
  {
    mulib_microjs_parser_ p;
    mulib_microjs_value_t last = { 0 }, value = { 0 };
    size_t i;
    memset (&p, 0, sizeof p);
    p.vm = v;
    p.source = s;
    p.length = n;
    if (!v || !out || out->string || out->type != MULIB_MICROJS_NULL
        || (!s && n))
      {
        mulib_microjs_fail_ (&p, MULIB_MICROJS_INVALID, "invalid arguments");
        goto done;
      }
    if (n > v->limits.source_bytes)
      {
        mulib_microjs_fail_ (&p, MULIB_MICROJS_LIMIT, "source byte limit");
        goto done;
      }
    if (!mulib_microjs_next_ (&p))
      goto done;
    while (p.token)
      {
        int declaration = mulib_microjs_word_ (&p, "let"), assignment = 0;
        size_t name_start = 0, name_length = 0, index = SIZE_MAX;
        if (p.token == ';')
          {
            if (!mulib_microjs_next_ (&p))
              goto done;
            continue;
          }
        if (declaration && !mulib_microjs_next_ (&p))
          goto done;
        if (p.token == MULIB_MICROJS_ID_ && !mulib_microjs_reserved_ (&p))
          {
            mulib_microjs_parser_ look = p;
            if (!mulib_microjs_next_ (&look))
              {
                p = look;
                goto done;
              }
            if (look.token == '=')
              {
                name_start = p.start;
                name_length = p.end - p.start;
                assignment = 1;
                index = mulib_microjs_find_ (v, s + name_start, name_length);
                p = look;
                if ((declaration && index != SIZE_MAX)
                    || (!declaration && index == SIZE_MAX))
                  {
                    mulib_microjs_fail_ (
                        &p, MULIB_MICROJS_NAME,
                        declaration ? "duplicate declaration"
                                    : "assignment to unknown variable");
                    goto done;
                  }
                if (!mulib_microjs_next_ (&p))
                  goto done;
              }
          }
        if (declaration && !assignment)
          {
            mulib_microjs_fail_ (&p, MULIB_MICROJS_SYNTAX,
                                 "let requires name = expression");
            goto done;
          }
        if (!mulib_microjs_expr_ (&p, 1, 1, &value))
          goto done;
        if (p.token && p.token != ';')
          {
            mulib_microjs_fail_ (&p, MULIB_MICROJS_SYNTAX,
                                 "expected semicolon or end");
            goto done;
          }
        if (assignment)
          {
            mulib_microjs_value_t copy = { 0 };
            char *name = NULL;
            if (declaration)
              {
                if (v->count == v->limits.variables
                    || name_length > v->limits.string_bytes
                    || name_length == SIZE_MAX)
                  {
                    mulib_microjs_fail_ (&p, MULIB_MICROJS_LIMIT,
                                         "variable/name limit");
                    goto done;
                  }
                name = (char *)malloc (name_length + 1);
                if (!name)
                  {
                    mulib_microjs_fail_ (&p, MULIB_MICROJS_MEMORY,
                                         "variable allocation failed");
                    goto done;
                  }
                memcpy (name, s + name_start, name_length);
                name[name_length] = 0;
              }
            if (!mulib_microjs_copy_ (&p, &value, &copy))
              {
                free (name);
                goto done;
              }
            if (declaration)
              {
                index = v->count++;
                v->vars[index].name = name;
                v->vars[index].length = name_length;
              }
            mulib_microjs_value_free (&v->vars[index].value);
            v->vars[index].value = copy;
          }
        mulib_microjs_value_free (&last);
        last = value;
        memset (&value, 0, sizeof value);
        if (p.token && !mulib_microjs_next_ (&p))
          goto done;
      }
    *out = last;
    memset (&last, 0, sizeof last);
  done:
    mulib_microjs_value_free (&last);
    mulib_microjs_value_free (&value);
    if (p.error.code)
      {
        p.error.line = p.error.column = 1;
        for (i = 0; i < p.error.offset; ++i)
          {
            if (s[i] == '\n')
              {
                ++p.error.line;
                p.error.column = 1;
              }
            else
              ++p.error.column;
          }
      }
    if (error)
      *error = p.error;
    return p.error.code;
  }
#ifdef __cplusplus
}
#endif
#endif
