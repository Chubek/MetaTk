/**
 * @file mulib-template.h
 * @brief Length-delimited, JSON-context templates with escaped interpolation.
 * Define MULIB_IMPL_TEMPLATE once; dependencies: JSON, STRBUF, UTF8 (and
 * JSON's PARSEC dependency). See manual/mulib-template.md for the supported
 * grammar.
 */
#ifndef MULIB_TEMPLATE_H
#define MULIB_TEMPLATE_H
#include "mulib-json.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Diagnostic locations are byte offsets and one-based lines/columns.
   */
  typedef struct mulib_template_error
  {
    size_t offset, line, column;
    const char *message;
  } mulib_template_error_t;

  /**
   * @brief Render into malloc-owned, NUL-terminated text, or return NULL.
   * Context/source are borrowed and never modified. output_length, when
   * non-NULL, receives the byte length, including any embedded NULs but not
   * the terminator. On failure it is zero and error contains a static
   * diagnostic. All branches are syntax-checked, including branches that will
   * not execute. NULL context behaves as JSON null. NULL source is allowed
   * only with length zero.
   */
  char *mulib_template_render (const char *source, size_t length,
                               const mulib_json_t *context,
                               size_t *output_length,
                               mulib_template_error_t *error);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TEMPLATE_H */

#if defined(MULIB_IMPL_TEMPLATE) && !defined(MULIB_TEMPLATE_IMPLEMENTED)
#define MULIB_TEMPLATE_IMPLEMENTED
#include "mulib-strbuf.h"
#include "mulib-utf8.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  enum mulib_template_kind_
  {
    MULIB_TEMPLATE_TEXT_,
    MULIB_TEMPLATE_VALUE_,
    MULIB_TEMPLATE_RAW_,
    MULIB_TEMPLATE_IF_,
    MULIB_TEMPLATE_EACH_,
    MULIB_TEMPLATE_ELSE_,
    MULIB_TEMPLATE_END_
  };
  typedef struct mulib_template_token_
  {
    int kind;
    size_t offset, start, length, end, alternate;
  } mulib_template_token_;
  typedef struct mulib_template_state_
  {
    const char *source;
    size_t length, count, capacity;
    mulib_template_token_ *tokens;
    mulib_template_error_t *error;
    mulib_strbuf_t output;
  } mulib_template_state_;
  typedef struct mulib_template_frame_
  {
    const mulib_json_t *value, *root;
    const struct mulib_template_frame_ *parent;
    size_t index;
    const char *key;
    size_t key_length;
    int loop;
  } mulib_template_frame_;
  typedef struct mulib_template_lookup_
  {
    const mulib_json_t *value;
    const char *key;
    size_t length, index;
    int special;
  } mulib_template_lookup_;

  static int
  mulib_template_fail_ (mulib_template_state_ *s, size_t at,
                        const char *message)
  {
    if (s->error)
      {
        size_t i;
        s->error->offset = at;
        s->error->line = s->error->column = 1;
        for (i = 0; i < at && i < s->length; ++i)
          {
            if (s->source[i] == '\n')
              {
                ++s->error->line;
                s->error->column = 1;
              }
            else
              ++s->error->column;
          }
        s->error->message = message;
      }
    return 0;
  }
  static int
  mulib_template_space_ (char c)
  {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
  }
  static int
  mulib_template_word_ (const char *p, size_t n, const char *word)
  {
    return n == strlen (word) && memcmp (p, word, n) == 0;
  }
  static int
  mulib_template_path_ (const char *p, size_t n)
  {
    size_t i = 0, start;
    while (n - i >= 3 && memcmp (p + i, "../", 3) == 0)
      i += 3;
    if (i == n)
      return 0;
    if (n - i == 1 && p[i] == '.')
      return 1;
    if (p[i] == '@')
      {
        start = i++;
        while (i < n && p[i] != '.')
          ++i;
        if (!mulib_template_word_ (p + start, i - start, "@root"))
          return i == n
                 && (mulib_template_word_ (p + start, i - start, "@index")
                     || mulib_template_word_ (p + start, i - start, "@key"));
        if (i == n)
          return 1;
        if (++i == n)
          return 0;
      }
    while (i < n)
      {
        start = i;
        while (i < n && p[i] != '.')
          {
            unsigned char c = (unsigned char)p[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                  || (c >= '0' && c <= '9') || c == '_' || c == '-'
                  || c >= 128))
              return 0;
            ++i;
          }
        if (i == start)
          return 0;
        if (i < n && ++i == n)
          return 0;
      }
    return 1;
  }
  static int
  mulib_template_push_ (mulib_template_state_ *s, mulib_template_token_ t)
  {
    if (s->count == s->capacity)
      {
        size_t cap = s->capacity ? s->capacity * 2 : 32;
        mulib_template_token_ *p;
        if (cap < s->capacity || cap > SIZE_MAX / sizeof (*p))
          return mulib_template_fail_ (s, t.offset, "template is too large");
        p = (mulib_template_token_ *)realloc (s->tokens, cap * sizeof (*p));
        if (!p)
          return mulib_template_fail_ (s, t.offset, "out of memory");
        s->tokens = p;
        s->capacity = cap;
      }
    s->tokens[s->count++] = t;
    return 1;
  }
  static int
  mulib_template_compile_ (mulib_template_state_ *s)
  {
    size_t pos = 0, text = 0, depth = 0, stack[128];
    while (pos < s->length)
      {
        mulib_template_token_ t;
        size_t start, end, close, opener;
        int raw;
        if (s->source[pos] != '{' || s->length - pos < 2
            || s->source[pos + 1] != '{')
          {
            ++pos;
            continue;
          }
        memset (&t, 0, sizeof (t));
        if (pos > text)
          {
            t.kind = MULIB_TEMPLATE_TEXT_;
            t.start = t.offset = text;
            t.length = pos - text;
            if (!mulib_template_push_ (s, t))
              return 0;
          }
        opener = pos;
        raw = s->length - pos >= 3 && s->source[pos + 2] == '{';
        close = raw ? 3 : 2;
        start = pos + close;
        end = start;
        while (end < s->length)
          {
            if (s->source[end] == '}' && s->length - end >= close
                && s->source[end + 1] == '}'
                && (!raw || s->source[end + 2] == '}'))
              break;
            ++end;
          }
        if (end == s->length)
          return mulib_template_fail_ (s, opener, "unclosed template tag");
        pos = end + close;
        text = pos;
        while (start < end && mulib_template_space_ (s->source[start]))
          ++start;
        while (end > start && mulib_template_space_ (s->source[end - 1]))
          --end;
        if (start == end)
          return mulib_template_fail_ (s, opener, "empty template tag");
        memset (&t, 0, sizeof (t));
        t.offset = opener;
        t.start = start;
        t.length = end - start;
        t.end = t.alternate = SIZE_MAX;
        if (!raw && s->source[start] == '!')
          continue;
        if (!raw
            && mulib_template_word_ (s->source + start, end - start, "else"))
          {
            size_t block;
            if (!depth)
              return mulib_template_fail_ (s, opener, "else outside a block");
            block = stack[depth - 1];
            if (s->tokens[block].alternate != SIZE_MAX)
              return mulib_template_fail_ (s, opener, "duplicate else");
            s->tokens[block].alternate = s->count + 1;
            t.kind = MULIB_TEMPLATE_ELSE_;
          }
        else if (!raw && s->source[start] == '/')
          {
            size_t block;
            int expected;
            if (!depth)
              return mulib_template_fail_ (s, opener,
                                           "closing tag outside a block");
            block = stack[depth - 1];
            expected = s->tokens[block].kind;
            if (!((expected == MULIB_TEMPLATE_IF_
                   && mulib_template_word_ (s->source + start, end - start,
                                            "/if"))
                  || (expected == MULIB_TEMPLATE_EACH_
                      && mulib_template_word_ (s->source + start, end - start,
                                               "/each"))))
              return mulib_template_fail_ (s, opener,
                                           "mismatched closing tag");
            --depth;
            s->tokens[block].end = s->count;
            t.kind = MULIB_TEMPLATE_END_;
          }
        else
          {
            t.kind = raw ? MULIB_TEMPLATE_RAW_ : MULIB_TEMPLATE_VALUE_;
            if (!raw && s->source[start] == '#')
              {
                size_t word = start;
                while (start < end
                       && !mulib_template_space_ (s->source[start]))
                  ++start;
                if (mulib_template_word_ (s->source + word, start - word,
                                          "#if"))
                  t.kind = MULIB_TEMPLATE_IF_;
                else if (mulib_template_word_ (s->source + word, start - word,
                                               "#each"))
                  t.kind = MULIB_TEMPLATE_EACH_;
                else
                  return mulib_template_fail_ (s, opener, "unknown block tag");
                while (start < end && mulib_template_space_ (s->source[start]))
                  ++start;
                t.start = start;
                t.length = end - start;
              }
            if (!mulib_template_path_ (s->source + t.start, t.length))
              return mulib_template_fail_ (s, opener, "invalid context path");
            if (t.kind == MULIB_TEMPLATE_IF_ || t.kind == MULIB_TEMPLATE_EACH_)
              {
                if (depth == sizeof (stack) / sizeof (stack[0]))
                  return mulib_template_fail_ (s, opener,
                                               "template nesting exceeds 128");
                stack[depth++] = s->count;
              }
          }
        if (!mulib_template_push_ (s, t))
          return 0;
      }
    if (depth)
      return mulib_template_fail_ (s, s->tokens[stack[depth - 1]].offset,
                                   "unclosed block");
    if (text < s->length)
      {
        mulib_template_token_ t;
        memset (&t, 0, sizeof (t));
        t.kind = MULIB_TEMPLATE_TEXT_;
        t.start = t.offset = text;
        t.length = s->length - text;
        if (!mulib_template_push_ (s, t))
          return 0;
      }
    return 1;
  }
  static mulib_template_lookup_
  mulib_template_lookup_path_ (const char *p, size_t n,
                               const mulib_template_frame_ *f)
  {
    mulib_template_lookup_ result;
    size_t i = 0;
    memset (&result, 0, sizeof (result));
    while (n - i >= 3 && memcmp (p + i, "../", 3) == 0)
      {
        if (!f->parent)
          return result;
        f = f->parent;
        i += 3;
      }
    result.value = f->value;
    if (n - i == 1 && p[i] == '.')
      return result;
    if (n - i >= 5 && memcmp (p + i, "@root", 5) == 0
        && (n - i == 5 || p[i + 5] == '.'))
      {
        result.value = f->root;
        i += 5;
        if (i < n)
          ++i;
      }
    else if (mulib_template_word_ (p + i, n - i, "@index"))
      {
        result.value = NULL;
        if (f->loop)
          {
            result.special = 1;
            result.index = f->index;
          }
        return result;
      }
    else if (mulib_template_word_ (p + i, n - i, "@key"))
      {
        result.value = NULL;
        if (f->loop && f->key)
          {
            result.special = 2;
            result.key = f->key;
            result.length = f->key_length;
          }
        return result;
      }
    else if (n - i >= 4 && memcmp (p + i, "this", 4) == 0
             && (n - i == 4 || p[i + 4] == '.'))
      {
        i += 4;
        if (i < n)
          ++i;
      }
    while (i < n && result.value)
      {
        size_t start = i;
        while (i < n && p[i] != '.')
          ++i;
        if (mulib_json_typeof (result.value) == MULIB_JSON_ARRAY)
          {
            size_t j, index = 0;
            for (j = start; j < i; ++j)
              {
                unsigned char c = (unsigned char)p[j];
                if (c < '0' || c > '9'
                    || index > (SIZE_MAX - (size_t)(c - '0')) / 10)
                  {
                    result.value = NULL;
                    return result;
                  }
                index = index * 10 + (size_t)(c - '0');
              }
            result.value = mulib_json_at (result.value, index);
          }
        else
          result.value = mulib_json_get_n (result.value, p + start, i - start);
        if (i < n)
          ++i;
      }
    return result;
  }
  static int
  mulib_template_truth_ (mulib_template_lookup_ v)
  {
    if (v.special == 1)
      return v.index != 0;
    if (v.special == 2)
      return v.length != 0;
    switch (mulib_json_typeof (v.value))
      {
      case MULIB_JSON_NULL:
        return 0;
      case MULIB_JSON_BOOL:
        return mulib_json_boolean (v.value);
      case MULIB_JSON_NUMBER:
        return mulib_json_numeric (v.value) != 0;
      case MULIB_JSON_STRING:
        {
          size_t n;
          (void)mulib_json_text (v.value, &n);
          return n != 0;
        }
      default:
        return mulib_json_size (v.value) != 0;
      }
  }
  static int
  mulib_template_bytes_ (mulib_template_state_ *s, const char *p, size_t n,
                         int raw)
  {
    size_t i, start = 0;
    if (raw)
      return mulib_strbuf_append_n (&s->output, p, n);
    for (i = 0; i < n; ++i)
      {
        const char *escape = NULL;
        switch (p[i])
          {
          case '&':
            escape = "&amp;";
            break;
          case '<':
            escape = "&lt;";
            break;
          case '>':
            escape = "&gt;";
            break;
          case '"':
            escape = "&quot;";
            break;
          case '\'':
            escape = "&#39;";
            break;
          default:
            break;
          }
        if (escape)
          {
            if (!mulib_strbuf_append_n (&s->output, p + start, i - start)
                || !mulib_strbuf_append (&s->output, escape))
              return 0;
            start = i + 1;
          }
      }
    return mulib_strbuf_append_n (&s->output, p ? p + start : NULL, n - start);
  }
  static int
  mulib_template_emit_ (mulib_template_state_ *s, mulib_template_lookup_ v,
                        int raw)
  {
    const char *p;
    char *owned = NULL;
    size_t n;
    int ok;
    if (v.special == 2)
      return mulib_template_bytes_ (s, v.key, v.length, raw);
    if (v.special == 1)
      {
        char number[3 * sizeof (size_t) + 1];
        size_t end = sizeof (number), i = end, value = v.index;
        do
          {
            number[--i] = (char)('0' + value % 10);
            value /= 10;
          }
        while (value);
        return mulib_template_bytes_ (s, number + i, end - i, raw);
      }
    if (mulib_json_typeof (v.value) == MULIB_JSON_NULL)
      return 1;
    p = mulib_json_text (v.value, &n);
    if (!p)
      {
        owned = mulib_json_stringify (v.value, 0);
        if (!owned)
          return 0;
        p = owned;
        n = strlen (owned);
      }
    ok = mulib_template_bytes_ (s, p, n, raw);
    free (owned);
    return ok;
  }
  static int
  mulib_template_run_ (mulib_template_state_ *s, size_t start, size_t end,
                       const mulib_template_frame_ *frame)
  {
    size_t i;
    for (i = start; i < end; ++i)
      {
        mulib_template_token_ *t = &s->tokens[i];
        mulib_template_lookup_ value;
        if (t->kind == MULIB_TEMPLATE_TEXT_)
          {
            if (!mulib_strbuf_append_n (&s->output, s->source + t->start,
                                        t->length))
              return mulib_template_fail_ (
                  s, t->offset, "out of memory or output too large");
            continue;
          }
        value = mulib_template_lookup_path_ (s->source + t->start, t->length,
                                             frame);
        if (t->kind == MULIB_TEMPLATE_VALUE_ || t->kind == MULIB_TEMPLATE_RAW_)
          {
            if (!mulib_template_emit_ (s, value,
                                       t->kind == MULIB_TEMPLATE_RAW_))
              return mulib_template_fail_ (
                  s, t->offset,
                  "cannot serialize context value or grow output");
          }
        else if (t->kind == MULIB_TEMPLATE_IF_
                 || t->kind == MULIB_TEMPLATE_EACH_)
          {
            size_t body_end
                = t->alternate == SIZE_MAX ? t->end : t->alternate - 1;
            if (t->kind == MULIB_TEMPLATE_IF_)
              {
                if (mulib_template_truth_ (value))
                  {
                    if (!mulib_template_run_ (s, i + 1, body_end, frame))
                      return 0;
                  }
                else if (t->alternate != SIZE_MAX
                         && !mulib_template_run_ (s, t->alternate, t->end,
                                                  frame))
                  return 0;
              }
            else
              {
                size_t j, count = mulib_json_size (value.value);
                for (j = 0; j < count; ++j)
                  {
                    mulib_template_frame_ inner;
                    memset (&inner, 0, sizeof (inner));
                    inner.value = mulib_json_at (value.value, j);
                    inner.root = frame->root;
                    inner.parent = frame;
                    inner.index = j;
                    inner.loop = 1;
                    inner.key
                        = mulib_json_key (value.value, j, &inner.key_length);
                    if (!mulib_template_run_ (s, i + 1, body_end, &inner))
                      return 0;
                  }
                if (!count && t->alternate != SIZE_MAX
                    && !mulib_template_run_ (s, t->alternate, t->end, frame))
                  return 0;
              }
            i = t->end;
          }
      }
    return 1;
  }
  char *
  mulib_template_render (const char *source, size_t length,
                         const mulib_json_t *context, size_t *output_length,
                         mulib_template_error_t *error)
  {
    mulib_template_state_ s;
    mulib_template_frame_ frame;
    int ok;
    memset (&s, 0, sizeof (s));
    memset (&frame, 0, sizeof (frame));
    if (output_length)
      *output_length = 0;
    if (error)
      memset (error, 0, sizeof (*error));
    s.source = source ? source : "";
    s.length = length;
    s.error = error;
    mulib_strbuf_init (&s.output);
    if ((!source && length)
        || !mulib_utf8_validate ((const unsigned char *)s.source, length))
      {
        mulib_template_fail_ (&s, 0, "invalid source or UTF-8");
        return NULL;
      }
    frame.value = frame.root = context;
    ok = mulib_template_compile_ (&s)
         && mulib_template_run_ (&s, 0, s.count, &frame)
         && mulib_strbuf_append_n (&s.output, "", 0);
    free (s.tokens);
    if (!ok)
      {
        if (error && !error->message)
          mulib_template_fail_ (&s, 0, "out of memory");
        mulib_strbuf_free (&s.output);
        return NULL;
      }
    if (output_length)
      *output_length = s.output.length;
    return s.output.data;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
