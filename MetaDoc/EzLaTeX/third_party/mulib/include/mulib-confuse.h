/** @file mulib-confuse.h
 * @brief Libconfuse-style configuration, parsed by scannerless PEG
 * combinators. Define MULIB_IMPL_CONFUSE and explicitly implement PARSER,
 * PARSEC, PEG, AST, STRBUF and GLOB. This is a dynamic configuration tree, not
 * libconfuse ABI.
 */
#ifndef MULIB_CONFUSE_H
#define MULIB_CONFUSE_H
#include "mulib-parsec.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum mulib_confuse_kind
  {
    MULIB_CONFUSE_SECTION,
    MULIB_CONFUSE_LIST,
    MULIB_CONFUSE_STRING,
    MULIB_CONFUSE_INTEGER,
    MULIB_CONFUSE_REAL,
    MULIB_CONFUSE_BOOLEAN
  } mulib_confuse_kind_t;
  typedef struct mulib_confuse_value
  {
    mulib_confuse_kind_t kind;
    char *name,
        *title; /* List elements have no name; sections may have titles. */
    union
    {
      char *string;
      int64_t integer;
      double real;
      int boolean;
    } as;
    struct mulib_confuse_value *child, *next;
  } mulib_confuse_value_t;
  typedef mulib_confuse_value_t mulib_confuse_t;
  /** Parse an explicit byte range or a file. Entire input is checked. Returned
   * root is an owned section; repeated names/sections retain source order.
   * Supports key=value, titled sections, lists, quoted/bare strings, numbers,
   * true/false/yes/no/on/off and #, //, block comments. Semicolons are
   * optional.
   */
  mulib_confuse_t *mulib_confuse_parse (const char *source, size_t length,
                                        mulib_parser_error_t *error);
  mulib_confuse_t *mulib_confuse_load (const char *filename,
                                       mulib_parser_error_t *error);
  void mulib_confuse_destroy (mulib_confuse_t *config);
  /** Borrow the nth child with a name; NULL name selects the nth child
   * overall. */
  const mulib_confuse_value_t *
  mulib_confuse_get (const mulib_confuse_value_t *parent, const char *name,
                     size_t index);
  /** Typed lookups return fallbacks for absent or differently typed entries.
   */
  const char *mulib_confuse_getstr (const mulib_confuse_t *config,
                                    const char *name, const char *fallback);
  int64_t mulib_confuse_getint (const mulib_confuse_t *config,
                                const char *name, int64_t fallback);
  double mulib_confuse_getreal (const mulib_confuse_t *config,
                                const char *name, double fallback);
  int mulib_confuse_getbool (const mulib_confuse_t *config, const char *name,
                             int fallback);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONFUSE) && !defined(MULIB_CONFUSE_IMPLEMENTED)
