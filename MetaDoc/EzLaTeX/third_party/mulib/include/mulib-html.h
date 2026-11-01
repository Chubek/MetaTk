/**
 * @file mulib-html.h
 * @brief Escaped HTML builder and serializer using the XML DOM (not an HTML
 * parser).
 *
 * Lowercase ASCII tag/attribute names, HTML void elements, and doctype output
 * are supported. Raw-text elements are rejected to avoid misleading escaping.
 * Attributes/URLs are not sanitized. Implement with MULIB_IMPL_HTML plus XML,
 * STRBUF and UTF8 switches. See manual/mulib-html.md.
 */
#ifndef MULIB_HTML_H
#define MULIB_HTML_H
#include "mulib-xml.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Create an owned html/head/title/body document. NULL title means
   * empty. */
  mulib_xml_node_t *mulib_html_document (const char *title);
  /** @brief Borrow the body's node from a document made by
   * mulib_html_document. */
  mulib_xml_node_t *mulib_html_body (const mulib_xml_node_t *document);
  /** @brief Append an element, optionally with a text child, atomically.
   * Returns a borrowed child or NULL; parent is unchanged on failure. */
  mulib_xml_node_t *mulib_html_add (mulib_xml_node_t *parent, const char *tag,
                                    const char *text);
  /** @brief Shared XML escaping for ordinary HTML text and quoted attributes.
   * No raw-text contexts; 1 success, 0 invalid UTF-8/XML chars or allocation
   * error. */
  int mulib_html_escape (mulib_strbuf_t *out, const char *text, size_t length,
                         int attribute);
  /** @brief Serialize a DOM node as HTML; returns malloc-owned text or NULL.
   * Documents require an html root and receive <!doctype html>. Void elements
   * cannot have children; script/style and other raw-text tags are rejected.
   * Initial LF in pre/textarea/listing is padded to survive HTML parsing. */
  char *mulib_html_serialize (const mulib_xml_node_t *node, size_t *length);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HTML_H */

