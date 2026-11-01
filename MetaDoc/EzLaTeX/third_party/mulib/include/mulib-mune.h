/** @file mulib-mune.h
 * @brief Bounded numeric Lua subset: assignments, arithmetic and return.
 * Define MULIB_IMPL_MUNE once. See manual for deliberate grammar limits.
 */
#ifndef MULIB_MUNE_H
#define MULIB_MUNE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Return API revision 1. */
  unsigned mulib_mune_version (void);
  typedef struct mulib_mune mulib_mune_t;
  typedef enum mulib_mune_status
  {
    MULIB_MUNE_OK,
    MULIB_MUNE_INVALID,
    MULIB_MUNE_SYNTAX,
    MULIB_MUNE_NAME,
    MULIB_MUNE_ARITHMETIC,
    MULIB_MUNE_LIMIT
  } mulib_mune_status_t;
  /** @brief Static diagnostic and zero-based source byte offset. */
  typedef struct mulib_mune_error
  {
    mulib_mune_status_t code;
    size_t offset;
    const char *message;
  } mulib_mune_error_t;
  /** @brief Allocate an empty persistent numeric environment; NULL on OOM. */
  mulib_mune_t *mulib_mune_create (void);
  /** @brief Release environment; NULL safe. Objects require external
   * synchronization. */
  void mulib_mune_destroy (mulib_mune_t *interpreter);
  /** @brief Evaluate at most 64 KiB with bounded work/depth/storage. All
   * assignments commit together on success; output unchanged on error. Empty
   * script returns 0. Bytes are borrowed for the call only; NULL is allowed at
   * length zero.
   */
  mulib_mune_status_t mulib_mune_eval (mulib_mune_t *interpreter,
                                       const char *source, size_t length,
                                       double *out, mulib_mune_error_t *error);
  /** @brief Copy a variable value; return 0 for missing/invalid, output
   * unchanged. */
  int mulib_mune_get (const mulib_mune_t *interpreter, const char *name,
                      double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MUNE) && !defined(MULIB_MUNE_IMPLEMENTED)
