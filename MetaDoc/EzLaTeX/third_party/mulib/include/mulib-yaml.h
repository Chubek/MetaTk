/**
 * @file mulib-yaml.h
 * @brief A bounded YAML 1.2-style configuration subset backed by the JSON DOM.
 * Not a full YAML processor. See manual/mulib-yaml.md for accepted/rejected
 * syntax. Define MULIB_IMPL_YAML once, with JSON/STRBUF/UTF8 and JSON's
 * PARSEC.
 */
#ifndef MULIB_YAML_H
#define MULIB_YAML_H
#include "mulib-json.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Static diagnostic message, byte offset, one-based line and column.
   */
  typedef struct mulib_yaml_error
  {
    size_t offset, line, column;
    const char *message;
  } mulib_yaml_error_t;
  /**
   * @brief Parse one UTF-8 configuration document into a caller-owned JSON
   * DOM. Empty/comment-only documents produce JSON null. Input is borrowed;
   * embedded source NULs, duplicate keys and unsupported syntax fail with
   * NULL. On failure no partial tree escapes. error may be NULL. Maximum
   * container depth is 128.
   */
  mulib_json_t *mulib_yaml_parse (const char *source, size_t length,
                                  mulib_yaml_error_t *error);
  /**
   * @brief Serialize a borrowed JSON value as malloc-owned block-style YAML.
   * Keys and strings are double-quoted; scalar spelling is JSON-compatible.
   * NULL value means JSON null. Container depth, including empty containers,
   * is limited to 128. Returns NULL on allocation/depth failure.
   */
  char *mulib_yaml_stringify (const mulib_json_t *value,
                              mulib_yaml_error_t *error);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_YAML_H */

