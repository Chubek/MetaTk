/**
 * @file mulib-xpath.h
 * @brief Focused XPath-style node selection over the MuLIB XML DOM.
 *
 * Supports /, //, relative paths, ., .., names, *, node(), text(), comment(),
 * [N], [@name], [@name='value'] and [text()='value']. No namespaces,
 * arithmetic, general functions or attribute-valued results. See the manual.
 * Implement with MULIB_IMPL_XPATH plus XML, STRBUF and UTF8 switches.
 */
#ifndef MULIB_XPATH_H
#define MULIB_XPATH_H
#include "mulib-xml.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Owned array of borrowed nodes, unique and in document order.
   * Free the array before reuse; nodes must outlive the result. */
  typedef struct mulib_xpath_result
  {
    mulib_xml_node_t **nodes;
    size_t count;
  } mulib_xpath_result_t;
  /** @brief Evaluate a NUL-terminated expression; 1 success (possibly empty),
   * 0 error. out must be non-NULL and not own a previous result; it is zeroed
   * on failure. Absolute paths require a document ancestor. Limit: 64 steps, 8
   * predicates/step. */
  int mulib_xpath_query (mulib_xml_node_t *context, const char *expression,
                         mulib_xpath_result_t *out, mulib_xml_error_t *error);
  /** @brief Release only the result array and zero the result. */
  void mulib_xpath_result_free (mulib_xpath_result_t *result);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_XPATH_H */

