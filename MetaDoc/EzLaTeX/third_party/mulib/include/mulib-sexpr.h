/**
 * @file mulib-sexpr.h
 * @brief A tree-based S-expression parser with atoms, lists, and deterministic
 * rendering.
 * @defgroup mulib_sexpr MuLIB sexpr
 * @{
 * @details The parser accepts proper nested lists, unquoted atoms, and
 * double-quoted atoms with basic escapes. It has a recursion-depth limit of
 * 128. The returned tree owns all atom strings; destroy it recursively. Parse
 * does not enforce end-of-input; consult the optional end pointer.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_SEXPR before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_SEXPR_H
#define MULIB_SEXPR_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief A tree-based S-expression parser with atoms, lists, and
   * deterministic rendering. */
  typedef enum mulib_sexpr_kind
  {
    MULIB_SEXPR_ATOM = 1,
    MULIB_SEXPR_LIST = 2
  } mulib_sexpr_kind_t;
  /** @brief A tree-based S-expression parser with atoms, lists, and
   * deterministic rendering. */
  typedef struct mulib_sexpr
  {
    mulib_sexpr_kind_t kind;
    char *atom;
    struct mulib_sexpr *child, *next;
  } mulib_sexpr_t;
  /** @brief mulib sexpr cont; see manual/mulib-sexpr.md for contracts and
   * examples. */
  mulib_sexpr_t *mulib_sexpr_cont (void);
  /** @brief mulib sexpr parse; see manual/mulib-sexpr.md for contracts and
   * examples. */
  mulib_sexpr_t *mulib_sexpr_parse (const char *source, const char **end);
  /** @brief mulib sexpr push; see manual/mulib-sexpr.md for contracts and
   * examples. */
  int mulib_sexpr_push (mulib_sexpr_t *list, mulib_sexpr_t *value);
  /** @brief mulib sexpr render; see manual/mulib-sexpr.md for contracts and
   * examples. */
  char *mulib_sexpr_render (const mulib_sexpr_t *value);
  /** @brief mulib sexpr destroy; see manual/mulib-sexpr.md for contracts and
   * examples. */
  void mulib_sexpr_destroy (mulib_sexpr_t *value);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SEXPR_H */

