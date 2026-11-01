/**
 * @file mulib-graphviz.h
 * @brief Quoted DOT serialization of the existing directed MuLIB graph.
 *
 * Emits every vertex, directed edges (including loops), default attributes,
 * and optional vertex labels. No Graphviz executable/library is needed.
 * Define MULIB_IMPL_GRAPHVIZ plus GRAPH, STRBUF and UTF8 switches in one TU.
 */
#ifndef MULIB_GRAPHVIZ_H
#define MULIB_GRAPHVIZ_H
#include "mulib-graph.h"
#include "mulib-strbuf.h"
#include "mulib-utf8.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Borrowed attribute name/value pair. Both must be non-NULL UTF-8.
   */
  typedef struct mulib_graphviz_attr
  {
    const char *name, *value;
  } mulib_graphviz_attr_t;
  /** @brief Optional borrowed output configuration. Zero initialization is
   * valid. Labels apply to vertices 0..label_count-1; NULL entries omit the
   * label. Arrays/strings need only outlive the serialization call. */
  typedef struct mulib_graphviz_options
  {
    const char *name;
    const mulib_graphviz_attr_t *graph_attributes;
    size_t graph_attribute_count;
    const mulib_graphviz_attr_t *node_attributes;
    size_t node_attribute_count;
    const mulib_graphviz_attr_t *edge_attributes;
    size_t edge_attribute_count;
    const char *const *labels;
    size_t label_count;
  } mulib_graphviz_options_t;
  /** @brief Append a quoted DOT string, escaping quotes, backslashes/newlines.
   * Tabs remain literal (DOT has no tab escape). UTF-8 is required;
   * NUL/controls except tab, CR, LF are rejected.
   * 1 success, 0 failure; allocation failure may leave a partial prefix. */
  int mulib_graphviz_quote (mulib_strbuf_t *out, const char *text,
                            size_t length);
  /** @brief Serialize graph to malloc-owned DOT or NULL on invalid
   * input/allocation. All IDs/attribute values are quoted; labels are literal,
   * never HTML or raw DOT. Vertex IDs are n0, n1, ... . Optional length is
   * zero on failure. */
  char *mulib_graphviz_dot (const mulib_graph_t *graph,
                            const mulib_graphviz_options_t *options,
                            size_t *length);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_GRAPHVIZ_H */