#if defined(MULIB_IMPL_XPATH) && !defined(MULIB_XPATH_IMPLEMENTED)
#define MULIB_XPATH_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_xpath_pred_internal
  {
    int kind;
    const char *name, *value;
    size_t name_length, value_length, position;
  } mulib_xpath_pred_internal;
  typedef struct mulib_xpath_step_internal
  {
    int test, descendant;
    const char *name;
    size_t name_length, count;
    mulib_xpath_pred_internal predicates[8];
  } mulib_xpath_step_internal;
  typedef struct mulib_xpath_parser_internal
  {
    const char *s;
    size_t p;
    mulib_xml_error_t *error;
  } mulib_xpath_parser_internal;
  static int
  mulib_xpath_fail_internal (mulib_xpath_parser_internal *p,
                             const char *message)
  {
    if (p->error)
      {
        p->error->offset = p->p;
        p->error->message = message;
      }
    return 0;
  }
  static int
  mulib_xpath_space_internal (char c)
  {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
  }
  static void
  mulib_xpath_skip_internal (mulib_xpath_parser_internal *p)
  {
    while (mulib_xpath_space_internal (p->s[p->p]))
      p->p++;
  }
  static int
  mulib_xpath_namechar_internal (char c, int first)
  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
           || c == ':'
           || (!first && ((c >= '0' && c <= '9') || c == '.' || c == '-'));
  }
  static int
  mulib_xpath_name_internal (mulib_xpath_parser_internal *p, const char **s,
                             size_t *n)
  {
    size_t begin = p->p;
    if (!mulib_xpath_namechar_internal (p->s[begin], 1))
      return mulib_xpath_fail_internal (p, "expected XML name");
    p->p++;
    while (mulib_xpath_namechar_internal (p->s[p->p], 0))
      p->p++;
    *s = p->s + begin;
    *n = p->p - begin;
    return 1;
  }
  static int
  mulib_xpath_equal_internal (const char *s, const char *p, size_t n)
  {
    return s && strlen (s) == n && !memcmp (s, p, n);
  }
  static const char *
  mulib_xpath_attr_internal (const mulib_xml_node_t *node, const char *name,
                             size_t n)
  {
    size_t i;
    for (i = 0; i < mulib_xml_attr_count (node); i++)
      if (mulib_xpath_equal_internal (mulib_xml_attr_name (node, i), name, n))
        return mulib_xml_attr_value (node, i);
    return NULL;
  }
  static int
  mulib_xpath_pred_parse_internal (mulib_xpath_parser_internal *p,
                                   mulib_xpath_pred_internal *pred)
  {
    memset (pred, 0, sizeof (*pred));
    p->p++;
    mulib_xpath_skip_internal (p);
    if (p->s[p->p] >= '0' && p->s[p->p] <= '9')
      {
        pred->kind = 1;
        do
          {
            unsigned d = (unsigned)(p->s[p->p++] - '0');
            if (pred->position > (SIZE_MAX - d) / 10)
              return mulib_xpath_fail_internal (p, "position overflow");
            pred->position = pred->position * 10 + d;
          }
        while (p->s[p->p] >= '0' && p->s[p->p] <= '9');
        if (!pred->position)
          return mulib_xpath_fail_internal (p, "positions are one-based");
      }
    else
      {
        if (p->s[p->p] == '@')
          {
            p->p++;
            pred->kind = 2;
            if (!mulib_xpath_name_internal (p, &pred->name,
                                            &pred->name_length))
              return 0;
          }
        else if (!strncmp (p->s + p->p, "text()", 6))
          {
            p->p += 6;
            pred->kind = 4;
          }
        else
          return mulib_xpath_fail_internal (p, "unsupported predicate");
        mulib_xpath_skip_internal (p);
        if (p->s[p->p] == '=')
          {
            char quote;
            size_t begin;
            if (pred->kind == 2)
              pred->kind = 3;
            p->p++;
            mulib_xpath_skip_internal (p);
            quote = p->s[p->p];
            if (quote != '\'' && quote != '"')
              return mulib_xpath_fail_internal (
                  p, "predicate value must be quoted");
            begin = ++p->p;
            while (p->s[p->p] && p->s[p->p] != quote)
              p->p++;
            if (!p->s[p->p])
              return mulib_xpath_fail_internal (
                  p, "unterminated predicate string");
            pred->value = p->s + begin;
            pred->value_length = p->p - begin;
            p->p++;
          }
        else if (pred->kind == 4)
          return mulib_xpath_fail_internal (
              p, "text predicate requires equality");
      }
    mulib_xpath_skip_internal (p);
    if (p->s[p->p] != ']')
      return mulib_xpath_fail_internal (p, "expected ']'");
    p->p++;
    return 1;
  }
  static int
  mulib_xpath_step_parse_internal (mulib_xpath_parser_internal *p,
                                   mulib_xpath_step_internal *s,
                                   int descendant)
  {
    memset (s, 0, sizeof (*s));
    s->descendant = descendant;
    mulib_xpath_skip_internal (p);
    if (p->s[p->p] == '.')
      {
        p->p++;
        s->test = 5;
        if (p->s[p->p] == '.')
          {
            p->p++;
            s->test = 6;
          }
        if (descendant)
          return mulib_xpath_fail_internal (p,
                                            "'//' requires a child node test");
      }
    else if (p->s[p->p] == '*')
      {
        p->p++;
        s->test = 1;
      }
    else
      {
        if (!mulib_xpath_name_internal (p, &s->name, &s->name_length))
          return 0;
        if (p->s[p->p] == '(')
          {
            p->p++;
            if (p->s[p->p] != ')')
              return mulib_xpath_fail_internal (
                  p, "node test requires empty parentheses");
            p->p++;
            if (s->name_length == 4 && !memcmp (s->name, "node", 4))
              s->test = 2;
            else if (s->name_length == 4 && !memcmp (s->name, "text", 4))
              s->test = 3;
            else if (s->name_length == 7 && !memcmp (s->name, "comment", 7))
              s->test = 4;
            else
              return mulib_xpath_fail_internal (p, "unsupported node test");
          }
      }
    mulib_xpath_skip_internal (p);
    while (p->s[p->p] == '[')
      {
        if (s->count == 8)
          return mulib_xpath_fail_internal (p,
                                            "too many predicates (limit 8)");
        if (!mulib_xpath_pred_parse_internal (p, &s->predicates[s->count++]))
          return 0;
        mulib_xpath_skip_internal (p);
      }
    return 1;
  }
  void
  mulib_xpath_result_free (mulib_xpath_result_t *r)
  {
    if (r)
      {
        free (r->nodes);
        r->nodes = NULL;
        r->count = 0;
      }
  }
  /* All compared nodes belong to the same tree. Ancestors precede descendants.
   */
  static int
  mulib_xpath_before_internal (mulib_xml_node_t *a, mulib_xml_node_t *b)
  {
    mulib_xml_node_t *x = a, *y = b, *n;
    size_t da = 0, db = 0;
    for (n = a; n; n = mulib_xml_parent (n))
      da++;
    for (n = b; n; n = mulib_xml_parent (n))
      db++;
    while (da > db)
      {
        x = mulib_xml_parent (x);
        da--;
      }
    while (db > da)
      {
        y = mulib_xml_parent (y);
        db--;
      }
    if (x == y)
      return x == a;
    while (mulib_xml_parent (x) != mulib_xml_parent (y))
      {
        x = mulib_xml_parent (x);
        y = mulib_xml_parent (y);
      }
    for (n = mulib_xml_first (mulib_xml_parent (x)); n; n = mulib_xml_next (n))
      {
        if (n == x)
          return 1;
        if (n == y)
          return 0;
      }
    return 0;
  }
  static int
  mulib_xpath_push_internal (mulib_xpath_result_t *r, mulib_xml_node_t *n,
                             int ordered)
  {
    mulib_xml_node_t **a;
    size_t i = r->count, j;
    if (ordered)
      {
        for (j = 0; j < r->count; j++)
          {
            if (r->nodes[j] == n)
              return 1;
            if (i == r->count && mulib_xpath_before_internal (n, r->nodes[j]))
              i = j;
          }
      }
    if (r->count >= SIZE_MAX / sizeof (*a))
      return 0;
    a = (mulib_xml_node_t **)realloc (r->nodes, (r->count + 1) * sizeof (*a));
    if (!a)
      return 0;
    r->nodes = a;
    memmove (a + i + 1, a + i, (r->count - i) * sizeof (*a));
    a[i] = n;
    r->count++;
    return 1;
  }
  static int
  mulib_xpath_test_internal (mulib_xml_node_t *n,
                             const mulib_xpath_step_internal *s)
  {
    mulib_xml_type_t t = mulib_xml_type (n);
    if (s->test == 0)
      return mulib_xpath_equal_internal (mulib_xml_name (n), s->name,
                                         s->name_length);
    if (s->test == 1)
      return t == MULIB_XML_ELEMENT;
    if (s->test == 2)
      return 1;
    if (s->test == 3)
      return t == MULIB_XML_TEXT || t == MULIB_XML_CDATA;
    return s->test == 4 && t == MULIB_XML_COMMENT;
  }
  static int
  mulib_xpath_filter_internal (mulib_xml_node_t *n,
                               const mulib_xpath_pred_internal *p)
  {
    const char *v;
    mulib_xml_node_t *c;
    if (p->kind == 2 || p->kind == 3)
      {
        v = mulib_xpath_attr_internal (n, p->name, p->name_length);
        return p->kind == 2
                   ? v != NULL
                   : mulib_xpath_equal_internal (v, p->value, p->value_length);
      }
    for (c = mulib_xml_first (n); c; c = mulib_xml_next (c))
      if ((mulib_xml_type (c) == MULIB_XML_TEXT
           || mulib_xml_type (c) == MULIB_XML_CDATA)
          && mulib_xpath_equal_internal (mulib_xml_value (c), p->value,
                                         p->value_length))
        return 1;
    return 0;
  }
  static int
  mulib_xpath_select_internal (mulib_xml_node_t *context,
                               const mulib_xpath_step_internal *s,
                               mulib_xpath_result_t *out)
  {
    mulib_xpath_result_t temp = { NULL, 0 };
    mulib_xml_node_t *c;
    size_t i, j, k;
    if (s->test == 5 || s->test == 6)
      {
        c = s->test == 5 ? context : mulib_xml_parent (context);
        if (c && !mulib_xpath_push_internal (&temp, c, 0))
          return 0;
      }
    else
      for (c = mulib_xml_first (context); c; c = mulib_xml_next (c))
        if (mulib_xpath_test_internal (c, s)
            && !mulib_xpath_push_internal (&temp, c, 0))
          {
            mulib_xpath_result_free (&temp);
            return 0;
          }
    for (i = 0; i < s->count; i++)
      {
        const mulib_xpath_pred_internal *p = &s->predicates[i];
        if (p->kind == 1)
          {
            if (p->position <= temp.count)
              {
                temp.nodes[0] = temp.nodes[p->position - 1];
                temp.count = 1;
              }
            else
              temp.count = 0;
          }
        else
          {
            k = 0;
            for (j = 0; j < temp.count; j++)
              if (mulib_xpath_filter_internal (temp.nodes[j], p))
                temp.nodes[k++] = temp.nodes[j];
            temp.count = k;
          }
      }
    for (i = 0; i < temp.count; i++)
      if (!mulib_xpath_push_internal (out, temp.nodes[i], 1))
        {
          mulib_xpath_result_free (&temp);
          return 0;
        }
    mulib_xpath_result_free (&temp);
    return 1;
  }
  int
  mulib_xpath_query (mulib_xml_node_t *context, const char *expr,
                     mulib_xpath_result_t *out, mulib_xml_error_t *error)
  {
    mulib_xpath_step_internal steps[64];
    size_t count = 0, i, j;
    int descendant = 0;
    mulib_xpath_parser_internal p;
    mulib_xpath_result_t current = { NULL, 0 }, next = { NULL, 0 };
    if (error)
      {
        error->offset = 0;
        error->message = NULL;
      }
    p.s = expr;
    p.p = 0;
    p.error = error;
    if (out)
      {
        out->nodes = NULL;
        out->count = 0;
      }
    if (!context || !expr || !out)
      return mulib_xpath_fail_internal (&p, "invalid query argument");
    if (!mulib_xml_valid_text (expr, strlen (expr)))
      return mulib_xpath_fail_internal (&p, "invalid query UTF-8");
    mulib_xpath_skip_internal (&p);
    if (expr[p.p] == '/')
      {
        while (mulib_xml_parent (context))
          context = mulib_xml_parent (context);
        if (mulib_xml_type (context) != MULIB_XML_DOCUMENT)
          return mulib_xpath_fail_internal (
              &p, "absolute path requires a document");
        p.p++;
        if (expr[p.p] == '/')
          {
            descendant = 1;
            p.p++;
          }
        mulib_xpath_skip_internal (&p);
        if (!expr[p.p] && descendant)
          return mulib_xpath_fail_internal (&p, "missing descendant step");
      }
    else if (!expr[p.p])
      return mulib_xpath_fail_internal (&p, "empty path");
    while (expr[p.p])
      {
        if (count == 64)
          return mulib_xpath_fail_internal (&p,
                                            "path step limit exceeded (64)");
        if (!mulib_xpath_step_parse_internal (&p, &steps[count++], descendant))
          return 0;
        if (!expr[p.p])
          break;
        if (expr[p.p] != '/')
          return mulib_xpath_fail_internal (&p, "expected '/' or end of path");
        p.p++;
        descendant = 0;
        if (expr[p.p] == '/')
          {
            p.p++;
            descendant = 1;
          }
        mulib_xpath_skip_internal (&p);
        if (!expr[p.p])
          return mulib_xpath_fail_internal (&p, "missing path step");
      }
    if (!mulib_xpath_push_internal (&current, context, 0))
      goto allocation_failure;
    for (i = 0; i < count; i++)
      {
        for (j = 0; j < current.count; j++)
          {
            mulib_xml_node_t *base = current.nodes[j], *node = base;
            do
              {
                if (!mulib_xpath_select_internal (node, &steps[i], &next))
                  goto allocation_failure;
                if (!steps[i].descendant)
                  break;
                if (mulib_xml_first (node))
                  node = mulib_xml_first (node);
                else
                  {
                    while (node != base && !mulib_xml_next (node))
                      node = mulib_xml_parent (node);
                    node = node == base ? NULL : mulib_xml_next (node);
                  }
              }
            while (node);
          }
        mulib_xpath_result_free (&current);
        current = next;
        next.nodes = NULL;
        next.count = 0;
      }
    *out = current;
    return 1;
  allocation_failure:
    mulib_xpath_result_free (&current);
    mulib_xpath_result_free (&next);
    return mulib_xpath_fail_internal (&p, "allocation failed");
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
