/**
 * @file mulib-manpage.h
 * @brief An owned Linux roff manpage authoring object and section emitter.
 * @defgroup mulib_manpage MuLIB manpage
 * @{
 * @details Produces `.TH`, `.SH NAME`, `.SH SYNOPSIS`, and `.SH DESCRIPTION`
 * roff sections. Its output is deliberately minimal; it is not a complete
 * man(7) formatter and does not accept raw trusted roff markup. Treat
 * untrusted input cautiously: newlines and backslashes are replaced, but a
 * production manpage generator should also escape leading roff control
 * characters.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_MANPAGE before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_MANPAGE_H
#define MULIB_MANPAGE_H
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

  /** @brief An owned Linux roff manpage authoring object and section emitter.
   */
  typedef struct mulib_manpage
  {
    char *title, *section, *name, *synopsis, *description;
  } mulib_manpage_t;
  /** @brief mulib manpage linux; see manual/mulib-manpage.md for contracts and
   * examples. */
  mulib_manpage_t *mulib_manpage_linux (void);
  /** @brief mulib manpage set; see manual/mulib-manpage.md for contracts and
   * examples. */
  int mulib_manpage_set (mulib_manpage_t *m, const char *title,
                         const char *section, const char *synopsis,
                         const char *description);
  /** @brief mulib manpage render; see manual/mulib-manpage.md for contracts
   * and examples. */
  char *mulib_manpage_render (const mulib_manpage_t *m);
  /** @brief mulib manpage destroy; see manual/mulib-manpage.md for contracts
   * and examples. */
  void mulib_manpage_destroy (mulib_manpage_t *m);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_MANPAGE_H */

#if defined(MULIB_IMPL_MANPAGE) && !defined(MULIB_MANPAGE_IMPLEMENTED)
#define MULIB_MANPAGE_IMPLEMENTED
#ifdef __cplusplus
extern "C"
{
#endif
  static char *
  mulib_manpage_copy_ (const char *s)
  {
    if (!s)
      return NULL;
    size_t n = strlen (s);
    char *p = malloc (n + 1);
    if (p)
      memcpy (p, s, n + 1);
    return p;
  }
  mulib_manpage_t *
  mulib_manpage_linux (void)
  {
    mulib_manpage_t *m = calloc (1, sizeof (*m));
    return m;
  }
  void
  mulib_manpage_destroy (mulib_manpage_t *m)
  {
    if (m)
      {
        free (m->title);
        free (m->section);
        free (m->name);
        free (m->synopsis);
        free (m->description);
        free (m);
      }
  }
  int
  mulib_manpage_set (mulib_manpage_t *m, const char *t, const char *s,
                     const char *y, const char *d)
  {
    if (!m || !t || !s || !y || !d)
      return 0;
    char *a = mulib_manpage_copy_ (t), *b = mulib_manpage_copy_ (s),
         *c = mulib_manpage_copy_ (y), *e = mulib_manpage_copy_ (d);
    if (!a || !b || !c || !e)
      {
        free (a);
        free (b);
        free (c);
        free (e);
        return 0;
      }
    free (m->title);
    free (m->section);
    free (m->synopsis);
    free (m->description);
    m->title = a;
    m->section = b;
    m->synopsis = c;
    m->description = e;
    return 1;
  }
  static int
  mulib_manpage_safe_ (char *p, const char *s)
  {
    while (*s)
      {
        if (*s == '\\' || *s == '\n' || *s == '\r')
          {
            *p++ = ' ';
          }
        else
          *p++ = *s;
        s++;
      }
    *p = 0;
    return 1;
  }
  char *
  mulib_manpage_render (const mulib_manpage_t *m)
  {
    const char *parts[5];
    size_t n = 200;
    char *r, *t, *s, *y, *d;
    int result;
    if (!m || !m->title || !m->section || !m->synopsis || !m->description)
      return NULL;
    parts[0] = parts[1] = m->title;
    parts[2] = m->section;
    parts[3] = m->synopsis;
    parts[4] = m->description;
    for (size_t i = 0; i < 5; ++i)
      {
        size_t length = strlen (parts[i]);
        if (length > SIZE_MAX - n)
          return NULL;
        n += length;
      }
    r = (char *)malloc (n);
    if (!r)
      return NULL;
    t = (char *)malloc (strlen (m->title) + 1);
    s = (char *)malloc (strlen (m->section) + 1);
    y = (char *)malloc (strlen (m->synopsis) + 1);
    d = (char *)malloc (strlen (m->description) + 1);
    if (!t || !s || !y || !d)
      {
        free (t);
        free (s);
        free (y);
        free (d);
        free (r);
        return NULL;
      }
    mulib_manpage_safe_ (t, m->title);
    mulib_manpage_safe_ (s, m->section);
    mulib_manpage_safe_ (y, m->synopsis);
    mulib_manpage_safe_ (d, m->description);
    result = snprintf (r, n,
                       ".TH %s %s\n.SH NAME\n%s%s\n.SH SYNOPSIS\n%s%s\n.SH "
                       "DESCRIPTION\n%s%s\n",
                       t, s, (*t == '.' || *t == '\'') ? "\\&" : "", t,
                       (*y == '.' || *y == '\'') ? "\\&" : "", y,
                       (*d == '.' || *d == '\'') ? "\\&" : "", d);
    free (t);
    free (s);
    free (y);
    free (d);
    if (result < 0 || (size_t)result >= n)
      {
        free (r);
        return NULL;
      }
    return r;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