#if defined(MULIB_IMPL_GRAPHVIZ) && !defined(MULIB_GRAPHVIZ_IMPLEMENTED)
#define MULIB_GRAPHVIZ_IMPLEMENTED
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_graphviz_quote (mulib_strbuf_t *out, const char *s, size_t n)
  {
    size_t i, begin = 0;
    if (!out || (!s && n)
        || !mulib_utf8_validate ((const unsigned char *)s, n))
      return 0;
    for (i = 0; i < n; i++)
      if ((unsigned char)s[i] < 32 && s[i] != '\t' && s[i] != '\r'
          && s[i] != '\n')
        return 0;
    for (i = 0; i < n; i++)
      if ((unsigned char)s[i] == 127)
        return 0;
    if (!mulib_strbuf_putc (out, '"'))
      return 0;
    for (i = 0; i < n; i++)
      {
        const char *replace = NULL;
        if (s[i] == '"')
          replace = "\\\"";
        else if (s[i] == '\\')
          replace = "\\\\";
        else if (s[i] == '\n')
          replace = "\\n";
        else if (s[i] == '\r')
          replace = "\\r";
        if (replace)
          {
            if (!mulib_strbuf_append_n (out, s + begin, i - begin)
                || !mulib_strbuf_append (out, replace))
              return 0;
            begin = i + 1;
          }
      }
    return mulib_strbuf_append_n (out, s ? s + begin : NULL, n - begin)
           && mulib_strbuf_putc (out, '"');
  }
  static int
  mulib_graphviz_string_internal (mulib_strbuf_t *out, const char *s)
  {
    return s && mulib_graphviz_quote (out, s, strlen (s));
  }
  static int
  mulib_graphviz_attrs_internal (mulib_strbuf_t *out,
                                 const mulib_graphviz_attr_t *attrs,
                                 size_t count)
  {
    size_t i;
    if (count && !attrs)
      return 0;
    if (!mulib_strbuf_putc (out, '['))
      return 0;
    for (i = 0; i < count; i++)
      {
        if ((i && !mulib_strbuf_append (out, ", ")) || !attrs[i].name
            || !*attrs[i].name
            || !mulib_graphviz_string_internal (out, attrs[i].name)
            || !mulib_strbuf_putc (out, '=')
            || !mulib_graphviz_string_internal (out, attrs[i].value))
          return 0;
      }
    return mulib_strbuf_putc (out, ']');
  }
  static int
  mulib_graphviz_id_internal (mulib_strbuf_t *out, size_t vertex)
  {
    char id[3 * sizeof (size_t) + 4];
    int n = snprintf (id, sizeof (id), "n%zu", vertex);
    return n >= 0 && (size_t)n < sizeof (id)
           && mulib_graphviz_quote (out, id, (size_t)n);
  }
  char *
  mulib_graphviz_dot (const mulib_graph_t *g,
                      const mulib_graphviz_options_t *options, size_t *length)
  {
    mulib_graphviz_options_t empty = { 0 };
    const mulib_graphviz_options_t *o = options ? options : &empty;
    mulib_strbuf_t out;
    size_t i, j;
    if (length)
      *length = 0;
    if (!g || (g->n && (!g->edges || g->n > SIZE_MAX / g->n))
        || o->label_count > g->n || (o->label_count && !o->labels)
        || (o->graph_attribute_count && !o->graph_attributes)
        || (o->node_attribute_count && !o->node_attributes)
        || (o->edge_attribute_count && !o->edge_attributes))
      return NULL;
    mulib_strbuf_init (&out);
    if (!mulib_strbuf_append (&out, "digraph ")
        || !mulib_graphviz_string_internal (&out, o->name ? o->name : "G")
        || !mulib_strbuf_append (&out, " {\n"))
      goto fail;
    if (o->graph_attribute_count
        && (!mulib_strbuf_append (&out, "  graph ")
            || !mulib_graphviz_attrs_internal (&out, o->graph_attributes,
                                               o->graph_attribute_count)
            || !mulib_strbuf_append (&out, ";\n")))
      goto fail;
    if (o->node_attribute_count
        && (!mulib_strbuf_append (&out, "  node ")
            || !mulib_graphviz_attrs_internal (&out, o->node_attributes,
                                               o->node_attribute_count)
            || !mulib_strbuf_append (&out, ";\n")))
      goto fail;
    if (o->edge_attribute_count
        && (!mulib_strbuf_append (&out, "  edge ")
            || !mulib_graphviz_attrs_internal (&out, o->edge_attributes,
                                               o->edge_attribute_count)
            || !mulib_strbuf_append (&out, ";\n")))
      goto fail;
    for (i = 0; i < g->n; i++)
      {
        if (!mulib_strbuf_append (&out, "  ")
            || !mulib_graphviz_id_internal (&out, i))
          goto fail;
        if (i < o->label_count && o->labels[i]
            && (!mulib_strbuf_append (&out, " [\"label\"=")
                || !mulib_graphviz_string_internal (&out, o->labels[i])
                || !mulib_strbuf_putc (&out, ']')))
          goto fail;
        if (!mulib_strbuf_append (&out, ";\n"))
          goto fail;
      }
    for (i = 0; i < g->n; i++)
      for (j = 0; j < g->n; j++)
        if (g->edges[i * g->n + j])
          {
            if (!mulib_strbuf_append (&out, "  ")
                || !mulib_graphviz_id_internal (&out, i)
                || !mulib_strbuf_append (&out, " -> ")
                || !mulib_graphviz_id_internal (&out, j)
                || !mulib_strbuf_append (&out, ";\n"))
              goto fail;
          }
    if (!mulib_strbuf_append (&out, "}\n"))
      goto fail;
    if (length)
      *length = out.length;
    return out.data;
  fail:
    mulib_strbuf_free (&out);
    return NULL;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
