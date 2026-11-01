/**
 * @file mulib-css.h
 * @brief Owned CSS declarations/rules and a focused DOM selector matcher.
 *
 * Values are preserved, not interpreted. No at-rules, cascade, layout,
 * fetching or CSS escapes in selectors. Matching supports types, *, #id,
 * .class, [attr], [attr=value], descendant/child combinators and comma groups.
 * Define MULIB_IMPL_CSS plus XML, STRBUF and UTF8 switches. See the manual.
 */
#ifndef MULIB_CSS_H
#define MULIB_CSS_H
#include "mulib-xml.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Opaque owned declaration list. */
  typedef struct mulib_css_style mulib_css_style_t;
  /** @brief Opaque owned stylesheet containing ordered rules. */
  typedef struct mulib_css_sheet mulib_css_sheet_t;
  /** @brief Parse declarations (without braces). Returns owned list or NULL.
   * Handles comments, quoted/escaped strings, balanced ()/[], and !important.
   */
  mulib_css_style_t *mulib_css_parse_declarations (const char *source,
                                                   size_t length,
                                                   mulib_xml_error_t *error);
  /** @brief Release declarations. NULL is harmless. */
  void mulib_css_style_destroy (mulib_css_style_t *style);
  /** @brief Number of declarations, including duplicate properties. */
  size_t mulib_css_count (const mulib_css_style_t *style);
  /** @brief Borrow indexed property name/value (NULL out of range). */
  const char *mulib_css_name (const mulib_css_style_t *style, size_t index);
  /** @brief Borrow an indexed value, excluding !important. */
  const char *mulib_css_value (const mulib_css_style_t *style, size_t index);
  /** @brief Whether an indexed declaration is important; zero out of range. */
  int mulib_css_important (const mulib_css_style_t *style, size_t index);
  /** @brief Borrow the last important value, else last ordinary value, for a
   * name. Ordinary names compare ASCII case-insensitively; --custom names are
   * exact. */
  const char *mulib_css_get (const mulib_css_style_t *style, const char *name);
  /** @brief Parse ordinary selector { declarations } rules; owned result or
   * NULL. */
  mulib_css_sheet_t *mulib_css_parse (const char *source, size_t length,
                                      mulib_xml_error_t *error);
  /** @brief Release a sheet and its declaration lists. NULL is harmless. */
  void mulib_css_destroy (mulib_css_sheet_t *sheet);
  /** @brief Rule count. */
  size_t mulib_css_rule_count (const mulib_css_sheet_t *sheet);
  /** @brief Borrow an indexed rule selector (NULL out of range). */
  const char *mulib_css_selector (const mulib_css_sheet_t *sheet,
                                  size_t index);
  /** @brief Borrow indexed rule declarations (NULL out of range). */
  const mulib_css_style_t *
  mulib_css_declarations (const mulib_css_sheet_t *sheet, size_t index);
  /** @brief Match a selector: 1 matches, 0 no match, -1 invalid
   * syntax/allocation. NULL node validates syntax and returns 0. All comma
   * branches are validated. Maximum 64 compound selectors in an expression;
   * comparisons are case-sensitive. */
  int mulib_css_matches (const mulib_xml_node_t *node, const char *selector);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CSS_H */