#define MULIB_MUNE_IMPLEMENTED
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  unsigned
  mulib_mune_version (void)
  {
    return 1;
  }
  struct mulib_mune
  {
    size_t count;
    struct
    {
      char name[64];
      double value;
    } vars[128];
  };
  typedef struct mulib_mune_parser_
  {
    const char *source;
    size_t length, at, token_at, steps;
    unsigned token, depth;
    char word[128];
    double number;
    mulib_mune_t *env;
    mulib_mune_error_t error;
  } mulib_mune_parser_;
  static int
  mulib_mune_fail_ (mulib_mune_parser_ *p, mulib_mune_status_t code,
                    const char *message)
  {
    if (!p->error.code)
      {
        p->error.code = code;
        p->error.offset = p->token_at;
        p->error.message = message;
      }
    return 0;
  }
  static int
  mulib_mune_alpha_ (unsigned char c)
  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
  }
  static int
  mulib_mune_digit_ (unsigned char c)
  {
    return c >= '0' && c <= '9';
  }
  static int
  mulib_mune_next_ (mulib_mune_parser_ *p)
  {
    size_t start, n;
    unsigned char c;
    if (++p->steps > 10000)
      return mulib_mune_fail_ (p, MULIB_MUNE_LIMIT, "token limit");
    for (;;)
      {
        while (p->at < p->length
               && (p->source[p->at] == ' ' || p->source[p->at] == '\t'
                   || p->source[p->at] == '\r' || p->source[p->at] == '\n'))
          ++p->at;
        if (p->length - p->at >= 2 && p->source[p->at] == '-'
            && p->source[p->at + 1] == '-')
          {
            p->at += 2;
            if (p->length - p->at >= 2 && p->source[p->at] == '['
                && p->source[p->at + 1] == '[')
              return mulib_mune_fail_ (p, MULIB_MUNE_SYNTAX,
                                       "long comments unsupported");
            while (p->at < p->length && p->source[p->at] != '\n')
              ++p->at;
          }
        else
          break;
      }
    p->token_at = p->at;
    if (p->at == p->length)
      {
        p->token = 0;
        return 1;
      }
    c = (unsigned char)p->source[p->at++];
    start = p->token_at;
    if (mulib_mune_alpha_ (c))
      {
        while (p->at < p->length
               && (mulib_mune_alpha_ ((unsigned char)p->source[p->at])
                   || mulib_mune_digit_ ((unsigned char)p->source[p->at])))
          ++p->at;
        n = p->at - start;
        if (n > 63)
          return mulib_mune_fail_ (p, MULIB_MUNE_LIMIT, "identifier limit");
        memcpy (p->word, p->source + start, n);
        p->word[n] = 0;
        p->token = !strcmp (p->word, "return") ? 258 : 257;
        if (p->token == 257)
          {
            static const char *const reserved[]
                = { "and",   "break", "do",       "else", "elseif", "end",
                    "false", "for",   "function", "goto", "if",     "in",
                    "local", "nil",   "not",      "or",   "repeat", "then",
                    "true",  "until", "while" };
            for (size_t i = 0; i < sizeof (reserved) / sizeof (reserved[0]);
                 ++i)
              if (!strcmp (p->word, reserved[i]))
                return mulib_mune_fail_ (p, MULIB_MUNE_SYNTAX,
                                         "unsupported Lua keyword");
          }
        return 1;
      }
    if (mulib_mune_digit_ (c)
        || (c == '.' && p->at < p->length
            && mulib_mune_digit_ ((unsigned char)p->source[p->at])))
      {
        char *end;
        int saved_errno = errno;
        while (p->at < p->length
               && mulib_mune_digit_ ((unsigned char)p->source[p->at]))
          ++p->at;
        if (c != '.' && p->at < p->length && p->source[p->at] == '.')
          {
            ++p->at;
            while (p->at < p->length
                   && mulib_mune_digit_ ((unsigned char)p->source[p->at]))
              ++p->at;
          }
        if (p->at < p->length
            && (p->source[p->at] == 'e' || p->source[p->at] == 'E'))
          {
            ++p->at;
            if (p->at < p->length
                && (p->source[p->at] == '+' || p->source[p->at] == '-'))
              ++p->at;
            while (p->at < p->length
                   && mulib_mune_digit_ ((unsigned char)p->source[p->at]))
              ++p->at;
          }
        if (p->at < p->length
            && (mulib_mune_alpha_ ((unsigned char)p->source[p->at])
                || p->source[p->at] == '.'))
          return mulib_mune_fail_ (p, MULIB_MUNE_SYNTAX,
                                   "invalid numeric token boundary");
        n = p->at - start;
        if (n >= sizeof (p->word))
          return mulib_mune_fail_ (p, MULIB_MUNE_LIMIT,
                                   "numeric literal limit");
        memcpy (p->word, p->source + start, n);
        p->word[n] = 0;
        errno = 0;
        p->number = strtod (p->word, &end);
        {
          int bad = errno || !isfinite (p->number);
          errno = saved_errno;
          if (bad)
            return mulib_mune_fail_ (p, MULIB_MUNE_ARITHMETIC,
                                     "numeric literal out of range");
        }
        if (*end)
          return mulib_mune_fail_ (p, MULIB_MUNE_SYNTAX,
                                   "invalid numeric literal");
        p->token = 256;
        return 1;
      }
    if (!c || !strchr ("+-*/%()=;", c))
      return mulib_mune_fail_ (p, MULIB_MUNE_SYNTAX, "unsupported token");
    p->token = c;
    return 1;
  }
  int
  mulib_mune_get (const mulib_mune_t *m, const char *name, double *out)
  {
    if (!m || !name || !out)
      return 0;
    for (size_t i = 0; i < m->count; ++i)
      if (!strcmp (m->vars[i].name, name))
        {
          *out = m->vars[i].value;
          return 1;
        }
    return 0;
  }
  static int
  mulib_mune_expr_ (mulib_mune_parser_ *p, unsigned minimum, double *out)
  {
    double value = 0;
    if (++p->depth > 64)
      return mulib_mune_fail_ (p, MULIB_MUNE_LIMIT, "expression depth limit");
    if (p->token == '-')
      {
        unsigned sign = p->token;
        if (!mulib_mune_next_ (p) || !mulib_mune_expr_ (p, 30, &value))
          return 0;
        if (sign == '-')
          value = -value;
      }
    else if (p->token == '(')
      {
        if (!mulib_mune_next_ (p) || !mulib_mune_expr_ (p, 0, &value))
          return 0;
        if (p->token != ')')
          return mulib_mune_fail_ (p, MULIB_MUNE_SYNTAX,
                                   "expected closing parenthesis");
        if (!mulib_mune_next_ (p))
          return 0;
      }
    else if (p->token == 256)
      {
        value = p->number;
        if (!mulib_mune_next_ (p))
          return 0;
      }
    else if (p->token == 257)
      {
        if (!mulib_mune_get (p->env, p->word, &value))
          return mulib_mune_fail_ (p, MULIB_MUNE_NAME, "unknown variable");
        if (!mulib_mune_next_ (p))
          return 0;
      }
    else
      return mulib_mune_fail_ (p, MULIB_MUNE_SYNTAX,
                               "expected numeric expression");
    for (;;)
      {
        unsigned op = p->token;
        unsigned prec = (op == '+' || op == '-')                ? 10
                        : (op == '*' || op == '/' || op == '%') ? 20
                                                                : 0;
        double right;
        if (!prec || prec < minimum)
          break;
        if (!mulib_mune_next_ (p) || !mulib_mune_expr_ (p, prec + 1, &right))
          return 0;
        if ((op == '/' || op == '%') && right == 0)
          return mulib_mune_fail_ (p, MULIB_MUNE_ARITHMETIC,
                                   "division by zero");
        switch (op)
          {
          case '+':
            value += right;
            break;
          case '-':
            value -= right;
            break;
          case '*':
            value *= right;
            break;
          case '/':
            value /= right;
            break;
          default:
            value = fmod (value, right);
            if (value && ((value < 0) != (right < 0)))
              value += right;
            break;
          }
        if (!isfinite (value))
          return mulib_mune_fail_ (p, MULIB_MUNE_ARITHMETIC,
                                   "nonfinite arithmetic result");
      }
    --p->depth;
    *out = value;
    return 1;
  }
  mulib_mune_t *
  mulib_mune_create (void)
  {
    return (mulib_mune_t *)calloc (1, sizeof (mulib_mune_t));
  }
  void
  mulib_mune_destroy (mulib_mune_t *m)
  {
    free (m);
  }
  mulib_mune_status_t
  mulib_mune_eval (mulib_mune_t *m, const char *source, size_t n, double *out,
                   mulib_mune_error_t *error)
  {
    mulib_mune_parser_ p;
    mulib_mune_t env;
    double result = 0;
    memset (&p, 0, sizeof (p));
    p.error.message = "ok";
    if (!m || !out || (!source && n))
      {
        mulib_mune_fail_ (&p, MULIB_MUNE_INVALID, "invalid arguments");
        goto done;
      }
    if (n > 65536)
      {
        mulib_mune_fail_ (&p, MULIB_MUNE_LIMIT, "source size limit");
        goto done;
      }
    env = *m;
    p.env = &env;
    p.source = source;
    p.length = n;
    if (!mulib_mune_next_ (&p))
      goto done;
    while (p.token)
      {
        char name[64];
        double value;
        size_t i;
        if (p.token == ';')
          {
            if (!mulib_mune_next_ (&p))
              goto done;
            continue;
          }
        if (p.token == 258)
          {
            if (!mulib_mune_next_ (&p) || !mulib_mune_expr_ (&p, 0, &result))
              goto done;
            if (p.token == ';' && !mulib_mune_next_ (&p))
              goto done;
            if (p.token)
              {
                mulib_mune_fail_ (&p, MULIB_MUNE_SYNTAX,
                                  "return must be final");
                goto done;
              }
            break;
          }
        if (p.token != 257)
          {
            mulib_mune_fail_ (&p, MULIB_MUNE_SYNTAX,
                              "expected assignment or return");
            goto done;
          }
        strcpy (name, p.word);
        if (!mulib_mune_next_ (&p))
          goto done;
        if (p.token != '=')
          {
            mulib_mune_fail_ (&p, MULIB_MUNE_SYNTAX, "expected assignment");
            goto done;
          }
        if (!mulib_mune_next_ (&p) || !mulib_mune_expr_ (&p, 0, &value))
          goto done;
        for (i = 0; i < env.count && strcmp (env.vars[i].name, name); ++i)
          {
          }
        if (i == 128)
          {
            mulib_mune_fail_ (&p, MULIB_MUNE_LIMIT, "variable limit");
            goto done;
          }
        if (i == env.count)
          {
            strcpy (env.vars[i].name, name);
            ++env.count;
          }
        env.vars[i].value = value;
      }
    *m = env;
    *out = result;
  done:
    if (error)
      *error = p.error;
    return p.error.code;
  }
#ifdef __cplusplus
}
#endif
#endif
