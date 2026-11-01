/**
 * @file mulib-xml.h
 * @brief Owned UTF-8 XML DOM, bounded parser, and escaped serializer.
 *
 * Supports elements, attributes, text, comments, CDATA and the five predefined
 * and numeric entities. DTDs, external entities and processing instructions
 * other than an initial XML 1.0 declaration are rejected. Names use an ASCII
 * subset; namespaces are preserved as names, not resolved. See the manual.
 * Define MULIB_IMPL_XML, MULIB_IMPL_STRBUF and MULIB_IMPL_UTF8 in one TU.
 */
#ifndef MULIB_XML_H
#define MULIB_XML_H
#include "mulib-strbuf.h"
#include "mulib-utf8.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Node kind. Document nodes own at most one element root. */
  typedef enum mulib_xml_type
  {
    MULIB_XML_DOCUMENT,
    MULIB_XML_ELEMENT,
    MULIB_XML_TEXT,
    MULIB_XML_COMMENT,
    MULIB_XML_CDATA
  } mulib_xml_type_t;
  /** @brief Opaque, individually allocated node; children/attributes are
   * owned. */
  typedef struct mulib_xml_node mulib_xml_node_t;
  /** @brief Failure position in input bytes and a static, non-owned message.
   */
  typedef struct mulib_xml_error
  {
    size_t offset;
    const char *message;
  } mulib_xml_error_t;
/** @brief Parser/serializer nesting limit, including the root element. */
#define MULIB_XML_DEPTH_LIMIT 128

  /** @brief Test the supported ASCII XML name grammar. NULL is invalid. */
  int mulib_xml_valid_name (const char *name);
  /** @brief Validate UTF-8 and XML 1.0 character ranges; embedded NUL is
   * invalid. */
  int mulib_xml_valid_text (const char *text, size_t length);
  /** @brief Allocate a node. Value is a name for elements, content otherwise;
   * documents require NULL or an empty string. Copies input, returns NULL on
   * error. */
  mulib_xml_node_t *mulib_xml_new (mulib_xml_type_t type, const char *value);
  /** @brief Detach and free a subtree, iteratively. NULL is harmless. */
  void mulib_xml_destroy (mulib_xml_node_t *node);
  /** @brief Detach without freeing; ownership returns to the caller. */
  mulib_xml_node_t *mulib_xml_detach (mulib_xml_node_t *node);
  /** @brief Append an unattached node; transfers ownership only on success.
   * Rejects cycles, document children, and invalid document-level content. */
  int mulib_xml_append (mulib_xml_node_t *parent, mulib_xml_node_t *child);
  /** @brief Return node kind; NULL returns MULIB_XML_DOCUMENT. */
  mulib_xml_type_t mulib_xml_type (const mulib_xml_node_t *node);
  /** @brief Borrow an element name, or NULL for other node kinds. */
  const char *mulib_xml_name (const mulib_xml_node_t *node);
  /** @brief Borrow text/comment/CDATA content, or NULL for containers. */
  const char *mulib_xml_value (const mulib_xml_node_t *node);
  /** @brief Borrow a parent, first child, or next sibling (NULL at the
   * boundary). */
  mulib_xml_node_t *mulib_xml_parent (const mulib_xml_node_t *node);
  /** @brief Borrow the first child. */
  mulib_xml_node_t *mulib_xml_first (const mulib_xml_node_t *node);
  /** @brief Borrow the next sibling. */
  mulib_xml_node_t *mulib_xml_next (const mulib_xml_node_t *node);
  /** @brief Borrow a document's element root; returns NULL otherwise. */
  mulib_xml_node_t *mulib_xml_root (const mulib_xml_node_t *document);
  /** @brief Copy/set an element attribute transactionally; 1 success, 0
   * failure. */
  int mulib_xml_set_attr (mulib_xml_node_t *node, const char *name,
                          const char *value);
  /** @brief Borrow an attribute value, or NULL when absent. */
  const char *mulib_xml_attr (const mulib_xml_node_t *node, const char *name);
  /** @brief Attribute count (zero for non-elements). */
  size_t mulib_xml_attr_count (const mulib_xml_node_t *node);
  /** @brief Borrow attribute name/value by insertion-order index, NULL out of
   * range. */
  const char *mulib_xml_attr_name (const mulib_xml_node_t *node, size_t index);
  /** @brief Borrow an indexed attribute value. */
  const char *mulib_xml_attr_value (const mulib_xml_node_t *node,
                                    size_t index);
  /** @brief Parse exactly length bytes into an owned document; NULL on
   * failure. No I/O is performed. On error, error (if non-NULL) receives byte
   * position/message. */
  mulib_xml_node_t *mulib_xml_parse (const char *source, size_t length,
                                     mulib_xml_error_t *error);
  /** @brief Append escaped, validated text/attribute content to a strbuf.
   * Attribute mode also escapes quotes/tabs/newlines. Failure may leave a
   * prefix. */
  int mulib_xml_escape (mulib_strbuf_t *out, const char *text, size_t length,
                        int attribute);
  /** @brief Serialize a node/subtree to malloc-owned NUL-terminated UTF-8.
   * Documents require one root; depth overflow or allocation failure returns
   * NULL. Optional length excludes the terminator and is set to zero on
   * failure. */
  char *mulib_xml_serialize (const mulib_xml_node_t *node, size_t *length);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_XML_H */