#if defined(MULIB_IMPL_CSS) && !defined(MULIB_CSS_IMPLEMENTED)
#define MULIB_CSS_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_css_decl_internal
  {
    char *name, *value;
    int important;
  } mulib_css_decl_internal;
  struct mulib_css_style
  {
    mulib_css_decl_internal *items;
    size_t count;
  };
  typedef struct mulib_css_rule_internal
  {
    char *selector;
    mulib_css_style_t *style;
  } mulib_css_rule_internal;
  struct mulib_css_sheet
  {
    mulib_css_rule_internal *rules;
    size_t count;
  };
  static int
  mulib_css_space_internal (char c)
  {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
  }
  static char
  mulib_css_lower_internal (char c)
  {
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
  }
  static int
  mulib_css_equal_internal (const char *a, const char *b)
  {
    if (a[0] == '-' && a[1] == '-')
      return !strcmp (a, b);
    while (*a && *b)
      if (mulib_css_lower_internal (*a++) != mulib_css_lower_internal (*b++))
        return 0;
    return *a == *b;
  }
  static int
  mulib_css_error_internal (mulib_xml_error_t *e, size_t pos, const char *s)
  {
    if (e)
      {
        e->offset = pos;
        e->message = s;
      }
    return 0;
  }
  static char *
  mulib_css_copy_internal (const char *s, size_t n)
  {
    char *r;
    if (n == SIZE_MAX)
      return NULL;
    r = (char *)malloc (n + 1);
    if (r)
      {
        if (n)
          memcpy (r, s, n);
        r[n] = 0;
      }
    return r;
  }
  static int
  mulib_css_identchar_internal (char c, int first)
  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
           || c == '-' || (!first && c >= '0' && c <= '9');
  }
  static int
  mulib_css_ident_internal (const char *s, size_t *p, size_t *begin, size_t *n)
  {
    *begin = *p;
    if (!mulib_css_identchar_internal (s[*p], 1))
      return 0;
    (*p)++;
    while (mulib_css_identchar_internal (s[*p], 0))
      (*p)++;
    *n = *p - *begin;
    return 1;
  }
  static int
  mulib_css_span_equal_internal (const char *s, const char *pattern, size_t n)
  {
    return s && strlen (s) == n && !memcmp (s, pattern, n);
  }
  static const char *
  mulib_css_attr_internal (const mulib_xml_node_t *node, const char *name,
                           size_t n)
  {
    size_t i;
    for (i = 0; i < mulib_xml_attr_count (node); i++)
      if (mulib_css_span_equal_internal (mulib_xml_attr_name (node, i), name,
                                         n))
        return mulib_xml_attr_value (node, i);
    return NULL;
  }
  static int
  mulib_css_class_internal (const char *classes, const char *name, size_t n)
  {
    if (!classes)
      return 0;
    while (*classes)
      {
        const char *start;
        while (mulib_css_space_internal (*classes))
          classes++;
        start = classes;
        while (*classes && !mulib_css_space_internal (*classes))
          classes++;
        if ((size_t)(classes - start) == n && !memcmp (start, name, n))
          return 1;
      }
    return 0;
  }
  /* Parse/validate a compound, also testing it if node is supplied. */
  static int
  mulib_css_compound_internal (const mulib_xml_node_t *node, const char *s,
                               size_t *p, int *matches)
  {
    size_t begin, n, start = *p;
    int match = mulib_xml_type (node) == MULIB_XML_ELEMENT;
    if (s[*p] == '*')
      (*p)++;
    else if (mulib_css_identchar_internal (s[*p], 1))
      {
        if (!mulib_css_ident_internal (s, p, &begin, &n))
          return 0;
        if (!mulib_css_span_equal_internal (mulib_xml_name (node), s + begin,
                                            n))
          match = 0;
      }
    for (;;)
      {
        char c = s[*p];
        if (c == '#' || c == '.')
          {
            (*p)++;
            if (!mulib_css_ident_internal (s, p, &begin, &n))
              return 0;
            if (c == '#')
              {
                if (!mulib_css_span_equal_internal (
                        mulib_xml_attr (node, "id"), s + begin, n))
                  match = 0;
              }
            else if (!mulib_css_class_internal (mulib_xml_attr (node, "class"),
                                                s + begin, n))
              match = 0;
          }
        else if (c == '[')
          {
            const char *v;
            (*p)++;
            while (mulib_css_space_internal (s[*p]))
              (*p)++;
            if (!mulib_css_ident_internal (s, p, &begin, &n))
              return 0;
            v = mulib_css_attr_internal (node, s + begin, n);
            while (mulib_css_space_internal (s[*p]))
              (*p)++;
            if (s[*p] == '=')
              {
                (*p)++;
                while (mulib_css_space_internal (s[*p]))
                  (*p)++;
                if (s[*p] == '\'' || s[*p] == '"')
                  {
                    char q = s[(*p)++];
                    begin = *p;
                    while (s[*p] && s[*p] != q)
                      {
                        if (s[*p] == '\\' || s[*p] == '\n' || s[*p] == '\r')
                          return 0;
                        (*p)++;
                      }
                    if (!s[*p])
                      return 0;
                    n = *p - begin;
                    (*p)++;
                  }
                else if (!mulib_css_ident_internal (s, p, &begin, &n))
                  return 0;
                if (!mulib_css_span_equal_internal (v, s + begin, n))
                  match = 0;
                while (mulib_css_space_internal (s[*p]))
                  (*p)++;
              }
            else if (!v)
              match = 0;
            if (s[*p] != ']')
              return 0;
            (*p)++;
          }
        else
          break;
      }
    *matches = match;
    return *p > start;
  }
  typedef struct mulib_css_step_internal
  {
    size_t begin;
    char relation;
  } mulib_css_step_internal;
  int
  mulib_css_matches (const mulib_xml_node_t *node, const char *s)
  {
    mulib_css_step_internal steps[64];
    size_t count = 0, p = 0, depth = 0, i, j, start, end;
    char relation = 0;
    const mulib_xml_node_t *a;
    unsigned char *rows, *previous, *current;
    int ignored, matched = 0;
    if (!s || !mulib_xml_valid_text (s, strlen (s)))
      return -1;
    while (mulib_css_space_internal (s[p]))
      p++;
    if (!s[p])
      return -1;
    for (;;)
      {
        size_t before;
        if (count == 64)
          return -1;
        steps[count].begin = p;
        steps[count++].relation = relation;
        if (!mulib_css_compound_internal (NULL, s, &p, &ignored))
          return -1;
        before = p;
        while (mulib_css_space_internal (s[p]))
          p++;
        if (!s[p])
          break;
        if (s[p] == ',' || s[p] == '>')
          {
            relation = s[p] == ',' ? 0 : '>';
            p++;
            while (mulib_css_space_internal (s[p]))
              p++;
            if (!s[p])
              return -1;
          }
        else if (p > before)
          relation = ' ';
        else
          return -1;
      }
    if (!node || mulib_xml_type (node) != MULIB_XML_ELEMENT)
      return 0;
    for (a = node; a; a = mulib_xml_parent (a))
      {
        if (depth == SIZE_MAX / 2)
          return -1;
        depth++;
      }
    rows = (unsigned char *)calloc (depth, 2);
    if (!rows)
      return -1;
    previous = rows;
    current = rows + depth;
    for (start = 0; start < count; start = end)
      {
        int test;
        end = start + 1;
        while (end < count && steps[end].relation)
          end++;
        memset (previous, 0, depth);
        p = steps[end - 1].begin;
        if (!mulib_css_compound_internal (node, s, &p, &test))
          {
            free (rows);
            return -1;
          }
        previous[0] = (unsigned char)test;
        for (i = end - 1; i > start; i--)
          {
            int seen = 0;
            for (a = node, j = 0; a; a = mulib_xml_parent (a), j++)
              {
                p = steps[i - 1].begin;
                if (!mulib_css_compound_internal (a, s, &p, &test))
                  {
                    free (rows);
                    return -1;
                  }
                current[j] = (unsigned char)(test && j
                                             && (steps[i].relation == '>'
                                                     ? previous[j - 1]
                                                     : seen));
                seen = seen || previous[j];
              }
            {
              unsigned char *swap = previous;
              previous = current;
              current = swap;
            }
          }
        for (j = 0; j < depth; j++)
          if (previous[j])
            matched = 1;
      }
    free (rows);
    return matched;
  }
  /* Replace comments by spaces, preserving source offsets and quoted content.
   */
  static char *
  mulib_css_clean_internal (const char *s, size_t n, mulib_xml_error_t *e)
  {
    char *r;
    size_t i = 0;
    char quote = 0;
    if (!mulib_xml_valid_text (s, n))
      {
        mulib_css_error_internal (e, 0, "invalid UTF-8/CSS character");
        return NULL;
      }
    r = mulib_css_copy_internal (s, n);
    if (!r)
      {
        mulib_css_error_internal (e, 0, "allocation failed");
        return NULL;
      }
    while (i < n)
      {
        if (quote)
          {
            if (r[i] == '\\')
              {
                if (i + 1 < n)
                  i += 2;
                else
                  {
                    free (r);
                    mulib_css_error_internal (e, i, "trailing escape");
                    return NULL;
                  }
              }
            else if (r[i] == quote)
              {
                quote = 0;
                i++;
              }
            else if (r[i] == '\n' || r[i] == '\r')
              {
                free (r);
                mulib_css_error_internal (e, i, "newline in string");
                return NULL;
              }
            else
              i++;
          }
        else if (r[i] == '\'' || r[i] == '"')
          quote = r[i++];
        else if (r[i] == '\\')
          {
            if (i + 1 < n)
              i += 2;
            else
              {
                free (r);
                mulib_css_error_internal (e, i, "trailing escape");
                return NULL;
              }
          }
        else if (i + 1 < n && r[i] == '/' && r[i + 1] == '*')
          {
            size_t begin = i;
            r[i++] = ' ';
            r[i++] = ' ';
            while (i + 1 < n && !(r[i] == '*' && r[i + 1] == '/'))
              {
                if (!mulib_css_space_internal (r[i]))
                  r[i] = ' ';
                i++;
              }
            if (i + 1 >= n)
              {
                free (r);
                mulib_css_error_internal (e, begin, "unterminated comment");
                return NULL;
              }
            r[i++] = ' ';
            r[i++] = ' ';
          }
        else
          i++;
      }
    if (quote)
      {
        free (r);
        mulib_css_error_internal (e, n, "unterminated string");
        return NULL;
      }
    return r;
  }
  static int
  mulib_css_boundary_internal (const char *s, size_t n, size_t start,
                               const char *stops, size_t *end,
                               mulib_xml_error_t *e)
  {
    char stack[64], quote = 0;
    size_t depth = 0, i;
    for (i = start; i < n; i++)
      {
        char c = s[i];
        if (quote)
          {
            if (c == '\\')
              {
                if (i + 1 == n)
                  return mulib_css_error_internal (e, i, "trailing escape");
                i++;
              }
            else if (c == quote)
              quote = 0;
            continue;
          }
        if (c == '\\')
          {
            if (i + 1 == n)
              return mulib_css_error_internal (e, i, "trailing escape");
            i++;
            continue;
          }
        if (c == '\'' || c == '"')
          {
            quote = c;
            continue;
          }
        if (!depth && strchr (stops, c))
          {
            *end = i;
            return 1;
          }
        if (c == '(' || c == '[')
          {
            if (depth == 64)
              return mulib_css_error_internal (e, i, "CSS nesting limit (64)");
            stack[depth++] = c;
          }
        else if (c == ')' || c == ']')
          {
            if (!depth || stack[depth - 1] != (c == ')' ? '(' : '['))
              return mulib_css_error_internal (e, i, "unbalanced delimiter");
            depth--;
          }
        else if (c == '{' || c == '}')
          return mulib_css_error_internal (e, i,
                                           "unexpected brace or nested rule");
      }
    if (quote || depth)
      return mulib_css_error_internal (e, n, "unterminated string/delimiter");
    *end = n;
    return 1;
  }
  void
  mulib_css_style_destroy (mulib_css_style_t *s)
  {
    size_t i;
    if (!s)
      return;
    for (i = 0; i < s->count; i++)
      {
        free (s->items[i].name);
        free (s->items[i].value);
      }
    free (s->items);
    free (s);
  }
  mulib_css_style_t *
  mulib_css_parse_declarations (const char *source, size_t n,
                                mulib_xml_error_t *e)
  {
    char *s;
    mulib_css_style_t *style;
    size_t p = 0;
    if (e)
      {
        e->offset = 0;
        e->message = NULL;
      }
    s = mulib_css_clean_internal (source, n, e);
    if (!s)
      return NULL;
    style = (mulib_css_style_t *)calloc (1, sizeof (*style));
    if (!style)
      {
        free (s);
        mulib_css_error_internal (e, 0, "allocation failed");
        return NULL;
      }
    while (p < n)
      {
        size_t begin, end, colon, k, value_end;
        int important = 0;
        char *name, *value;
        mulib_css_decl_internal *items;
        while (p < n && (mulib_css_space_internal (s[p]) || s[p] == ';'))
          p++;
        if (p == n)
          break;
        begin = p;
        if (!mulib_css_boundary_internal (s, n, p, ";", &end, e))
          goto fail;
        p = end < n ? end + 1 : end;
        value_end = end;
        while (end > begin && mulib_css_space_internal (s[end - 1]))
          end--;
        colon = begin;
        while (colon < end && s[colon] != ':')
          colon++;
        if (colon == end)
          {
            mulib_css_error_internal (e, begin, "expected property ':'");
            goto fail;
          }
        k = colon;
        while (k > begin && mulib_css_space_internal (s[k - 1]))
          k--;
        if (k == begin || !mulib_css_identchar_internal (s[begin], 1))
          {
            mulib_css_error_internal (e, begin, "invalid property name");
            goto fail;
          }
        {
          size_t j;
          for (j = begin + 1; j < k; j++)
            if (!mulib_css_identchar_internal (s[j], 0))
              {
                mulib_css_error_internal (e, j, "invalid property name");
                goto fail;
              }
        }
        name = mulib_css_copy_internal (s + begin, k - begin);
        if (!name)
          {
            mulib_css_error_internal (e, begin, "allocation failed");
            goto fail;
          }
        if (!(name[0] == '-' && name[1] == '-'))
          {
            for (k = 0; name[k]; k++)
              name[k] = mulib_css_lower_internal (name[k]);
          }
        begin = colon + 1;
        while (begin < end && mulib_css_space_internal (s[begin]))
          begin++;
        if (end - begin >= 9)
          {
            size_t word = end - 9, j;
            int same = 1;
            for (j = 0; j < 9; j++)
              if (mulib_css_lower_internal (s[word + j]) != "important"[j])
                same = 0;
            if (same)
              {
                k = word;
                while (k > begin && mulib_css_space_internal (s[k - 1]))
                  k--;
                if (k > begin && s[k - 1] == '!')
                  {
                    size_t slash = k - 1;
                    while (slash > begin && s[slash - 1] == '\\')
                      slash--;
                    if (((k - 1 - slash) & 1u) == 0)
                      {
                        important = 1;
                        end = k - 1;
                        while (end > begin
                               && mulib_css_space_internal (s[end - 1]))
                          end--;
                      }
                  }
              }
          }
        if (begin == end)
          {
            free (name);
            mulib_css_error_internal (e, begin, "empty property value");
            goto fail;
          }
        value = mulib_css_copy_internal (s + begin, end - begin);
        if (!value || style->count >= SIZE_MAX / sizeof (*items))
          {
            free (name);
            free (value);
            mulib_css_error_internal (e, value_end, "allocation/size failure");
            goto fail;
          }
        items = (mulib_css_decl_internal *)realloc (
            style->items, (style->count + 1) * sizeof (*items));
        if (!items)
          {
            free (name);
            free (value);
            mulib_css_error_internal (e, value_end, "allocation failed");
            goto fail;
          }
        style->items = items;
        items[style->count].name = name;
        items[style->count].value = value;
        items[style->count].important = important;
        style->count++;
      }
    free (s);
    return style;
  fail:
    free (s);
    mulib_css_style_destroy (style);
    return NULL;
  }
  size_t
  mulib_css_count (const mulib_css_style_t *s)
  {
    return s ? s->count : 0;
  }
  const char *
  mulib_css_name (const mulib_css_style_t *s, size_t i)
  {
    return s && i < s->count ? s->items[i].name : NULL;
  }
  const char *
  mulib_css_value (const mulib_css_style_t *s, size_t i)
  {
    return s && i < s->count ? s->items[i].value : NULL;
  }
  int
  mulib_css_important (const mulib_css_style_t *s, size_t i)
  {
    return s && i < s->count ? s->items[i].important : 0;
  }
  const char *
  mulib_css_get (const mulib_css_style_t *s, const char *name)
  {
    size_t i;
    const char *value = NULL;
    int important = 0;
    if (!s || !name)
      return NULL;
    for (i = 0; i < s->count; i++)
      if (mulib_css_equal_internal (s->items[i].name, name)
          && (!important || s->items[i].important))
        {
          value = s->items[i].value;
          important = s->items[i].important;
        }
    return value;
  }
  void
  mulib_css_destroy (mulib_css_sheet_t *s)
  {
    size_t i;
    if (!s)
      return;
    for (i = 0; i < s->count; i++)
      {
        free (s->rules[i].selector);
        mulib_css_style_destroy (s->rules[i].style);
      }
    free (s->rules);
    free (s);
  }
  mulib_css_sheet_t *
  mulib_css_parse (const char *source, size_t n, mulib_xml_error_t *e)
  {
    char *s;
    size_t p = 0;
    mulib_css_sheet_t *sheet;
    if (e)
      {
        e->offset = 0;
        e->message = NULL;
      }
    s = mulib_css_clean_internal (source, n, e);
    if (!s)
      return NULL;
    sheet = (mulib_css_sheet_t *)calloc (1, sizeof (*sheet));
    if (!sheet)
      {
        free (s);
        mulib_css_error_internal (e, 0, "allocation failed");
        return NULL;
      }
    while (p < n)
      {
        size_t begin, end, body, close;
        char *selector;
        mulib_css_style_t *style;
        mulib_css_rule_internal *rules;
        while (p < n && mulib_css_space_internal (s[p]))
          p++;
        if (p == n)
          break;
        begin = p;
        if (s[p] == '@')
          {
            mulib_css_error_internal (e, p, "at-rules are unsupported");
            goto fail;
          }
        if (!mulib_css_boundary_internal (s, n, p, "{", &end, e))
          goto fail;
        if (end == n)
          {
            mulib_css_error_internal (e, p, "expected rule '{'");
            goto fail;
          }
        body = end + 1;
        while (end > begin && mulib_css_space_internal (s[end - 1]))
          end--;
        selector = mulib_css_copy_internal (s + begin, end - begin);
        if (!selector)
          {
            mulib_css_error_internal (e, begin, "allocation failed");
            goto fail;
          }
        if (mulib_css_matches (NULL, selector) < 0)
          {
            free (selector);
            mulib_css_error_internal (e, begin,
                                      "unsupported or invalid selector");
            goto fail;
          }
        if (!mulib_css_boundary_internal (s, n, body, "}", &close, e))
          {
            free (selector);
            goto fail;
          }
        if (close == n)
          {
            free (selector);
            mulib_css_error_internal (e, n, "unclosed rule");
            goto fail;
          }
        style = mulib_css_parse_declarations (s + body, close - body, e);
        if (!style)
          {
            free (selector);
            if (e)
              e->offset += body;
            goto fail;
          }
        if (sheet->count >= SIZE_MAX / sizeof (*rules))
          {
            free (selector);
            mulib_css_style_destroy (style);
            mulib_css_error_internal (e, p, "rule count overflow");
            goto fail;
          }
        rules = (mulib_css_rule_internal *)realloc (
            sheet->rules, (sheet->count + 1) * sizeof (*rules));
        if (!rules)
          {
            free (selector);
            mulib_css_style_destroy (style);
            mulib_css_error_internal (e, p, "allocation failed");
            goto fail;
          }
        sheet->rules = rules;
        rules[sheet->count].selector = selector;
        rules[sheet->count].style = style;
        sheet->count++;
        p = close + 1;
      }
    free (s);
    return sheet;
  fail:
    free (s);
    mulib_css_destroy (sheet);
    return NULL;
  }
  size_t
  mulib_css_rule_count (const mulib_css_sheet_t *s)
  {
    return s ? s->count : 0;
  }
  const char *
  mulib_css_selector (const mulib_css_sheet_t *s, size_t i)
  {
    return s && i < s->count ? s->rules[i].selector : NULL;
  }
  const mulib_css_style_t *
  mulib_css_declarations (const mulib_css_sheet_t *s, size_t i)
  {
    return s && i < s->count ? s->rules[i].style : NULL;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