#if defined(MULIB_IMPL_YAML) && !defined(MULIB_YAML_IMPLEMENTED)
#define MULIB_YAML_IMPLEMENTED
#include "mulib-strbuf.h"
#include "mulib-utf8.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_yaml_line_
  {
    size_t start, end, indent;
  } mulib_yaml_line_;
  typedef struct mulib_yaml_state_
  {
    const char *source;
    size_t length, count, capacity;
    mulib_yaml_line_ *lines;
    mulib_yaml_error_t *error;
    int failed;
  } mulib_yaml_state_;
  typedef struct mulib_yaml_inline_
  {
    mulib_yaml_state_ *state;
    size_t pos, end;
  } mulib_yaml_inline_;

  static int
  mulib_yaml_fail_ (mulib_yaml_state_ *s, size_t at, const char *message)
  {
    if (!s->failed && s->error)
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
    s->failed = 1;
    return 0;
  }
  static int
  mulib_yaml_space_ (char c)
  {
    return c == ' ' || c == '\t';
  }
  static int
  mulib_yaml_equal_ (const char *p, size_t n, const char *word)
  {
    return n == strlen (word) && memcmp (p, word, n) == 0;
  }
  static int
  mulib_yaml_lines_ (mulib_yaml_state_ *s)
  {
    size_t pos = 0;
    while (pos < s->length)
      {
        size_t begin = pos, start, end, indent = 0;
        mulib_yaml_line_ *p;
        while (pos < s->length && s->source[pos] != '\n')
          ++pos;
        end = pos;
        if (pos < s->length)
          ++pos;
        if (end > begin && s->source[end - 1] == '\r')
          --end;
        start = begin;
        if (!begin && end >= 3 && memcmp (s->source, "\xef\xbb\xbf", 3) == 0)
          start += 3;
        while (start < end && s->source[start] == ' ')
          {
            ++start;
            ++indent;
          }
        if (start < end && s->source[start] == '\t')
          return mulib_yaml_fail_ (s, start,
                                   "tabs are not allowed in indentation");
        while (end > start && mulib_yaml_space_ (s->source[end - 1]))
          --end;
        if (start == end || s->source[start] == '#')
          continue;
        if (!indent && end - start > 3
            && (memcmp (s->source + start, "---", 3) == 0
                || memcmp (s->source + start, "...", 3) == 0)
            && mulib_yaml_space_ (s->source[start + 3]))
          {
            size_t tail = start + 3;
            while (tail < end && mulib_yaml_space_ (s->source[tail]))
              ++tail;
            if (tail < end && s->source[tail] != '#')
              return mulib_yaml_fail_ (
                  s, start,
                  "document markers must be standalone; inline document "
                  "content is unsupported");
          }
        if (s->count == s->capacity)
          {
            size_t cap = s->capacity ? s->capacity * 2 : 32;
            if (cap < s->capacity || cap > SIZE_MAX / sizeof (*p))
              return mulib_yaml_fail_ (s, start, "document is too large");
            p = (mulib_yaml_line_ *)realloc (s->lines, cap * sizeof (*p));
            if (!p)
              return mulib_yaml_fail_ (s, start, "out of memory");
            s->lines = p;
            s->capacity = cap;
          }
        s->lines[s->count].start = start;
        s->lines[s->count].end = end;
        s->lines[s->count++].indent = indent;
      }
    return 1;
  }
  static void
  mulib_yaml_skip_ (mulib_yaml_inline_ *p)
  {
    while (p->pos < p->end && mulib_yaml_space_ (p->state->source[p->pos]))
      ++p->pos;
  }
  static int
  mulib_yaml_hex_ (char c)
  {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    return -1;
  }
  static int
  mulib_yaml_code_ (mulib_yaml_inline_ *p, unsigned digits, uint32_t *code)
  {
    unsigned i;
    *code = 0;
    if (p->end - p->pos < digits)
      return mulib_yaml_fail_ (p->state, p->pos, "truncated Unicode escape");
    for (i = 0; i < digits; ++i)
      {
        int h = mulib_yaml_hex_ (p->state->source[p->pos++]);
        if (h < 0)
          return mulib_yaml_fail_ (p->state, p->pos - 1,
                                   "invalid Unicode escape");
        *code = (*code << 4) | (uint32_t)h;
      }
    return 1;
  }
  static mulib_json_t *
  mulib_yaml_quoted_ (mulib_yaml_inline_ *p)
  {
    mulib_yaml_state_ *s = p->state;
    size_t at = p->pos;
    char quote = s->source[p->pos++];
    mulib_strbuf_t b;
    mulib_json_t *result = NULL;
    int closed = 0;
    mulib_strbuf_init (&b);
    while (p->pos < p->end)
      {
        unsigned char c = (unsigned char)s->source[p->pos++];
        if (c == (unsigned char)quote)
          {
            if (quote == '\'' && p->pos < p->end && s->source[p->pos] == '\'')
              ++p->pos;
            else
              {
                closed = 1;
                break;
              }
          }
        else if (quote == '"' && c == '\\')
          {
            uint32_t code = 0;
            unsigned char encoded[4];
            size_t n;
            if (p->pos == p->end)
              {
                mulib_yaml_fail_ (s, p->pos, "truncated escape");
                break;
              }
            c = (unsigned char)s->source[p->pos++];
            switch (c)
              {
              case '0':
                c = 0;
                break;
              case 'a':
                c = 7;
                break;
              case 'b':
                c = 8;
                break;
              case 't':
                c = 9;
                break;
              case 'n':
                c = 10;
                break;
              case 'v':
                c = 11;
                break;
              case 'f':
                c = 12;
                break;
              case 'r':
                c = 13;
                break;
              case 'e':
                c = 27;
                break;
              case ' ':
              case '\t':
              case '"':
              case '/':
              case '\\':
                break;
              case 'N':
                code = 0x85;
                break;
              case '_':
                code = 0xa0;
                break;
              case 'L':
                code = 0x2028;
                break;
              case 'P':
                code = 0x2029;
                break;
              case 'x':
              case 'u':
              case 'U':
                if (!mulib_yaml_code_ (p,
                                       c == 'x'   ? 2U
                                       : c == 'u' ? 4U
                                                  : 8U,
                                       &code))
                  goto done;
                if (c == 'u' && code >= 0xd800 && code <= 0xdbff)
                  {
                    uint32_t low;
                    if (p->end - p->pos < 6 || s->source[p->pos] != '\\'
                        || s->source[p->pos + 1] != 'u')
                      {
                        mulib_yaml_fail_ (s, p->pos,
                                          "unpaired Unicode surrogate");
                        goto done;
                      }
                    p->pos += 2;
                    if (!mulib_yaml_code_ (p, 4, &low))
                      goto done;
                    if (low < 0xdc00 || low > 0xdfff)
                      {
                        mulib_yaml_fail_ (s, p->pos - 4,
                                          "unpaired Unicode surrogate");
                        goto done;
                      }
                    code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
                  }
                if (code == 0)
                  {
                    c = 0;
                    break;
                  }
                /* fall through to UTF-8 encoding below */
                break;
              default:
                mulib_yaml_fail_ (s, p->pos - 1, "unknown string escape");
                goto done;
              }
            if (code)
              {
                n = mulib_utf8_encode (code, encoded, sizeof (encoded));
                if (!n)
                  {
                    mulib_yaml_fail_ (s, p->pos, "invalid Unicode scalar");
                    goto done;
                  }
                if (!mulib_strbuf_append_n (&b, (const char *)encoded, n))
                  goto memory;
                continue;
              }
          }
        if (!mulib_strbuf_putc (&b, (char)c))
          goto memory;
      }
    if (!closed)
      {
        mulib_yaml_fail_ (s, at, "unclosed or multiline quoted string");
        goto done;
      }
    result = mulib_json_string_n (b.data ? b.data : "", b.length);
    if (!result)
      goto memory;
    goto done;
  memory:
    mulib_yaml_fail_ (s, at, "out of memory");
  done:
    mulib_strbuf_free (&b);
    return result;
  }
  /* Return a numeric value only for the complete supported decimal grammar. */
  static mulib_json_t *
  mulib_yaml_number_ (mulib_yaml_state_ *s, size_t at, size_t n, int *numeric)
  {
    const char *p = s->source + at;
    size_t i = 0, first, integer_end, fraction = SIZE_MAX, fraction_end = 0,
           exponent = SIZE_MAX;
    int negative = 0, digits = 0;
    mulib_strbuf_t b;
    mulib_json_t *value;
    *numeric = 0;
    if (i < n && (p[i] == '+' || p[i] == '-'))
      {
        negative = p[i] == '-';
        ++i;
      }
    first = i;
    while (i < n && p[i] >= '0' && p[i] <= '9')
      {
        ++i;
        digits = 1;
      }
    integer_end = i;
    if (i < n && p[i] == '.')
      {
        fraction = ++i;
        while (i < n && p[i] >= '0' && p[i] <= '9')
          {
            ++i;
            digits = 1;
          }
        fraction_end = i;
      }
    if (!digits)
      return NULL;
    if (i < n && (p[i] == 'e' || p[i] == 'E'))
      {
        size_t exp_digits;
        exponent = i++;
        if (i < n && (p[i] == '+' || p[i] == '-'))
          ++i;
        exp_digits = i;
        while (i < n && p[i] >= '0' && p[i] <= '9')
          ++i;
        if (i == exp_digits)
          {
            if (i == n)
              {
                *numeric = 1;
                mulib_yaml_fail_ (s, at, "invalid numeric exponent");
              }
            return NULL;
          }
      }
    if (i != n)
      return NULL;
    *numeric = 1;
    if (fraction == SIZE_MAX && exponent == SIZE_MAX)
      {
        uint64_t integer = 0;
        for (i = first; i < integer_end; ++i)
          {
            unsigned d = (unsigned)(p[i] - '0');
            if (integer > (UINT64_C (9007199254740991) - d) / 10)
              {
                mulib_yaml_fail_ (s, at,
                                  "integer exceeds exact JSON numeric range");
                return NULL;
              }
            integer = integer * 10 + d;
          }
        value = mulib_json_number (negative ? -(double)integer
                                            : (double)integer);
        if (!value)
          mulib_yaml_fail_ (s, at, "out of memory");
        return value;
      }
    mulib_strbuf_init (&b);
    if (negative && !mulib_strbuf_putc (&b, '-'))
      goto memory;
    while (first < integer_end && p[first] == '0')
      ++first;
    if (!(first < integer_end
              ? mulib_strbuf_append_n (&b, p + first, integer_end - first)
              : mulib_strbuf_putc (&b, '0')))
      goto memory;
    if (fraction != SIZE_MAX)
      {
        if (!mulib_strbuf_putc (&b, '.')
            || !(fraction < fraction_end
                     ? mulib_strbuf_append_n (&b, p + fraction,
                                              fraction_end - fraction)
                     : mulib_strbuf_putc (&b, '0')))
          goto memory;
      }
    if (exponent != SIZE_MAX
        && !mulib_strbuf_append_n (&b, p + exponent, n - exponent))
      goto memory;
    value = mulib_json_parse (b.data, b.length, NULL);
    mulib_strbuf_free (&b);
    if (!value)
      mulib_yaml_fail_ (s, at, "numeric value is out of range");
    return value;
  memory:
    mulib_strbuf_free (&b);
    mulib_yaml_fail_ (s, at, "out of memory");
    return NULL;
  }
  static mulib_json_t *
  mulib_yaml_plain_ (mulib_yaml_inline_ *p, int flow)
  {
    mulib_yaml_state_ *s = p->state;
    size_t at = p->pos, end;
    const char *text = s->source;
    mulib_json_t *v;
    int numeric;
    size_t prefix;
    while (p->pos < p->end)
      {
        char c = text[p->pos];
        if ((flow && (c == ',' || c == ']' || c == '}'))
            || (c == '#'
                && (p->pos == at || mulib_yaml_space_ (text[p->pos - 1]))))
          break;
        if (flow && (c == '[' || c == '{'))
          {
            mulib_yaml_fail_ (
                s, p->pos,
                "flow indicators inside plain scalars must be quoted");
            return NULL;
          }
        if (c == ':'
            && (p->pos + 1 == p->end || mulib_yaml_space_ (text[p->pos + 1])
                || (flow && strchr ("[]{},", text[p->pos + 1]))))
          {
            mulib_yaml_fail_ (s, p->pos,
                              "mapping delimiter inside a plain scalar");
            return NULL;
          }
        ++p->pos;
      }
    end = p->pos;
    while (end > at && mulib_yaml_space_ (text[end - 1]))
      --end;
    if (end == at)
      {
        mulib_yaml_fail_ (s, at, "expected a scalar");
        return NULL;
      }
    if (strchr (",]}&*!|>@%`", text[at])
        || ((text[at] == '?' || text[at] == '-')
            && (end == at + 1 || mulib_yaml_space_ (text[at + 1]))))
      {
        mulib_yaml_fail_ (
            s, at,
            "unsupported YAML tag, anchor, alias, block scalar or indicator");
        return NULL;
      }
    prefix = at + ((text[at] == '+' || text[at] == '-') ? 1U : 0U);
    if (mulib_yaml_equal_ (text + at, end - at, ".inf")
        || mulib_yaml_equal_ (text + at, end - at, ".Inf")
        || mulib_yaml_equal_ (text + at, end - at, ".INF")
        || mulib_yaml_equal_ (text + at, end - at, ".nan")
        || mulib_yaml_equal_ (text + at, end - at, ".NaN")
        || mulib_yaml_equal_ (text + at, end - at, ".NAN")
        || ((text[at] == '+' || text[at] == '-') && end - at > 1
            && text[at + 1] == '.')
        || (end - prefix > 2 && text[prefix] == '0'
            && (text[prefix + 1] == 'x' || text[prefix + 1] == 'o')))
      {
        /* Signed finite decimals such as -.5 are handled below. */
        if (!((text[at] == '+' || text[at] == '-') && end - at > 2
              && text[at + 1] == '.' && text[at + 2] >= '0'
              && text[at + 2] <= '9'))
          {
            mulib_yaml_fail_ (
                s, at, "non-finite and non-decimal numbers are unsupported");
            return NULL;
          }
      }
    if (mulib_yaml_equal_ (text + at, end - at, "null")
        || mulib_yaml_equal_ (text + at, end - at, "Null")
        || mulib_yaml_equal_ (text + at, end - at, "NULL")
        || mulib_yaml_equal_ (text + at, end - at, "~"))
      v = mulib_json_new (MULIB_JSON_NULL);
    else if (mulib_yaml_equal_ (text + at, end - at, "true")
             || mulib_yaml_equal_ (text + at, end - at, "True")
             || mulib_yaml_equal_ (text + at, end - at, "TRUE"))
      v = mulib_json_bool (1);
    else if (mulib_yaml_equal_ (text + at, end - at, "false")
             || mulib_yaml_equal_ (text + at, end - at, "False")
             || mulib_yaml_equal_ (text + at, end - at, "FALSE"))
      v = mulib_json_bool (0);
    else
      {
        v = mulib_yaml_number_ (s, at, end - at, &numeric);
        if (numeric || s->failed)
          return v;
        v = mulib_json_string_n (text + at, end - at);
      }
    if (!v)
      mulib_yaml_fail_ (s, at, "out of memory");
    return v;
  }
  static mulib_json_t *mulib_yaml_inline_value_ (mulib_yaml_inline_ *p,
                                                 unsigned depth);
  static mulib_json_t *
  mulib_yaml_flow_key_ (mulib_yaml_inline_ *p)
  {
    size_t start, end;
    mulib_json_t *key;
    mulib_yaml_skip_ (p);
    if (p->pos < p->end
        && (p->state->source[p->pos] == '\''
            || p->state->source[p->pos] == '"'))
      return mulib_yaml_quoted_ (p);
    start = p->pos;
    while (p->pos < p->end)
      {
        char c = p->state->source[p->pos];
        if (c == ',' || c == '}')
          break;
        if (c == ':'
            && (p->pos + 1 == p->end
                || mulib_yaml_space_ (p->state->source[p->pos + 1])
                || strchr ("[]{},", p->state->source[p->pos + 1])))
          break;
        if (c == '[' || c == ']' || c == '{'
            || (c == '#'
                && (p->pos == start
                    || mulib_yaml_space_ (p->state->source[p->pos - 1]))))
          {
            mulib_yaml_fail_ (
                p->state, p->pos,
                "complex keys/comments in flow mappings are unsupported");
            return NULL;
          }
        ++p->pos;
      }
    end = p->pos;
    while (end > start && mulib_yaml_space_ (p->state->source[end - 1]))
      --end;
    if (start == end || strchr ("[]{}&*!?|>@%`", p->state->source[start])
        || (p->state->source[start] == '-' && end - start > 1
            && mulib_yaml_space_ (p->state->source[start + 1])))
      {
        mulib_yaml_fail_ (p->state, start,
                          "expected a simple string mapping key");
        return NULL;
      }
    if (mulib_yaml_equal_ (p->state->source + start, end - start, "<<"))
      {
        mulib_yaml_fail_ (p->state, start, "YAML merge keys are unsupported");
        return NULL;
      }
    key = mulib_json_string_n (p->state->source + start, end - start);
    if (!key)
      mulib_yaml_fail_ (p->state, start, "out of memory");
    return key;
  }
  static mulib_json_t *
  mulib_yaml_inline_value_ (mulib_yaml_inline_ *p, unsigned depth)
  {
    mulib_yaml_state_ *s = p->state;
    mulib_json_t *container;
    char opening, closing;
    size_t at;
    mulib_yaml_skip_ (p);
    at = p->pos;
    if (at == p->end)
      {
        mulib_yaml_fail_ (s, at, "expected a value");
        return NULL;
      }
    opening = s->source[p->pos];
    if (opening == '\'' || opening == '"')
      return mulib_yaml_quoted_ (p);
    if (opening != '[' && opening != '{')
      return mulib_yaml_plain_ (p, 1);
    if (depth >= 128)
      {
        mulib_yaml_fail_ (s, at, "YAML nesting exceeds 128");
        return NULL;
      }
    closing = opening == '[' ? ']' : '}';
    container = mulib_json_new (opening == '[' ? MULIB_JSON_ARRAY
                                               : MULIB_JSON_OBJECT);
    if (!container)
      {
        mulib_yaml_fail_ (s, at, "out of memory");
        return NULL;
      }
    ++p->pos;
    mulib_yaml_skip_ (p);
    if (p->pos < p->end && s->source[p->pos] == closing)
      {
        ++p->pos;
        return container;
      }
    while (p->pos < p->end)
      {
        mulib_json_t *key = NULL, *value;
        const char *key_text = NULL;
        size_t key_length = 0;
        if (opening == '{')
          {
            key = mulib_yaml_flow_key_ (p);
            if (!key)
              goto fail;
            key_text = mulib_json_text (key, &key_length);
            mulib_yaml_skip_ (p);
            if (p->pos == p->end || s->source[p->pos] != ':')
              {
                mulib_yaml_fail_ (s, p->pos, "expected ':' in a flow mapping");
                mulib_json_destroy (key);
                goto fail;
              }
            if (mulib_json_get_n (container, key_text, key_length))
              {
                mulib_yaml_fail_ (s, p->pos, "duplicate mapping key");
                mulib_json_destroy (key);
                goto fail;
              }
            ++p->pos;
            mulib_yaml_skip_ (p);
          }
        value = mulib_yaml_inline_value_ (p, depth + 1);
        if (!value)
          {
            mulib_json_destroy (key);
            goto fail;
          }
        if (!(opening == '['
                  ? mulib_json_append (container, value)
                  : mulib_json_set_n (container, key_text, key_length, value)))
          {
            mulib_json_destroy (key);
            mulib_json_destroy (value);
            mulib_yaml_fail_ (s, at, "out of memory");
            goto fail;
          }
        mulib_json_destroy (key);
        mulib_yaml_skip_ (p);
        if (p->pos < p->end && s->source[p->pos] == closing)
          {
            ++p->pos;
            return container;
          }
        if (p->pos == p->end || s->source[p->pos] != ',')
          break;
        ++p->pos;
        mulib_yaml_skip_ (p);
        if (p->pos < p->end && s->source[p->pos] == closing)
          {
            ++p->pos;
            return container;
          }
      }
    mulib_yaml_fail_ (s, p->pos,
                      "unclosed flow collection or missing comma (multiline "
                      "flow is unsupported)");
  fail:
    mulib_json_destroy (container);
    return NULL;
  }
  static mulib_json_t *
  mulib_yaml_line_value_ (mulib_yaml_state_ *s, size_t start, size_t end,
                          unsigned depth)
  {
    mulib_yaml_inline_ p;
    mulib_json_t *v;
    p.state = s;
    p.pos = start;
    p.end = end;
    mulib_yaml_skip_ (&p);
    if (p.pos < end && s->source[p.pos] != '[' && s->source[p.pos] != '{'
        && s->source[p.pos] != '\'' && s->source[p.pos] != '"')
      v = mulib_yaml_plain_ (&p, 0);
    else
      v = mulib_yaml_inline_value_ (&p, depth);
    if (!v)
      return NULL;
    mulib_yaml_skip_ (&p);
    if (p.pos < end
        && (s->source[p.pos] != '#' || p.pos == start
            || !mulib_yaml_space_ (s->source[p.pos - 1])))
      {
        mulib_json_destroy (v);
        mulib_yaml_fail_ (s, p.pos, "unexpected text after a value");
        return NULL;
      }
    return v;
  }
  /* A block mapping colon must be followed by separation or end-of-line. */
  static size_t
  mulib_yaml_colon_ (mulib_yaml_state_ *s, size_t start, size_t end)
  {
    size_t i = start;
    if (i < end && (s->source[i] == '\'' || s->source[i] == '"'))
      {
        char q = s->source[i++];
        while (i < end)
          {
            if (q == '"' && s->source[i] == '\\')
              {
                i += end - i >= 2 ? 2 : 1;
                continue;
              }
            if (s->source[i++] == q)
              {
                if (q == '\'' && i < end && s->source[i] == q)
                  {
                    ++i;
                    continue;
                  }
                break;
              }
          }
        while (i < end && mulib_yaml_space_ (s->source[i]))
          ++i;
        return i < end && s->source[i] == ':'
                       && (i + 1 == end
                           || mulib_yaml_space_ (s->source[i + 1]))
                   ? i
                   : SIZE_MAX;
      }
    if (i < end && (s->source[i] == '[' || s->source[i] == '{'))
      return SIZE_MAX;
    for (; i < end; ++i)
      {
        if (s->source[i] == '#'
            && (i == start || mulib_yaml_space_ (s->source[i - 1])))
          break;
        if (s->source[i] == ':'
            && (i + 1 == end || mulib_yaml_space_ (s->source[i + 1])))
          return i;
      }
    return SIZE_MAX;
  }
  static int
  mulib_yaml_sequence_ (mulib_yaml_state_ *s, size_t start, size_t end)
  {
    return start < end && s->source[start] == '-'
           && (start + 1 == end || mulib_yaml_space_ (s->source[start + 1]));
  }
  static int
  mulib_yaml_absent_ (mulib_yaml_state_ *s, size_t *start, size_t end)
  {
    while (*start < end && mulib_yaml_space_ (s->source[*start]))
      ++*start;
    return *start == end || s->source[*start] == '#';
  }
  static mulib_json_t *mulib_yaml_block_ (mulib_yaml_state_ *s, size_t *index,
                                          size_t indent, unsigned depth);
  static int mulib_yaml_marker_ (mulib_yaml_state_ *s, size_t index,
                                 const char *marker);
  static mulib_json_t *
  mulib_yaml_mapping_ (mulib_yaml_state_ *s, size_t *index, size_t indent,
                       size_t first_start, unsigned depth)
  {
    mulib_json_t *object = mulib_json_new (MULIB_JSON_OBJECT);
    int first = 1;
    if (!object)
      {
        mulib_yaml_fail_ (s, s->lines[*index].start, "out of memory");
        return NULL;
      }
    while (*index < s->count)
      {
        mulib_yaml_line_ line = s->lines[*index];
        size_t start = first ? first_start : line.start, colon, end,
               key_length, value_start;
        mulib_json_t *key, *value;
        const char *text;
        mulib_yaml_inline_ key_parser;
        if (!first && line.indent != indent)
          break;
        if (!first
            && (mulib_yaml_marker_ (s, *index, "---")
                || mulib_yaml_marker_ (s, *index, "...")))
          break;
        first = 0;
        colon = mulib_yaml_colon_ (s, start, line.end);
        if (colon == SIZE_MAX || mulib_yaml_sequence_ (s, start, line.end))
          {
            mulib_yaml_fail_ (s, start,
                              "expected a mapping entry at this indentation");
            goto fail;
          }
        end = colon;
        while (end > start && mulib_yaml_space_ (s->source[end - 1]))
          --end;
        if (end == start)
          {
            mulib_yaml_fail_ (s, start, "empty unquoted mapping key");
            goto fail;
          }
        key_parser.state = s;
        key_parser.pos = start;
        key_parser.end = end;
        if (s->source[start] == '\'' || s->source[start] == '"')
          {
            key = mulib_yaml_quoted_ (&key_parser);
            if (key && key_parser.pos != end)
              {
                mulib_json_destroy (key);
                key = NULL;
                mulib_yaml_fail_ (s, key_parser.pos,
                                  "unexpected text in mapping key");
              }
          }
        else
          {
            if (strchr (",[]{}&*!?|>@%`", s->source[start])
                || mulib_yaml_equal_ (s->source + start, end - start, "<<"))
              {
                mulib_yaml_fail_ (s, start,
                                  "invalid or unsupported plain mapping key");
                goto fail;
              }
            key = mulib_json_string_n (s->source + start, end - start);
          }
        if (!key)
          {
            if (!s->failed)
              mulib_yaml_fail_ (s, start, "out of memory");
            goto fail;
          }
        text = mulib_json_text (key, &key_length);
        if (mulib_json_get_n (object, text, key_length))
          {
            mulib_json_destroy (key);
            mulib_yaml_fail_ (s, start, "duplicate mapping key");
            goto fail;
          }
        value_start = colon + 1;
        if (mulib_yaml_absent_ (s, &value_start, line.end))
          {
            ++*index;
            if (*index < s->count
                && (s->lines[*index].indent > indent
                    || (s->lines[*index].indent == indent
                        && mulib_yaml_sequence_ (s, s->lines[*index].start,
                                                 s->lines[*index].end))))
              value = mulib_yaml_block_ (s, index, s->lines[*index].indent,
                                         depth + 1);
            else
              value = mulib_json_new (MULIB_JSON_NULL);
          }
        else
          {
            value
                = mulib_yaml_line_value_ (s, value_start, line.end, depth + 1);
            ++*index;
            if (value && *index < s->count && s->lines[*index].indent > indent)
              {
                mulib_json_destroy (value);
                value = NULL;
                mulib_yaml_fail_ (s, s->lines[*index].start,
                                  "unexpected indentation; multiline "
                                  "scalars/flow are unsupported");
              }
          }
        if (!value || !mulib_json_set_n (object, text, key_length, value))
          {
            mulib_json_destroy (key);
            mulib_json_destroy (value);
            if (!s->failed)
              mulib_yaml_fail_ (s, start, "out of memory");
            goto fail;
          }
        mulib_json_destroy (key);
      }
    return object;
  fail:
    mulib_json_destroy (object);
    return NULL;
  }
  static mulib_json_t *
  mulib_yaml_block_ (mulib_yaml_state_ *s, size_t *index, size_t indent,
                     unsigned depth)
  {
    mulib_yaml_line_ first = s->lines[*index];
    mulib_json_t *array;
    if (depth >= 128)
      {
        mulib_yaml_fail_ (s, first.start, "YAML nesting exceeds 128");
        return NULL;
      }
    if (!mulib_yaml_sequence_ (s, first.start, first.end))
      {
        mulib_json_t *v;
        if (mulib_yaml_colon_ (s, first.start, first.end) != SIZE_MAX)
          return mulib_yaml_mapping_ (s, index, indent, first.start, depth);
        v = mulib_yaml_line_value_ (s, first.start, first.end, depth);
        ++*index;
        return v;
      }
    array = mulib_json_new (MULIB_JSON_ARRAY);
    if (!array)
      {
        mulib_yaml_fail_ (s, first.start, "out of memory");
        return NULL;
      }
    while (*index < s->count && s->lines[*index].indent == indent
           && mulib_yaml_sequence_ (s, s->lines[*index].start,
                                    s->lines[*index].end))
      {
        mulib_yaml_line_ line = s->lines[*index];
        size_t value_start = line.start + 1;
        mulib_json_t *value;
        if (mulib_yaml_absent_ (s, &value_start, line.end))
          {
            ++*index;
            if (*index < s->count && s->lines[*index].indent > indent)
              value = mulib_yaml_block_ (s, index, s->lines[*index].indent,
                                         depth + 1);
            else
              value = mulib_json_new (MULIB_JSON_NULL);
          }
        else if (mulib_yaml_colon_ (s, value_start, line.end) != SIZE_MAX)
          {
            if (depth + 1 >= 128)
              {
                mulib_yaml_fail_ (s, value_start, "YAML nesting exceeds 128");
                goto fail;
              }
            value = mulib_yaml_mapping_ (s, index,
                                         indent + value_start - line.start,
                                         value_start, depth + 1);
          }
        else
          {
            if (mulib_yaml_sequence_ (s, value_start, line.end))
              {
                mulib_yaml_fail_ (s, value_start,
                                  "compact nested sequences are unsupported; "
                                  "use a new indented line");
                goto fail;
              }
            value
                = mulib_yaml_line_value_ (s, value_start, line.end, depth + 1);
            ++*index;
            if (value && *index < s->count && s->lines[*index].indent > indent)
              {
                mulib_json_destroy (value);
                value = NULL;
                mulib_yaml_fail_ (
                    s, s->lines[*index].start,
                    "unexpected indentation after a sequence scalar");
              }
          }
        if (!value || !mulib_json_append (array, value))
          {
            mulib_json_destroy (value);
            if (!s->failed)
              mulib_yaml_fail_ (s, line.start, "out of memory");
            goto fail;
          }
      }
    return array;
  fail:
    mulib_json_destroy (array);
    return NULL;
  }
  static int
  mulib_yaml_marker_ (mulib_yaml_state_ *s, size_t index, const char *marker)
  {
    mulib_yaml_line_ line = s->lines[index];
    size_t p = line.start + 3;
    return line.indent == 0 && line.end - line.start >= 3
           && memcmp (s->source + line.start, marker, 3) == 0
           && (p == line.end
               || (mulib_yaml_space_ (s->source[p])
                   && mulib_yaml_absent_ (s, &p, line.end)));
  }
  mulib_json_t *
  mulib_yaml_parse (const char *source, size_t length,
                    mulib_yaml_error_t *error)
  {
    mulib_yaml_state_ s;
    mulib_json_t *value = NULL;
    size_t i, index = 0;
    memset (&s, 0, sizeof (s));
    if (error)
      memset (error, 0, sizeof (*error));
    s.source = source ? source : "";
    s.length = length;
    s.error = error;
    if ((!source && length)
        || !mulib_utf8_validate ((const unsigned char *)s.source, length))
      {
        mulib_yaml_fail_ (&s, 0, "invalid input or UTF-8");
        return NULL;
      }
    for (i = 0; i < length; ++i)
      {
        unsigned char c = (unsigned char)s.source[i];
        if ((c < 32 && c != '\t' && c != '\n' && c != '\r') || c == 127
            || (c == '\r' && (i + 1 == length || s.source[i + 1] != '\n')))
          {
            mulib_yaml_fail_ (&s, i, "invalid control byte in YAML source");
            return NULL;
          }
        if (c >= 128)
          {
            uint32_t code;
            size_t used;
            if (!mulib_utf8_decode ((const unsigned char *)s.source + i,
                                    length - i, &code, &used))
              {
                mulib_yaml_fail_ (&s, i, "invalid UTF-8");
                return NULL;
              }
            if ((code >= 0x80 && code <= 0x9f && code != 0x85)
                || code == 0xfffe || code == 0xffff)
              {
                mulib_yaml_fail_ (&s, i,
                                  "non-printable Unicode scalar in YAML "
                                  "source; use an escape");
                return NULL;
              }
            i += used - 1;
          }
      }
    if (!mulib_yaml_lines_ (&s))
      goto done;
    if (index < s.count && mulib_yaml_marker_ (&s, index, "---"))
      ++index;
    if (index < s.count && mulib_yaml_marker_ (&s, index, "---"))
      {
        mulib_yaml_fail_ (&s, s.lines[index].start,
                          "multiple YAML documents are unsupported");
        goto done;
      }
    if (index == s.count || mulib_yaml_marker_ (&s, index, "..."))
      value = mulib_json_new (MULIB_JSON_NULL);
    else
      value = mulib_yaml_block_ (&s, &index, s.lines[index].indent, 0);
    if (!value)
      {
        if (!s.failed)
          mulib_yaml_fail_ (&s, 0, "out of memory");
        goto done;
      }
    if (index < s.count && mulib_yaml_marker_ (&s, index, "..."))
      ++index;
    if (index != s.count)
      {
        mulib_yaml_fail_ (&s, s.lines[index].start,
                          "unexpected indentation or additional document");
        mulib_json_destroy (value);
        value = NULL;
      }
  done:
    free (s.lines);
    return value;
  }
  static int
  mulib_yaml_indent_ (mulib_strbuf_t *b, unsigned depth)
  {
    unsigned i;
    for (i = 0; i < depth; ++i)
      if (!mulib_strbuf_append (b, "  "))
        return 0;
    return 1;
  }
  static int
  mulib_yaml_scalar_out_ (mulib_strbuf_t *b, const mulib_json_t *v)
  {
    char *text;
    int ok;
    size_t i = 0, start = 0, length;
    if (!v)
      return mulib_strbuf_append (b, "null");
    text = mulib_json_stringify (v, 0);
    if (!text)
      return 0;
    if (mulib_json_typeof (v) == MULIB_JSON_NUMBER && !strchr (text, '.'))
      {
        const char *exponent = strchr (text, 'e');
        if (!exponent)
          exponent = strchr (text, 'E');
        if (exponent)
          {
            /* Also recognizable as a float by common YAML 1.1 readers. */
            ok = mulib_strbuf_append_n (b, text, (size_t)(exponent - text))
                 && mulib_strbuf_append (b, ".0")
                 && mulib_strbuf_append (b, exponent);
            free (text);
            return ok;
          }
      }
    /* JSON permits raw DEL/C1/noncharacters that YAML source does not. Escape
     * them, plus Unicode line separators, without altering the DOM's bytes. */
    length = strlen (text);
    ok = 1;
    while (i < length)
      {
        uint32_t code = (unsigned char)text[i];
        size_t used = 1;
        if (code >= 128
            && !mulib_utf8_decode ((const unsigned char *)text + i, length - i,
                                   &code, &used))
          {
            ok = 0;
            break;
          }
        if ((code >= 0x7f && code <= 0x9f) || code == 0x2028 || code == 0x2029
            || code == 0xfffe || code == 0xffff)
          {
            static const char hex[] = "0123456789abcdef";
            char escaped[6] = { '\\',
                                'u',
                                hex[(code >> 12) & 15],
                                hex[(code >> 8) & 15],
                                hex[(code >> 4) & 15],
                                hex[code & 15] };
            if (!mulib_strbuf_append_n (b, text + start, i - start)
                || !mulib_strbuf_append_n (b, escaped, sizeof (escaped)))
              {
                ok = 0;
                break;
              }
            start = i + used;
          }
        i += used;
      }
    if (ok)
      ok = mulib_strbuf_append_n (b, text + start, length - start);
    if (ok && mulib_json_typeof (v) == MULIB_JSON_NUMBER && !strchr (text, '.')
        && !strchr (text, 'e') && !strchr (text, 'E')
        && (mulib_json_numeric (v) > 9007199254740991.0
            || mulib_json_numeric (v) < -9007199254740991.0))
      ok = mulib_strbuf_append (b, ".0");
    free (text);
    return ok;
  }
  static int
  mulib_yaml_out_ (mulib_yaml_state_ *s, mulib_strbuf_t *b,
                   const mulib_json_t *v, unsigned depth)
  {
    mulib_json_type type = mulib_json_typeof (v);
    size_t i, count = mulib_json_size (v);
    if (depth >= 128)
      return mulib_yaml_fail_ (s, 0, "YAML nesting exceeds 128");
    if ((type != MULIB_JSON_ARRAY && type != MULIB_JSON_OBJECT) || count == 0)
      return mulib_yaml_indent_ (b, depth) && mulib_yaml_scalar_out_ (b, v)
             && mulib_strbuf_putc (b, '\n');
    for (i = 0; i < count; ++i)
      {
        const mulib_json_t *child = mulib_json_at (v, i);
        mulib_json_type child_type = mulib_json_typeof (child);
        if ((child_type == MULIB_JSON_OBJECT || child_type == MULIB_JSON_ARRAY)
            && depth + 1 >= 128)
          return mulib_yaml_fail_ (s, 0, "YAML nesting exceeds 128");
        if (!mulib_yaml_indent_ (b, depth))
          return 0;
        if (type == MULIB_JSON_OBJECT)
          {
            size_t n;
            const char *key = mulib_json_key (v, i, &n);
            mulib_json_t *string = mulib_json_string_n (key, n);
            int ok = string && mulib_yaml_scalar_out_ (b, string)
                     && mulib_strbuf_putc (b, ':');
            mulib_json_destroy (string);
            if (!ok)
              return 0;
          }
        else if (!mulib_strbuf_putc (b, '-'))
          return 0;
        if ((child_type == MULIB_JSON_OBJECT || child_type == MULIB_JSON_ARRAY)
            && mulib_json_size (child))
          {
            if (!mulib_strbuf_putc (b, '\n')
                || !mulib_yaml_out_ (s, b, child, depth + 1))
              return 0;
          }
        else if (!mulib_strbuf_putc (b, ' ')
                 || !mulib_yaml_scalar_out_ (b, child)
                 || !mulib_strbuf_putc (b, '\n'))
          return 0;
      }
    return 1;
  }
  char *
  mulib_yaml_stringify (const mulib_json_t *value, mulib_yaml_error_t *error)
  {
    mulib_yaml_state_ s;
    mulib_strbuf_t b;
    memset (&s, 0, sizeof (s));
    s.source = "";
    s.error = error;
    if (error)
      memset (error, 0, sizeof (*error));
    mulib_strbuf_init (&b);
    if (!mulib_yaml_out_ (&s, &b, value, 0))
      {
        if (!s.failed)
          mulib_yaml_fail_ (&s, 0, "out of memory");
        mulib_strbuf_free (&b);
        return NULL;
      }
    return b.data;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
