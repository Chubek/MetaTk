/**
 * @file mulib-argparse.h
 * @brief Lightweight GNU-style options and positional arguments parser.
 * @defgroup mulib_argparse MuLIB argparse
 * @{
 * @details Parses `--key=value`, `--key value`, `-k value`, bare flags
 * defaulting to "1", and operands after `--`. Short clusters are not expanded.
 * Value interpretation is deliberately heuristic; a negative-number argument
 * should be given with `=` to avoid option ambiguity. Positionals borrow argv
 * pointers.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_ARGPARSE before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_ARGPARSE_H
#define MULIB_ARGPARSE_H
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

  /** @brief Lightweight GNU-style options and positional arguments parser. */
  typedef struct mulib_argparse_option
  {
    char *key, *value;
  } mulib_argparse_option_t;
  /** @brief Lightweight GNU-style options and positional arguments parser. */
  typedef struct mulib_argparse
  {
    mulib_argparse_option_t *items;
    size_t length, capacity;
    char **positionals;
    size_t positional_count;
  } mulib_argparse_t;
  /** @brief mulib argparse load; see manual/mulib-argparse.md for contracts
   * and examples. */
  mulib_argparse_t *mulib_argparse_load (void);
  /** @brief mulib argparse parse; see manual/mulib-argparse.md for contracts
   * and examples. */
  int mulib_argparse_parse (mulib_argparse_t *a, int argc, char *const argv[]);
  /** @brief mulib argparse get; see manual/mulib-argparse.md for contracts and
   * examples. */
  const char *mulib_argparse_get (const mulib_argparse_t *a, const char *name);
  /** @brief mulib argparse has; see manual/mulib-argparse.md for contracts and
   * examples. */
  int mulib_argparse_has (const mulib_argparse_t *a, const char *name);
  /** @brief mulib argparse destroy; see manual/mulib-argparse.md for contracts
   * and examples. */
  void mulib_argparse_destroy (mulib_argparse_t *a);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ARGPARSE_H */

#if defined(MULIB_IMPL_ARGPARSE) && !defined(MULIB_ARGPARSE_IMPLEMENTED)
#define MULIB_ARGPARSE_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  static char *
  mulib_argparse_dup_ (const char *s, size_t n)
  {
    char *p = malloc (n + 1);
    if (p)
      {
        memcpy (p, s, n);
        p[n] = 0;
      }
    return p;
  }
  mulib_argparse_t *
  mulib_argparse_load (void)
  {
    return calloc (1, sizeof (mulib_argparse_t));
  }
  void
  mulib_argparse_destroy (mulib_argparse_t *a)
  {
    if (!a)
      return;
    for (size_t i = 0; i < a->length; i++)
      {
        free (a->items[i].key);
        free (a->items[i].value);
      }
    free (a->items);
    free (a->positionals);
    free (a);
  }
  static int
  mulib_argparse_put_ (mulib_argparse_t *a, const char *k, size_t n,
                       const char *v)
  {
    if (!n)
      return 0;
    for (size_t i = 0; i < a->length; i++)
      if (strlen (a->items[i].key) == n && !memcmp (k, a->items[i].key, n))
        {
          char *p = mulib_argparse_dup_ (v, strlen (v));
          if (!p)
            return 0;
          free (a->items[i].value);
          a->items[i].value = p;
          return 1;
        }
    if (a->length == a->capacity)
      {
        size_t cap = a->capacity ? a->capacity * 2 : 8;
        if (cap < a->capacity || cap > SIZE_MAX / sizeof (*a->items))
          return 0;
        void *p = realloc (a->items, cap * sizeof (*a->items));
        if (!p)
          return 0;
        a->items = p;
        a->capacity = cap;
      }
    char *key = mulib_argparse_dup_ (k, n),
         *value = mulib_argparse_dup_ (v, strlen (v));
    if (!key || !value)
      {
        free (key);
        free (value);
        return 0;
      }
    a->items[a->length++] = (mulib_argparse_option_t){ key, value };
    return 1;
  }
  int
  mulib_argparse_parse (mulib_argparse_t *a, int argc, char *const argv[])
  {
    if (!a || argc < 0 || (!argv && argc))
      return 0;
    int stop = 0;
    for (int i = 1; i < argc; i++)
      {
        char *s = argv[i];
        if (!s)
          continue;
        if (!stop && !strcmp (s, "--"))
          {
            stop = 1;
            continue;
          }
        if (!stop && s[0] == '-' && s[1])
          {
            char *k = s + (s[1] == '-' ? 2 : 1);
            if (!*k)
              return 0;
            char *eq = strchr (k, '=');
            size_t n = eq ? (size_t)(eq - k) : strlen (k);
            const char *v = eq ? eq + 1 : "1";
            if (!eq && i + 1 < argc && argv[i + 1] && argv[i + 1][0] != '-')
              v = argv[++i];
            if (!mulib_argparse_put_ (a, k, n, v))
              return 0;
          }
        else
          {
            if (a->positional_count == SIZE_MAX / sizeof (char *))
              return 0;
            void *p = realloc (a->positionals,
                               (a->positional_count + 1) * sizeof (char *));
            if (!p)
              return 0;
            a->positionals = p;
            a->positionals[a->positional_count++] = s;
          }
      }
    return 1;
  }
  const char *
  mulib_argparse_get (const mulib_argparse_t *a, const char *k)
  {
    if (!a || !k)
      return NULL;
    for (size_t i = 0; i < a->length; i++)
      if (!strcmp (a->items[i].key, k))
        return a->items[i].value;
    return NULL;
  }
  int
  mulib_argparse_has (const mulib_argparse_t *a, const char *k)
  {
    return mulib_argparse_get (a, k) != NULL;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