#if defined(MULIB_IMPL_SEXPR) && !defined(MULIB_SEXPR_IMPLEMENTED)
#define MULIB_SEXPR_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  static mulib_sexpr_t *
  mulib_sexpr_new_ (int kind)
  {
    mulib_sexpr_t *n = calloc (1, sizeof (*n));
    if (n)
      n->kind = (mulib_sexpr_kind_t)kind;
    return n;
  }
  mulib_sexpr_t *
  mulib_sexpr_cont (void)
  {
    return mulib_sexpr_new_ (MULIB_SEXPR_LIST);
  }
  void
  mulib_sexpr_destroy (mulib_sexpr_t *v)
  {
    /* Thread each detached child back to its owner: no recursion or
     * allocation. */
    while (v)
      {
        if (v->child)
          {
            mulib_sexpr_t *c = v->child;
            v->child = c->next;
            c->next = v;
            v = c;
          }
        else
          {
            mulib_sexpr_t *next = v->next;
            free (v->atom);
            free (v);
            v = next;
          }
      }
  }
  static int
  mulib_sexpr_contains_ (const mulib_sexpr_t *v, const mulib_sexpr_t *needle,
                         unsigned depth)
  {
    if (depth > 128)
      return 1;
    for (; v; v = v->next)
      if (v == needle || mulib_sexpr_contains_ (v->child, needle, depth + 1))
        return 1;
    return 0;
  }
  int
  mulib_sexpr_push (mulib_sexpr_t *l, mulib_sexpr_t *v)
  {
    if (!l || l->kind != MULIB_SEXPR_LIST || !v || v->next
        || mulib_sexpr_contains_ (v, l, 0))
      return 0;
    mulib_sexpr_t **p = &l->child;
    while (*p)
      {
        if (*p == v)
          return 0;
        p = &(*p)->next;
      }
    *p = v;
    return 1;
  }
  static void
  mulib_sexpr_skip_ (const char **p)
  {
    while (isspace ((unsigned char)**p))
      ++*p;
  }
  static mulib_sexpr_t *
  mulib_sexpr_read_ (const char **p, int depth)
  {
    if (depth > 128)
      return NULL;
    mulib_sexpr_skip_ (p);
    if (!**p || **p == ')')
      return NULL;
    if (**p == '(')
      {
        ++*p;
        mulib_sexpr_t *l = mulib_sexpr_cont ();
        if (!l)
          return NULL;
        mulib_sexpr_t **tail = &l->child;
        for (;;)
          {
            mulib_sexpr_skip_ (p);
            if (**p == ')')
              {
                ++*p;
                return l;
              }
            if (!**p)
              {
                mulib_sexpr_destroy (l);
                return NULL;
              }
            mulib_sexpr_t *item = mulib_sexpr_read_ (p, depth + 1);
            if (!item)
              {
                mulib_sexpr_destroy (l);
                return NULL;
              }
            *tail = item;
            tail = &item->next;
          }
      }
    const char *start = *p;
    int quote = **p == '"', closed = !quote;
    if (quote)
      ++*p;
    size_t cap = strlen (*p) + 1;
    char *s = malloc (cap);
    if (!s)
      return NULL;
    size_t n = 0;
    while (**p)
      {
        char c = *(*p)++;
        if (quote)
          {
            if (c == '"')
              {
                closed = 1;
                break;
              }
            if (c == '\\')
              {
                c = **p;
                if (!c)
                  {
                    free (s);
                    return NULL;
                  }
                ++*p;
                if (c == 'n')
                  c = '\n';
                else if (c == 't')
                  c = '\t';
              }
          }
        else if (isspace ((unsigned char)c) || c == '(' || c == ')')
          {
            --*p;
            break;
          }
        s[n++] = c;
      }
    if (!closed)
      {
        free (s);
        return NULL;
      }
    if (!quote && *p == start)
      {
        free (s);
        return NULL;
      }
    s[n] = 0;
    mulib_sexpr_t *v = mulib_sexpr_new_ (MULIB_SEXPR_ATOM);
    if (!v)
      {
        free (s);
        return NULL;
      }
    v->atom = s;
    return v;
  }
  mulib_sexpr_t *
  mulib_sexpr_parse (const char *s, const char **end)
  {
    if (!s)
      return NULL;
    const char *p = s;
    mulib_sexpr_t *v = mulib_sexpr_read_ (&p, 0);
    if (end)
      *end = p;
    return v;
  }
  static int
  mulib_sexpr_reserve_ (char **buf, size_t len, size_t *cap, size_t add)
  {
    size_t need, capacity;
    char *next;
    if (len == SIZE_MAX || add > SIZE_MAX - len - 1)
      return 0;
    need = len + add + 1;
    if (need <= *cap)
      return 1;
    capacity = *cap ? *cap : 64;
    while (capacity < need)
      {
        if (capacity > SIZE_MAX / 2)
          {
            capacity = need;
            break;
          }
        capacity *= 2;
      }
    next = (char *)realloc (*buf, capacity);
    if (!next)
      return 0;
    *buf = next;
    *cap = capacity;
    return 1;
  }
  static int
  mulib_sexpr_put_ (char **buf, size_t *len, size_t *cap, char c)
  {
    if (!mulib_sexpr_reserve_ (buf, *len, cap, 1))
      return 0;
    (*buf)[(*len)++] = c;
    return 1;
  }
  static int
  mulib_sexpr_emit_ (const mulib_sexpr_t *v, char **buf, size_t *len,
                     size_t *cap, int depth)
  {
    if (depth > 128)
      return 0;
    for (; v; v = v->next)
      {
        if (v->kind == MULIB_SEXPR_LIST)
          {
            if (!mulib_sexpr_put_ (buf, len, cap, '(')
                || !mulib_sexpr_emit_ (v->child, buf, len, cap, depth + 1)
                || !mulib_sexpr_put_ (buf, len, cap, ')'))
              return 0;
          }
        else if (v->kind == MULIB_SEXPR_ATOM)
          {
            const char *s = v->atom ? v->atom : "";
            size_t n = strlen (s);
            int quote = !n;
            for (size_t i = 0; i < n; ++i)
              if (isspace ((unsigned char)s[i]) || strchr ("()\\\"", s[i]))
                quote = 1;
            if (n > (SIZE_MAX - 2) / 2
                || !mulib_sexpr_reserve_ (buf, *len, cap, n * 2 + 2))
              return 0;
            if (quote)
              (*buf)[(*len)++] = '"';
            for (size_t i = 0; i < n; ++i)
              {
                char c = s[i];
                if (c == '"' || c == '\\')
                  (*buf)[(*len)++] = '\\';
                if (c == '\n')
                  {
                    (*buf)[(*len)++] = '\\';
                    c = 'n';
                  }
                (*buf)[(*len)++] = c;
              }
            if (quote)
              (*buf)[(*len)++] = '"';
          }
        else
          return 0;
        if (v->next && !mulib_sexpr_put_ (buf, len, cap, ' '))
          return 0;
      }
    return 1;
  }
  char *
  mulib_sexpr_render (const mulib_sexpr_t *v)
  {
    char *b = NULL;
    size_t len = 0, cap = 0;
    if (!mulib_sexpr_emit_ (v, &b, &len, &cap, 0)
        || !mulib_sexpr_reserve_ (&b, len, &cap, 0))
      {
        free (b);
        return NULL;
      }
    b[len] = 0;
    return b;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