#define MULIB_CONFUSE_IMPLEMENTED
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static const char mulib_confuse_grammar_[]
      = "config <- WS entry*;"
        "entry <- NAME WS (assign / section);"
        "assign <- '=' WS value ';'? WS;"
        "section <- title? '{' WS entry* '}' WS ';'? WS;"
        "title <- (STRING / NAME) WS;"
        "value <- (list / STRING / NUMBER / BARE) WS;"
        "list <- ('{' WS items? '}' / '[' WS items? ']');"
        "items <- value (',' WS value)* (',' WS)?;"
        "NAME <- [a-zA-Z_] [a-zA-Z0-9_.:/-]*;"
        "BARE <- [a-zA-Z_] [a-zA-Z0-9_.:/-]*;"
        "NUMBER <- [+-]? [0-9]+ ('.' [0-9]+)? ([eE] [+-]? [0-9]+)? "
        "![a-zA-Z0-9_.];"
        "STRING <- '\"' ('\\\\' [\\\\\"nrt] / ![\"\\\\] .)* '\"' / "
        "\"'\" ('\\\\' [\\\\'nrt] / !['\\\\] .)* \"'\";"
        "WS <- ([ \\t\\r\\n] / '#' (!'\\n' .)* / '//' (!'\\n' .)* / '/*' "
        "(!'*/' .)* '*/')*;";
  static char *
  mulib_confuse_text_ (const mulib_ast_t *n, int quoted)
  {
    size_t i, end, len = 0;
    char *s;
    if (!n)
      return NULL;
    s = (char *)malloc (n->length + 1);
    if (!s)
      return NULL;
    i = quoted ? 1 : 0;
    end = n->length - (quoted ? 1 : 0);
    for (; i < end; ++i)
      {
        char c = n->text[i];
        if (quoted && c == '\\')
          {
            c = n->text[++i];
            if (c == 'n')
              c = '\n';
            else if (c == 'r')
              c = '\r';
            else if (c == 't')
              c = '\t';
          }
        s[len++] = c;
      }
    s[len] = 0;
    return s;
  }
  static const mulib_ast_t *
  mulib_confuse_find_ (const mulib_ast_t *n, const char *name)
  {
    const mulib_ast_t *c;
    if (!n)
      return NULL;
    for (c = n->child; c; c = c->next)
      if (!strcmp (c->production, name))
        return c;
    return NULL;
  }
  void
  mulib_confuse_destroy (mulib_confuse_t *v)
  {
    while (v)
      {
        mulib_confuse_t *next = v->next;
        mulib_confuse_destroy (v->child);
        if (v->kind == MULIB_CONFUSE_STRING)
          free (v->as.string);
        free (v->name);
        free (v->title);
        free (v);
        v = next;
      }
  }
  static mulib_confuse_t *mulib_confuse_convert_ (const mulib_ast_t *n,
                                                  mulib_parser_error_t *e);
  static int
  mulib_confuse_children_ (mulib_confuse_t *v, const mulib_ast_t *n,
                           const char *name, mulib_parser_error_t *e)
  {
    const mulib_ast_t *c;
    mulib_confuse_t **tail = &v->child;
    for (c = n->child; c; c = c->next)
      if (!strcmp (c->production, name))
        {
          *tail = mulib_confuse_convert_ (c, e);
          if (!*tail)
            return 0;
          tail = &(*tail)->next;
        }
    return 1;
  }
  static void
  mulib_confuse_error_ (mulib_parser_error_t *e,
                        mulib_parser_error_code_t code, const char *message,
                        size_t offset)
  {
    if (e)
      {
        memset (e, 0, sizeof *e);
        e->code = code;
        e->message = message;
        e->offset = offset;
      }
  }
  static mulib_confuse_t *
  mulib_confuse_convert_ (const mulib_ast_t *n, mulib_parser_error_t *e)
  {
    mulib_confuse_t *v = NULL;
    const mulib_ast_t *c, *a;
    if (!strcmp (n->production, "entry"))
      {
        a = mulib_confuse_find_ (n, "assign");
        v = mulib_confuse_convert_ (a ? mulib_confuse_find_ (a, "value")
                                      : mulib_confuse_find_ (n, "section"),
                                    e);
        if (!v)
          return NULL;
        v->name = mulib_confuse_text_ (mulib_confuse_find_ (n, "NAME"), 0);
        if (!v->name)
          goto memory;
        return v;
      }
    if (!strcmp (n->production, "value"))
      {
        for (c = n->child; c; c = c->next)
          if (strcmp (c->production, "WS"))
            return mulib_confuse_convert_ (c, e);
        return NULL;
      }
    v = (mulib_confuse_t *)calloc (1, sizeof *v);
    if (!v)
      goto memory;
    if (!strcmp (n->production, "config")
        || !strcmp (n->production, "section"))
      {
        v->kind = MULIB_CONFUSE_SECTION;
        c = mulib_confuse_find_ (n, "title");
        if (c)
          {
            a = mulib_confuse_find_ (c, "STRING");
            v->title = mulib_confuse_text_ (
                a ? a : mulib_confuse_find_ (c, "NAME"), a != NULL);
            if (!v->title)
              goto memory;
          }
        if (!mulib_confuse_children_ (v, n, "entry", e))
          goto fail;
      }
    else if (!strcmp (n->production, "list"))
      {
        v->kind = MULIB_CONFUSE_LIST;
        c = mulib_confuse_find_ (n, "items");
        if (c && !mulib_confuse_children_ (v, c, "value", e))
          goto fail;
      }
    else if (!strcmp (n->production, "NUMBER"))
      {
        char *end;
        errno = 0;
        if (strpbrk (n->text, ".eE"))
          {
            v->kind = MULIB_CONFUSE_REAL;
            v->as.real = strtod (n->text, &end);
            if (errno == ERANGE || !isfinite (v->as.real) || *end)
              goto number;
          }
        else
          {
            long long number;
            v->kind = MULIB_CONFUSE_INTEGER;
            number = strtoll (n->text, &end, 10);
            if (errno == ERANGE || *end || number < INT64_MIN
                || number > INT64_MAX)
              goto number;
            v->as.integer = (int64_t)number;
          }
      }
    else if (!strcmp (n->production, "BARE")
             && (!strcmp (n->text, "true") || !strcmp (n->text, "yes")
                 || !strcmp (n->text, "on") || !strcmp (n->text, "false")
                 || !strcmp (n->text, "no") || !strcmp (n->text, "off")))
      {
        v->kind = MULIB_CONFUSE_BOOLEAN;
        v->as.boolean = !strcmp (n->text, "true") || !strcmp (n->text, "yes")
                        || !strcmp (n->text, "on");
      }
    else
      {
        v->kind = MULIB_CONFUSE_STRING;
        v->as.string
            = mulib_confuse_text_ (n, !strcmp (n->production, "STRING"));
        if (!v->as.string)
          goto memory;
      }
    return v;
  number:
    mulib_confuse_error_ (e, MULIB_PARSER_SYNTAX, "number out of range",
                          n->offset);
    goto fail;
  memory:
    mulib_confuse_error_ (e, MULIB_PARSER_MEMORY, "out of memory", n->offset);
  fail:
    mulib_confuse_destroy (v);
    return NULL;
  }
  static mulib_confuse_t *
  mulib_confuse_finish_ (mulib_ast_walker_t *w, mulib_parser_error_t *e)
  {
    mulib_confuse_t *v;
    if (!w)
      return NULL;
    if (memchr (w->root->text, 0, w->root->length))
      {
        mulib_confuse_error_ (
            e, MULIB_PARSER_SYNTAX, "NUL bytes are not configuration text",
            (size_t)((const char *)memchr (w->root->text, 0, w->root->length)
                     - w->root->text));
        v = NULL;
      }
    else
      v = mulib_confuse_convert_ (w->root, e);
    if (!v && e)
      {
        size_t i;
        e->line = e->column = 1;
        for (i = 0; i < e->offset; ++i)
          {
            if (w->root->text[i] == '\n')
              {
                ++e->line;
                e->column = 1;
              }
            else
              ++e->column;
          }
      }
    mulib_ast_walker_destroy (w);
    return v;
  }
  mulib_confuse_t *
  mulib_confuse_parse (const char *s, size_t len, mulib_parser_error_t *e)
  {
    mulib_parser_t *p = mulib_parser_from_peg_ex (mulib_confuse_grammar_, e);
    mulib_ast_walker_t *w;
    if (!p)
      return NULL;
    w = mulib_parser_parse (p, NULL, s, len, NULL, 0, NULL, e);
    mulib_parser_destroy (p);
    return mulib_confuse_finish_ (w, e);
  }
  mulib_confuse_t *
  mulib_confuse_load (const char *path, mulib_parser_error_t *e)
  {
    mulib_parser_t *p = mulib_parser_from_peg_ex (mulib_confuse_grammar_, e);
    mulib_ast_walker_t *w;
    if (!p)
      return NULL;
    w = mulib_parser_parse_to_walker (p, path, e);
    mulib_parser_destroy (p);
    return mulib_confuse_finish_ (w, e);
  }
  const mulib_confuse_value_t *
  mulib_confuse_get (const mulib_confuse_value_t *v, const char *name,
                     size_t index)
  {
    const mulib_confuse_value_t *c;
    if (!v
        || (v->kind != MULIB_CONFUSE_SECTION && v->kind != MULIB_CONFUSE_LIST))
      return NULL;
    for (c = v->child; c; c = c->next)
      if (!name || (c->name && !strcmp (c->name, name)))
        {
          if (!index)
            return c;
          --index;
        }
    return NULL;
  }
  const char *
  mulib_confuse_getstr (const mulib_confuse_t *v, const char *name,
                        const char *fallback)
  {
    v = mulib_confuse_get (v, name, 0);
    return v && v->kind == MULIB_CONFUSE_STRING ? v->as.string : fallback;
  }
  int64_t
  mulib_confuse_getint (const mulib_confuse_t *v, const char *name,
                        int64_t fallback)
  {
    v = mulib_confuse_get (v, name, 0);
    return v && v->kind == MULIB_CONFUSE_INTEGER ? v->as.integer : fallback;
  }
  double
  mulib_confuse_getreal (const mulib_confuse_t *v, const char *name,
                         double fallback)
  {
    v = mulib_confuse_get (v, name, 0);
    return v && v->kind == MULIB_CONFUSE_REAL ? v->as.real : fallback;
  }
  int
  mulib_confuse_getbool (const mulib_confuse_t *v, const char *name,
                         int fallback)
  {
    v = mulib_confuse_get (v, name, 0);
    return v && v->kind == MULIB_CONFUSE_BOOLEAN ? v->as.boolean : fallback;
  }
#ifdef __cplusplus
}
#endif
#endif