#if defined(MULIB_IMPL_XML) && !defined(MULIB_XML_IMPLEMENTED)
#define MULIB_XML_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_xml_attribute_internal
  {
    char *name, *value;
  } mulib_xml_attribute_internal;
  struct mulib_xml_node
  {
    mulib_xml_type_t type;
    char *value;
    struct mulib_xml_node *parent, *first, *last, *next, *previous;
    mulib_xml_attribute_internal *attributes;
    size_t attribute_count;
  };
  static int
  mulib_xml_space_internal (char c)
  {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
  }
  static int
  mulib_xml_start_internal (char c)
  {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'
           || c == ':';
  }
  static int
  mulib_xml_namechar_internal (char c)
  {
    return mulib_xml_start_internal (c) || (c >= '0' && c <= '9') || c == '-'
           || c == '.';
  }
  int
  mulib_xml_valid_name (const char *s)
  {
    if (!s || !mulib_xml_start_internal (*s++))
      return 0;
    while (*s)
      if (!mulib_xml_namechar_internal (*s++))
        return 0;
    return 1;
  }
  static int
  mulib_xml_scalar_internal (uint32_t c)
  {
    return c == 9 || c == 10 || c == 13 || (c >= 0x20 && c <= 0xd7ff)
           || (c >= 0xe000 && c <= 0xfffd) || (c >= 0x10000 && c <= 0x10ffff);
  }
  int
  mulib_xml_valid_text (const char *s, size_t n)
  {
    if (!s && n)
      return 0;
    while (n)
      {
        uint32_t c;
        size_t used;
        if (!mulib_utf8_decode ((const unsigned char *)s, n, &c, &used)
            || !mulib_xml_scalar_internal (c))
          return 0;
        s += used;
        n -= used;
      }
    return 1;
  }
  static char *
  mulib_xml_copy_internal (const char *s, size_t n)
  {
    char *p;
    if (n == SIZE_MAX)
      return NULL;
    p = (char *)malloc (n + 1);
    if (p)
      {
        if (n)
          memcpy (p, s, n);
        p[n] = 0;
      }
    return p;
  }
  mulib_xml_node_t *
  mulib_xml_new (mulib_xml_type_t type, const char *value)
  {
    mulib_xml_node_t *n;
    size_t len = value ? strlen (value) : 0;
    if (type < MULIB_XML_DOCUMENT || type > MULIB_XML_CDATA)
      return NULL;
    if (type == MULIB_XML_DOCUMENT)
      {
        if (len)
          return NULL;
      }
    else if (!value
             || (type == MULIB_XML_ELEMENT
                     ? !mulib_xml_valid_name (value)
                     : !mulib_xml_valid_text (value, len)))
      return NULL;
    if (type == MULIB_XML_COMMENT
        && (strstr (value, "--") || (len && value[len - 1] == '-')))
      return NULL;
    if (type == MULIB_XML_CDATA && strstr (value, "]]>"))
      return NULL;
    n = (mulib_xml_node_t *)calloc (1, sizeof (*n));
    if (!n)
      return NULL;
    n->type = type;
    if (type != MULIB_XML_DOCUMENT
        && !(n->value = mulib_xml_copy_internal (value, len)))
      {
        free (n);
        return NULL;
      }
    return n;
  }
  mulib_xml_node_t *
  mulib_xml_detach (mulib_xml_node_t *n)
  {
    if (n && n->parent)
      {
        if (n->previous)
          n->previous->next = n->next;
        else
          n->parent->first = n->next;
        if (n->next)
          n->next->previous = n->previous;
        else
          n->parent->last = n->previous;
        n->parent = NULL;
        n->next = NULL;
        n->previous = NULL;
      }
    return n;
  }
  void
  mulib_xml_destroy (mulib_xml_node_t *n)
  {
    mulib_xml_detach (n);
    while (n)
      {
        if (n->first)
          {
            mulib_xml_node_t *child = n->first;
            n->first = child->next;
            n = child;
          }
        else
          {
            mulib_xml_node_t *parent = n->parent;
            size_t i;
            for (i = 0; i < n->attribute_count; i++)
              {
                free (n->attributes[i].name);
                free (n->attributes[i].value);
              }
            free (n->attributes);
            free (n->value);
            free (n);
            n = parent;
          }
      }
  }
  int
  mulib_xml_append (mulib_xml_node_t *p, mulib_xml_node_t *c)
  {
    mulib_xml_node_t *a;
    if (!p || !c || c->parent || c->type == MULIB_XML_DOCUMENT
        || (p->type != MULIB_XML_ELEMENT && p->type != MULIB_XML_DOCUMENT))
      return 0;
    for (a = p; a; a = a->parent)
      if (a == c)
        return 0;
    if (p->type == MULIB_XML_DOCUMENT)
      {
        if (c->type == MULIB_XML_ELEMENT)
          {
            if (mulib_xml_root (p))
              return 0;
          }
        else if (c->type == MULIB_XML_TEXT)
          {
            const char *s = c->value;
            while (*s)
              if (!mulib_xml_space_internal (*s++))
                return 0;
          }
        else if (c->type != MULIB_XML_COMMENT)
          return 0;
      }
    c->parent = p;
    c->previous = p->last;
    if (p->last)
      p->last->next = c;
    else
      p->first = c;
    p->last = c;
    return 1;
  }
  mulib_xml_type_t
  mulib_xml_type (const mulib_xml_node_t *n)
  {
    return n ? n->type : MULIB_XML_DOCUMENT;
  }
  const char *
  mulib_xml_name (const mulib_xml_node_t *n)
  {
    return n && n->type == MULIB_XML_ELEMENT ? n->value : NULL;
  }
  const char *
  mulib_xml_value (const mulib_xml_node_t *n)
  {
    return n && n->type >= MULIB_XML_TEXT ? n->value : NULL;
  }
  mulib_xml_node_t *
  mulib_xml_parent (const mulib_xml_node_t *n)
  {
    return n ? n->parent : NULL;
  }
  mulib_xml_node_t *
  mulib_xml_first (const mulib_xml_node_t *n)
  {
    return n ? n->first : NULL;
  }
  mulib_xml_node_t *
  mulib_xml_next (const mulib_xml_node_t *n)
  {
    return n ? n->next : NULL;
  }
  mulib_xml_node_t *
  mulib_xml_root (const mulib_xml_node_t *n)
  {
    mulib_xml_node_t *c;
    if (!n || n->type != MULIB_XML_DOCUMENT)
      return NULL;
    for (c = n->first; c; c = c->next)
      if (c->type == MULIB_XML_ELEMENT)
        return c;
    return NULL;
  }
  int
  mulib_xml_set_attr (mulib_xml_node_t *n, const char *name, const char *value)
  {
    size_t i;
    char *v, *k;
    mulib_xml_attribute_internal *a;
    if (!n || n->type != MULIB_XML_ELEMENT || !mulib_xml_valid_name (name)
        || !value || !mulib_xml_valid_text (value, strlen (value)))
      return 0;
    v = mulib_xml_copy_internal (value, strlen (value));
    if (!v)
      return 0;
    for (i = 0; i < n->attribute_count; i++)
      if (!strcmp (n->attributes[i].name, name))
        {
          free (n->attributes[i].value);
          n->attributes[i].value = v;
          return 1;
        }
    if (n->attribute_count >= SIZE_MAX / sizeof (*a))
      {
        free (v);
        return 0;
      }
    k = mulib_xml_copy_internal (name, strlen (name));
    if (!k)
      {
        free (v);
        return 0;
      }
    a = (mulib_xml_attribute_internal *)realloc (
        n->attributes, (n->attribute_count + 1) * sizeof (*a));
    if (!a)
      {
        free (k);
        free (v);
        return 0;
      }
    n->attributes = a;
    a[n->attribute_count].name = k;
    a[n->attribute_count].value = v;
    n->attribute_count++;
    return 1;
  }
  const char *
  mulib_xml_attr (const mulib_xml_node_t *n, const char *name)
  {
    size_t i;
    if (!n || !name)
      return NULL;
    for (i = 0; i < n->attribute_count; i++)
      if (!strcmp (n->attributes[i].name, name))
        return n->attributes[i].value;
    return NULL;
  }
  size_t
  mulib_xml_attr_count (const mulib_xml_node_t *n)
  {
    return n ? n->attribute_count : 0;
  }
  const char *
  mulib_xml_attr_name (const mulib_xml_node_t *n, size_t i)
  {
    return n && i < n->attribute_count ? n->attributes[i].name : NULL;
  }
  const char *
  mulib_xml_attr_value (const mulib_xml_node_t *n, size_t i)
  {
    return n && i < n->attribute_count ? n->attributes[i].value : NULL;
  }
  int
  mulib_xml_escape (mulib_strbuf_t *out, const char *s, size_t n, int attr)
  {
    size_t i, begin = 0;
    if (!out || !mulib_xml_valid_text (s, n))
      return 0;
    for (i = 0; i < n; i++)
      {
        const char *rep = NULL;
        switch (s[i])
          {
          case '&':
            rep = "&amp;";
            break;
          case '<':
            rep = "&lt;";
            break;
          case '>':
            rep = "&gt;";
            break;
          case '"':
            if (attr)
              rep = "&quot;";
            break;
          case '\'':
            if (attr)
              rep = "&apos;";
            break;
          case '\r':
            rep = "&#13;";
            break;
          case '\n':
            if (attr)
              rep = "&#10;";
            break;
          case '\t':
            if (attr)
              rep = "&#9;";
            break;
          default:
            break;
          }
        if (rep)
          {
            if (!mulib_strbuf_append_n (out, s + begin, i - begin)
                || !mulib_strbuf_append (out, rep))
              return 0;
            begin = i + 1;
          }
      }
    return mulib_strbuf_append_n (out, s ? s + begin : NULL, n - begin);
  }

  typedef struct mulib_xml_parser_internal
  {
    const char *s;
    size_t n, pos;
    mulib_xml_error_t *error;
  } mulib_xml_parser_internal;
  static int
  mulib_xml_fail_internal (mulib_xml_parser_internal *p, const char *message)
  {
    if (p->error && !p->error->message)
      {
        p->error->offset = p->pos;
        p->error->message = message;
      }
    return 0;
  }
  static int
  mulib_xml_has_internal (mulib_xml_parser_internal *p, const char *s)
  {
    size_t n = strlen (s);
    return n <= p->n - p->pos && !memcmp (p->s + p->pos, s, n);
  }
  static void
  mulib_xml_skip_internal (mulib_xml_parser_internal *p)
  {
    while (p->pos < p->n && mulib_xml_space_internal (p->s[p->pos]))
      p->pos++;
  }
  static char *
  mulib_xml_parse_name_internal (mulib_xml_parser_internal *p)
  {
    size_t begin = p->pos;
    if (begin == p->n || !mulib_xml_start_internal (p->s[begin]))
      {
        mulib_xml_fail_internal (p, "expected ASCII XML name");
        return NULL;
      }
    p->pos++;
    while (p->pos < p->n && mulib_xml_namechar_internal (p->s[p->pos]))
      p->pos++;
    {
      char *s = mulib_xml_copy_internal (p->s + begin, p->pos - begin);
      if (!s)
        mulib_xml_fail_internal (p, "allocation failed");
      return s;
    }
  }
  static int
  mulib_xml_entity_internal (mulib_xml_parser_internal *p, mulib_strbuf_t *b)
  {
    size_t start = ++p->pos, end = start;
    uint32_t cp = 0;
    unsigned char encoded[4];
    size_t used;
    while (end < p->n && p->s[end] != ';')
      end++;
    if (end == p->n || p->s[end] != ';')
      return mulib_xml_fail_internal (p, "unterminated entity");
    if (end > start && p->s[start] == '#')
      {
        unsigned base = 10;
        size_t i = start + 1;
        if (i < end && p->s[i] == 'x')
          {
            base = 16;
            i++;
          }
        if (i == end)
          return mulib_xml_fail_internal (p, "empty numeric entity");
        for (; i < end; i++)
          {
            unsigned d;
            unsigned char c = (unsigned char)p->s[i];
            if (c >= '0' && c <= '9')
              d = c - '0';
            else if (c >= 'a' && c <= 'f')
              d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
              d = c - 'A' + 10;
            else
              return mulib_xml_fail_internal (p, "invalid numeric entity");
            if (d >= base || cp > (0x10ffffu - d) / base)
              return mulib_xml_fail_internal (p,
                                              "numeric entity out of range");
            cp = cp * base + d;
          }
        if (!mulib_xml_scalar_internal (cp))
          return mulib_xml_fail_internal (p, "invalid XML character entity");
      }
    else
      {
        size_t n = end - start;
        const char *s = p->s + start;
        if (n == 3 && !memcmp (s, "amp", 3))
          cp = '&';
        else if (n == 2 && !memcmp (s, "lt", 2))
          cp = '<';
        else if (n == 2 && !memcmp (s, "gt", 2))
          cp = '>';
        else if (n == 4 && !memcmp (s, "quot", 4))
          cp = '"';
        else if (n == 4 && !memcmp (s, "apos", 4))
          cp = '\'';
        else
          return mulib_xml_fail_internal (
              p, "unknown entity (DTD entities are unsupported)");
      }
    p->pos = end + 1;
    used = mulib_utf8_encode (cp, encoded, sizeof (encoded));
    return mulib_strbuf_append_n (b, (const char *)encoded, used)
           || mulib_xml_fail_internal (p, "allocation failed");
  }
  /* Decode content until a quote or '<'; raw newline normalization is XML 1.0.
   */
  static char *
  mulib_xml_content_internal (mulib_xml_parser_internal *p, char quote)
  {
    mulib_strbuf_t b;
    mulib_strbuf_init (&b);
    while (p->pos < p->n && p->s[p->pos] != (quote ? quote : '<'))
      {
        char c = p->s[p->pos];
        if (quote && c == '<')
          {
            mulib_xml_fail_internal (p, "unescaped '<' in attribute");
            goto fail;
          }
        if (!quote && mulib_xml_has_internal (p, "]]>"))
          {
            mulib_xml_fail_internal (p, "']]>' outside CDATA");
            goto fail;
          }
        if (c == '&')
          {
            if (!mulib_xml_entity_internal (p, &b))
              goto fail;
            continue;
          }
        p->pos++;
        if (c == '\r')
          {
            if (p->pos < p->n && p->s[p->pos] == '\n')
              p->pos++;
            c = quote ? ' ' : '\n';
          }
        else if (quote && (c == '\n' || c == '\t'))
          c = ' ';
        if (!mulib_strbuf_putc (&b, c))
          {
            mulib_xml_fail_internal (p, "allocation failed");
            goto fail;
          }
      }
    if (!mulib_strbuf_append_n (&b, "", 0))
      {
        mulib_xml_fail_internal (p, "allocation failed");
        goto fail;
      }
    return b.data;
  fail:
    mulib_strbuf_free (&b);
    return NULL;
  }
  static mulib_xml_node_t *
  mulib_xml_special_internal (mulib_xml_parser_internal *p, int cdata)
  {
    const char *end = cdata ? "]]>" : "-->";
    size_t begin;
    char *value;
    mulib_xml_node_t *n;
    mulib_strbuf_t b;
    p->pos += cdata ? 9 : 4;
    begin = p->pos;
    while (p->pos < p->n && !mulib_xml_has_internal (p, end))
      p->pos++;
    if (p->pos == p->n)
      {
        mulib_xml_fail_internal (p, "unterminated comment or CDATA");
        return NULL;
      }
    mulib_strbuf_init (&b);
    while (begin < p->pos)
      {
        char c = p->s[begin++];
        if (c == '\r')
          {
            if (begin < p->pos && p->s[begin] == '\n')
              begin++;
            c = '\n';
          }
        if (!mulib_strbuf_putc (&b, c))
          {
            mulib_strbuf_free (&b);
            mulib_xml_fail_internal (p, "allocation failed");
            return NULL;
          }
      }
    if (!mulib_strbuf_append_n (&b, "", 0))
      {
        mulib_strbuf_free (&b);
        mulib_xml_fail_internal (p, "allocation failed");
        return NULL;
      }
    value = b.data;
    n = mulib_xml_new (cdata ? MULIB_XML_CDATA : MULIB_XML_COMMENT, value);
    free (value);
    if (!n)
      {
        mulib_xml_fail_internal (p,
                                 "invalid comment/CDATA or allocation failed");
        return NULL;
      }
    p->pos += 3;
    return n;
  }
  static mulib_xml_node_t *
  mulib_xml_element_internal (mulib_xml_parser_internal *p, size_t depth)
  {
    mulib_xml_node_t *n = NULL;
    char *name = NULL;
    if (depth > MULIB_XML_DEPTH_LIMIT)
      {
        mulib_xml_fail_internal (p, "XML nesting limit exceeded");
        return NULL;
      }
    p->pos++;
    name = mulib_xml_parse_name_internal (p);
    if (!name)
      return NULL;
    n = mulib_xml_new (MULIB_XML_ELEMENT, name);
    free (name);
    if (!n)
      {
        mulib_xml_fail_internal (p, "allocation failed");
        return NULL;
      }
    for (;;)
      {
        size_t before = p->pos;
        char *key, *value;
        char quote;
        mulib_xml_skip_internal (p);
        if (mulib_xml_has_internal (p, "/>"))
          {
            p->pos += 2;
            return n;
          }
        if (mulib_xml_has_internal (p, ">"))
          {
            p->pos++;
            break;
          }
        if (before == p->pos)
          {
            mulib_xml_fail_internal (p,
                                     "expected whitespace before attribute");
            goto fail;
          }
        key = mulib_xml_parse_name_internal (p);
        if (!key)
          goto fail;
        if (mulib_xml_attr (n, key))
          {
            free (key);
            mulib_xml_fail_internal (p, "duplicate attribute");
            goto fail;
          }
        mulib_xml_skip_internal (p);
        if (p->pos == p->n || p->s[p->pos++] != '=')
          {
            free (key);
            mulib_xml_fail_internal (p, "expected '='");
            goto fail;
          }
        mulib_xml_skip_internal (p);
        if (p->pos == p->n || ((quote = p->s[p->pos]) != '\'' && quote != '"'))
          {
            free (key);
            mulib_xml_fail_internal (p, "expected quoted attribute");
            goto fail;
          }
        p->pos++;
        value = mulib_xml_content_internal (p, quote);
        if (!value)
          {
            free (key);
            goto fail;
          }
        if (p->pos == p->n)
          {
            free (key);
            free (value);
            mulib_xml_fail_internal (p, "unterminated attribute");
            goto fail;
          }
        p->pos++;
        if (!mulib_xml_set_attr (n, key, value))
          {
            free (key);
            free (value);
            mulib_xml_fail_internal (p, "allocation failed");
            goto fail;
          }
        free (key);
        free (value);
      }
    for (;;)
      {
        mulib_xml_node_t *child;
        if (p->pos == p->n)
          {
            mulib_xml_fail_internal (p, "unclosed element");
            goto fail;
          }
        if (mulib_xml_has_internal (p, "</"))
          {
            p->pos += 2;
            name = mulib_xml_parse_name_internal (p);
            if (!name)
              goto fail;
            if (strcmp (name, n->value))
              {
                free (name);
                mulib_xml_fail_internal (p, "mismatched closing tag");
                goto fail;
              }
            free (name);
            mulib_xml_skip_internal (p);
            if (!mulib_xml_has_internal (p, ">"))
              {
                mulib_xml_fail_internal (p, "expected closing '>'");
                goto fail;
              }
            p->pos++;
            return n;
          }
        if (mulib_xml_has_internal (p, "<!--"))
          child = mulib_xml_special_internal (p, 0);
        else if (mulib_xml_has_internal (p, "<![CDATA["))
          child = mulib_xml_special_internal (p, 1);
        else if (mulib_xml_has_internal (p, "<!")
                 || mulib_xml_has_internal (p, "<?"))
          {
            mulib_xml_fail_internal (
                p, "DTD/processing instructions are unsupported");
            goto fail;
          }
        else if (p->s[p->pos] == '<')
          child = mulib_xml_element_internal (p, depth + 1);
        else
          {
            char *s = mulib_xml_content_internal (p, 0);
            if (!s)
              goto fail;
            child = mulib_xml_new (MULIB_XML_TEXT, s);
            free (s);
            if (!child)
              mulib_xml_fail_internal (p, "allocation failed");
          }
        if (!child)
          goto fail;
        if (!mulib_xml_append (n, child))
          {
            mulib_xml_destroy (child);
            mulib_xml_fail_internal (p, "invalid child");
            goto fail;
          }
      }
  fail:
    mulib_xml_destroy (n);
    return NULL;
  }
  static int
  mulib_xml_declaration_internal (mulib_xml_parser_internal *p)
  {
    unsigned fields = 0;
    p->pos += 5;
    for (;;)
      {
        size_t before = p->pos;
        char *key, *value;
        char q;
        mulib_xml_skip_internal (p);
        if (mulib_xml_has_internal (p, "?>"))
          {
            if (!(fields & 1u))
              return mulib_xml_fail_internal (
                  p, "XML declaration requires version 1.0");
            p->pos += 2;
            return 1;
          }
        if (before == p->pos)
          return mulib_xml_fail_internal (p,
                                          "expected declaration whitespace");
        key = mulib_xml_parse_name_internal (p);
        if (!key)
          return 0;
        mulib_xml_skip_internal (p);
        if (p->pos == p->n || p->s[p->pos++] != '=')
          {
            free (key);
            return mulib_xml_fail_internal (p, "expected declaration '='");
          }
        mulib_xml_skip_internal (p);
        if (p->pos == p->n || ((q = p->s[p->pos]) != '\'' && q != '"'))
          {
            free (key);
            return mulib_xml_fail_internal (p, "expected declaration quote");
          }
        before = ++p->pos;
        while (p->pos < p->n && p->s[p->pos] != q)
          p->pos++;
        if (p->pos == p->n)
          {
            free (key);
            return mulib_xml_fail_internal (p, "unterminated declaration");
          }
        value = mulib_xml_copy_internal (p->s + before, p->pos - before);
        p->pos++;
        if (!value)
          {
            free (key);
            return mulib_xml_fail_internal (p, "allocation failed");
          }
        if (!strcmp (key, "version") && !fields && !strcmp (value, "1.0"))
          fields |= 1u;
        else if (!strcmp (key, "encoding") && fields == 1u
                 && (!strcmp (value, "UTF-8") || !strcmp (value, "utf-8")))
          fields |= 2u;
        else if (!strcmp (key, "standalone") && (fields == 1u || fields == 3u)
                 && (!strcmp (value, "yes") || !strcmp (value, "no")))
          fields |= 4u;
        else
          {
            free (key);
            free (value);
            return mulib_xml_fail_internal (
                p, "unsupported or malformed XML declaration");
          }
        free (key);
        free (value);
      }
  }
  mulib_xml_node_t *
  mulib_xml_parse (const char *s, size_t n, mulib_xml_error_t *error)
  {
    mulib_xml_parser_internal p;
    mulib_xml_node_t *doc;
    if (error)
      {
        error->offset = 0;
        error->message = NULL;
      }
    p.s = s;
    p.n = n;
    p.pos = 0;
    p.error = error;
    if (!mulib_xml_valid_text (s, n))
      {
        mulib_xml_fail_internal (&p, "invalid UTF-8/XML character");
        return NULL;
      }
    doc = mulib_xml_new (MULIB_XML_DOCUMENT, NULL);
    if (!doc)
      {
        mulib_xml_fail_internal (&p, "allocation failed");
        return NULL;
      }
    if (n >= 3 && !memcmp (s, "\xef\xbb\xbf", 3))
      p.pos = 3;
    if (mulib_xml_has_internal (&p, "<?xml"))
      {
        if (!mulib_xml_declaration_internal (&p))
          goto fail;
      }
    while (p.pos < n)
      {
        mulib_xml_node_t *child;
        if (mulib_xml_has_internal (&p, "<!--"))
          child = mulib_xml_special_internal (&p, 0);
        else if (mulib_xml_has_internal (&p, "<!")
                 || mulib_xml_has_internal (&p, "<?")
                 || mulib_xml_has_internal (&p, "</"))
          {
            mulib_xml_fail_internal (
                &p, "DTD/processing instruction or unexpected closing tag");
            goto fail;
          }
        else if (s[p.pos] == '<')
          child = mulib_xml_element_internal (&p, 1);
        else
          {
            size_t start = p.pos, finish, saved;
            char *text;
            while (p.pos < n && mulib_xml_space_internal (s[p.pos]))
              p.pos++;
            if (start == p.pos)
              {
                mulib_xml_fail_internal (&p, "non-whitespace outside root");
                goto fail;
              }
            /* Decode only literal whitespace: references are not legal here.
             */
            finish = p.pos;
            saved = p.n;
            p.n = finish;
            p.pos = start;
            text = mulib_xml_content_internal (&p, 0);
            p.n = saved;
            if (!text)
              goto fail;
            child = mulib_xml_new (MULIB_XML_TEXT, text);
            free (text);
            if (!child)
              mulib_xml_fail_internal (&p, "allocation failed");
          }
        if (!child)
          goto fail;
        if (!mulib_xml_append (doc, child))
          {
            mulib_xml_destroy (child);
            mulib_xml_fail_internal (
                &p, "multiple roots or invalid document content");
            goto fail;
          }
      }
    if (!mulib_xml_root (doc))
      {
        mulib_xml_fail_internal (&p, "document has no root element");
        goto fail;
      }
    return doc;
  fail:
    mulib_xml_destroy (doc);
    return NULL;
  }
  static int
  mulib_xml_write_internal (mulib_strbuf_t *b, const mulib_xml_node_t *n,
                            size_t depth)
  {
    const mulib_xml_node_t *c;
    size_t i;
    if (n->type == MULIB_XML_DOCUMENT)
      {
        if (!mulib_xml_root (n))
          return 0;
        for (c = n->first; c; c = c->next)
          {
            /* append() permits only whitespace text here. Character references
             * are not legal outside the root, including a reference to CR. */
            if (c->type == MULIB_XML_TEXT)
              {
                if (!mulib_strbuf_append (b, c->value))
                  return 0;
              }
            else if (!mulib_xml_write_internal (b, c, depth))
              return 0;
          }
        return 1;
      }
    if (n->type == MULIB_XML_ELEMENT)
      {
        if (depth > MULIB_XML_DEPTH_LIMIT || !mulib_strbuf_putc (b, '<')
            || !mulib_strbuf_append (b, n->value))
          return 0;
        for (i = 0; i < n->attribute_count; i++)
          {
            if (!mulib_strbuf_putc (b, ' ')
                || !mulib_strbuf_append (b, n->attributes[i].name)
                || !mulib_strbuf_append (b, "=\"")
                || !mulib_xml_escape (b, n->attributes[i].value,
                                      strlen (n->attributes[i].value), 1)
                || !mulib_strbuf_putc (b, '"'))
              return 0;
          }
        if (!n->first)
          return mulib_strbuf_append (b, "/>");
        if (!mulib_strbuf_putc (b, '>'))
          return 0;
        for (c = n->first; c; c = c->next)
          if (!mulib_xml_write_internal (b, c, depth + 1))
            return 0;
        return mulib_strbuf_append (b, "</")
               && mulib_strbuf_append (b, n->value)
               && mulib_strbuf_putc (b, '>');
      }
    if (n->type == MULIB_XML_TEXT)
      return mulib_xml_escape (b, n->value, strlen (n->value), 0);
    return mulib_strbuf_append (b, n->type == MULIB_XML_COMMENT ? "<!--"
                                                                : "<![CDATA[")
           && mulib_strbuf_append (b, n->value)
           && mulib_strbuf_append (b, n->type == MULIB_XML_COMMENT ? "-->"
                                                                   : "]]>");
  }
  char *
  mulib_xml_serialize (const mulib_xml_node_t *n, size_t *length)
  {
    mulib_strbuf_t b;
    mulib_strbuf_init (&b);
    if (length)
      *length = 0;
    if (!n || !mulib_xml_write_internal (&b, n, 1)
        || !mulib_strbuf_append_n (&b, "", 0))
      {
        mulib_strbuf_free (&b);
        return NULL;
      }
    if (length)
      *length = b.length;
    return b.data;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