#if defined(MULIB_IMPL_HTML) && !defined(MULIB_HTML_IMPLEMENTED)
#define MULIB_HTML_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_html_name_internal (const char *s)
  {
    if (!s || *s < 'a' || *s > 'z')
      return 0;
    for (s++; *s; s++)
      if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '-'
            || *s == '_' || *s == ':'))
        return 0;
    return 1;
  }
  static int
  mulib_html_in_internal (const char *tag, const char *const *names, size_t n)
  {
    size_t i;
    for (i = 0; i < n; i++)
      if (!strcmp (tag, names[i]))
        return 1;
    return 0;
  }
  static int
  mulib_html_void_internal (const char *s)
  {
    static const char *const names[]
        = { "area",  "base", "br",   "col",   "embed",  "hr",    "img",
            "input", "link", "meta", "param", "source", "track", "wbr" };
    return mulib_html_in_internal (s, names,
                                   sizeof (names) / sizeof (names[0]));
  }
  static int
  mulib_html_raw_internal (const char *s)
  {
    static const char *const names[]
        = { "script",  "style",    "xmp",       "iframe",
            "noembed", "noframes", "plaintext", "noscript" };
    return mulib_html_in_internal (s, names,
                                   sizeof (names) / sizeof (names[0]));
  }
  mulib_xml_node_t *
  mulib_html_add (mulib_xml_node_t *parent, const char *tag, const char *text)
  {
    mulib_xml_node_t *n, *t;
    if (!mulib_html_name_internal (tag) || mulib_html_raw_internal (tag)
        || (text && mulib_html_void_internal (tag)))
      return NULL;
    n = mulib_xml_new (MULIB_XML_ELEMENT, tag);
    if (!n)
      return NULL;
    if (text)
      {
        t = mulib_xml_new (MULIB_XML_TEXT, text);
        if (!t)
          {
            mulib_xml_destroy (n);
            return NULL;
          }
        if (!mulib_xml_append (n, t))
          {
            mulib_xml_destroy (t);
            mulib_xml_destroy (n);
            return NULL;
          }
      }
    if (!mulib_xml_append (parent, n))
      {
        mulib_xml_destroy (n);
        return NULL;
      }
    return n;
  }
  mulib_xml_node_t *
  mulib_html_document (const char *title)
  {
    mulib_xml_node_t *doc = mulib_xml_new (MULIB_XML_DOCUMENT, NULL), *html,
                     *head;
    if (!doc)
      return NULL;
    html = mulib_html_add (doc, "html", NULL);
    head = html ? mulib_html_add (html, "head", NULL) : NULL;
    if (!head || !mulib_html_add (head, "title", title ? title : "")
        || !mulib_html_add (html, "body", NULL))
      {
        mulib_xml_destroy (doc);
        return NULL;
      }
    return doc;
  }
  mulib_xml_node_t *
  mulib_html_body (const mulib_xml_node_t *doc)
  {
    mulib_xml_node_t *root = mulib_xml_root (doc), *n;
    if (!root || strcmp (mulib_xml_name (root), "html"))
      return NULL;
    for (n = mulib_xml_first (root); n; n = mulib_xml_next (n))
      if (mulib_xml_name (n) && !strcmp (mulib_xml_name (n), "body"))
        return n;
    return NULL;
  }
  int
  mulib_html_escape (mulib_strbuf_t *out, const char *s, size_t n, int attr)
  {
    return mulib_xml_escape (out, s, n, attr);
  }
  static int
  mulib_html_leading_lf_internal (const mulib_xml_node_t *node)
  {
    mulib_xml_node_t *c;
    for (c = mulib_xml_first (node); c; c = mulib_xml_next (c))
      {
        mulib_xml_type_t type = mulib_xml_type (c);
        const char *value;
        /* Empty text emits nothing; a tag/comment does emit a token before LF.
         */
        if (type != MULIB_XML_TEXT && type != MULIB_XML_CDATA)
          return 0;
        value = mulib_xml_value (c);
        if (*value)
          return *value == '\n';
      }
    return 0;
  }
  static int
  mulib_html_write_internal (mulib_strbuf_t *out, const mulib_xml_node_t *node,
                             size_t depth)
  {
    mulib_xml_type_t type = mulib_xml_type (node);
    mulib_xml_node_t *c;
    const char *name = mulib_xml_name (node);
    size_t i;
    if (type == MULIB_XML_DOCUMENT)
      {
        c = mulib_xml_root (node);
        if (!c || strcmp (mulib_xml_name (c), "html")
            || !mulib_strbuf_append (out, "<!doctype html>\n"))
          return 0;
        for (c = mulib_xml_first (node); c; c = mulib_xml_next (c))
          if (!mulib_html_write_internal (out, c, depth))
            return 0;
        return 1;
      }
    if (type == MULIB_XML_ELEMENT)
      {
        if (depth > MULIB_XML_DEPTH_LIMIT || !mulib_html_name_internal (name)
            || mulib_html_raw_internal (name)
            || (mulib_html_void_internal (name) && mulib_xml_first (node)))
          return 0;
        if (!mulib_strbuf_putc (out, '<') || !mulib_strbuf_append (out, name))
          return 0;
        for (i = 0; i < mulib_xml_attr_count (node); i++)
          {
            const char *key = mulib_xml_attr_name (node, i),
                       *value = mulib_xml_attr_value (node, i);
            if (!mulib_html_name_internal (key)
                || !mulib_strbuf_putc (out, ' ')
                || !mulib_strbuf_append (out, key)
                || !mulib_strbuf_append (out, "=\"")
                || !mulib_html_escape (out, value, strlen (value), 1)
                || !mulib_strbuf_putc (out, '"'))
              return 0;
          }
        if (!mulib_strbuf_putc (out, '>'))
          return 0;
        if (mulib_html_void_internal (name))
          return 1;
        /* HTML discards one LF token immediately after these start tags. */
        if ((!strcmp (name, "pre") || !strcmp (name, "textarea")
             || !strcmp (name, "listing"))
            && mulib_html_leading_lf_internal (node)
            && !mulib_strbuf_putc (out, '\n'))
          return 0;
        for (c = mulib_xml_first (node); c; c = mulib_xml_next (c))
          if (!mulib_html_write_internal (out, c, depth + 1))
            return 0;
        return mulib_strbuf_append (out, "</")
               && mulib_strbuf_append (out, name)
               && mulib_strbuf_putc (out, '>');
      }
    if (type == MULIB_XML_COMMENT)
      {
        /* HTML has additional comment-token hazards not rejected by XML. */
        const char *v = mulib_xml_value (node);
        size_t n = strlen (v);
        if (*v == '>' || !strncmp (v, "->", 2) || strstr (v, "<!-")
            || (n && v[n - 1] == '<'))
          return 0;
        return mulib_strbuf_append (out, "<!--")
               && mulib_strbuf_append (out, v)
               && mulib_strbuf_append (out, "-->");
      }
    return mulib_html_escape (out, mulib_xml_value (node),
                              strlen (mulib_xml_value (node)), 0);
  }
  char *
  mulib_html_serialize (const mulib_xml_node_t *node, size_t *length)
  {
    mulib_strbuf_t out;
    mulib_strbuf_init (&out);
    if (length)
      *length = 0;
    if (!node || !mulib_html_write_internal (&out, node, 1)
        || !mulib_strbuf_append_n (&out, "", 0))
      {
        mulib_strbuf_free (&out);
        return NULL;
      }
    if (length)
      *length = out.length;
    return out.data;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
