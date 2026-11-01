/**
 * @file mulib-markdown.h
 * @brief Small, escaped Markdown-to-HTML renderer (not CommonMark).
 *
 * ATX headings, paragraphs, flat bullet/numbered lists, thematic rules and
 * triple-backtick fenced code; inline emphasis/strong, single-backtick code,
 * links, and punctuation escapes. HTML is always text. See the manual for
 * exact syntax and URL restrictions. Define MULIB_IMPL_MARKDOWN plus HTML,
 * XML, STRBUF and UTF8 switches in one implementation TU.
 */
#ifndef MULIB_MARKDOWN_H
#define MULIB_MARKDOWN_H
#include "mulib-html.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Render length bytes to a malloc-owned HTML fragment, or NULL on
   * error. Input must contain XML-valid UTF-8 without NUL. Unclosed code
   * fences fail; unmatched inline markers remain literal. No raw HTML, images,
   * nested lists, reference links, blockquotes, tables or full CommonMark
   * parsing. Optional output length excludes NUL and is zero on failure. */
  char *mulib_markdown_to_html (const char *source, size_t length,
                                size_t *output_length,
                                mulib_xml_error_t *error);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MARKDOWN_H */

#if defined(MULIB_IMPL_MARKDOWN) && !defined(MULIB_MARKDOWN_IMPLEMENTED)
#define MULIB_MARKDOWN_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_markdown_space_internal (char c)
  {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
  }
  static int
  mulib_markdown_punctuation_internal (char c)
  {
    return c && strchr ("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", c) != NULL;
  }
  static int
  mulib_markdown_literal_internal (mulib_strbuf_t *b, const char *s, size_t n)
  {
    return mulib_html_escape (b, s, n, 0);
  }
  static int
  mulib_markdown_scheme_internal (const char *s, size_t n, const char *allowed)
  {
    size_t i;
    if (strlen (allowed) != n)
      return 0;
    for (i = 0; i < n; i++)
      {
        char c = s[i];
        if (c >= 'A' && c <= 'Z')
          c = (char)(c - 'A' + 'a');
        if (c != allowed[i])
          return 0;
      }
    return 1;
  }
  static int
  mulib_markdown_url_internal (const char *s, size_t n)
  {
    size_t i, colon = n, first = n;
    if (!n)
      return 0;
    for (i = 0; i < n; i++)
      {
        unsigned char c = (unsigned char)s[i];
        if (c <= 32 || c == 127 || c == '\\' || c == '\'' || c == '"'
            || c == '<' || c == '>' || c == '`' || c == '(' || c == ')')
          return 0;
        if (colon == n && c == ':')
          colon = i;
        if (first == n && (c == '/' || c == '?' || c == '#'))
          first = i;
      }
    if (colon < first)
      return mulib_markdown_scheme_internal (s, colon, "http")
             || mulib_markdown_scheme_internal (s, colon, "https")
             || mulib_markdown_scheme_internal (s, colon, "mailto");
    return 1;
  }
  /* Consume one supported code span, an unsupported literal backtick run, or
   * an unmatched single backtick. All inline scanners use the same span
   * boundaries. */
  static size_t
  mulib_markdown_code_internal (const char *s, size_t n, size_t start,
                                size_t *close)
  {
    size_t j = start + 1;
    *close = n;
    while (j < n && s[j] == '`')
      j++;
    if (j > start + 1)
      return j;
    while (j < n)
      {
        size_t end;
        if (s[j] != '`')
          {
            j++;
            continue;
          }
        end = j + 1;
        while (end < n && s[end] == '`')
          end++;
        if (end - j == 1)
          {
            *close = j;
            return end;
          }
        j = end;
      }
    return start + 1;
  }
  static size_t
  mulib_markdown_label_close_internal (const char *s, size_t n, size_t start)
  {
    size_t j = start;
    while (j < n)
      {
        if (s[j] == '\\' && j + 1 < n
            && mulib_markdown_punctuation_internal (s[j + 1]))
          {
            j += 2;
            continue;
          }
        if (s[j] == '`')
          {
            size_t close;
            j = mulib_markdown_code_internal (s, n, j, &close);
            continue;
          }
        if (s[j] == ']')
          break;
        j++;
      }
    return j;
  }
  /* Identify the syntax even for blocked URLs, which render as one literal
   * span. */
  static int
  mulib_markdown_link_internal (const char *s, size_t n, size_t start,
                                size_t *label, size_t *end)
  {
    size_t j = mulib_markdown_label_close_internal (s, n, start + 1), k;
    *label = j;
    *end = n;
    if (j < n && j + 1 < n && s[j + 1] == '(')
      {
        k = j + 2;
        while (k < n && s[k] != ')')
          k++;
        if (k < n)
          {
            *end = k;
            return 1;
          }
      }
    return 0;
  }
  static size_t
  mulib_markdown_close_internal (const char *s, size_t n, size_t start, char c,
                                 size_t count, int links)
  {
    size_t j = start;
    while (j < n)
      {
        if (s[j] == '\\' && j + 1 < n
            && mulib_markdown_punctuation_internal (s[j + 1]))
          {
            j += 2;
            continue;
          }
        if (s[j] == '`')
          {
            size_t close;
            j = mulib_markdown_code_internal (s, n, j, &close);
            continue;
          }
        if (s[j] == '[' && links && (!j || s[j - 1] != '!'))
          {
            size_t label, end;
            if (mulib_markdown_link_internal (s, n, j, &label, &end))
              {
                j = end + 1;
                continue;
              }
          }
        if (s[j] == c)
          {
            size_t k = j;
            while (k < n && s[k] == c)
              k++;
            if (k - j == count && j > start
                && !mulib_markdown_space_internal (s[j - 1]))
              return j;
            j = k;
          }
        else
          j++;
      }
    return n;
  }
  static int
  mulib_markdown_inline_internal (mulib_strbuf_t *b, const char *s, size_t n,
                                  unsigned depth, int links)
  {
    size_t i = 0;
    if (depth > 32)
      return 0;
    while (i < n)
      {
        char c = s[i];
        if (c == '\\' && i + 1 < n
            && mulib_markdown_punctuation_internal (s[i + 1]))
          {
            if (!mulib_markdown_literal_internal (b, s + i + 1, 1))
              return 0;
            i += 2;
            continue;
          }
        if (c == '`')
          {
            size_t close, end = mulib_markdown_code_internal (s, n, i, &close);
            if (close < n)
              {
                if (!mulib_strbuf_append (b, "<code>")
                    || !mulib_markdown_literal_internal (b, s + i + 1,
                                                         close - i - 1)
                    || !mulib_strbuf_append (b, "</code>"))
                  return 0;
                i = end;
                continue;
              }
            if (end > i + 1)
              {
                if (!mulib_markdown_literal_internal (b, s + i, end - i))
                  return 0;
                i = end;
                continue;
              }
          }
        if (c == '*' || c == '_')
          {
            size_t run = 1, j;
            while (i + run < n && s[i + run] == c)
              run++;
            if (run > 2)
              {
                if (!mulib_markdown_literal_internal (b, s + i, run))
                  return 0;
                i += run;
                continue;
              }
            if (i + run < n && !mulib_markdown_space_internal (s[i + run]))
              {
                j = mulib_markdown_close_internal (s, n, i + run, c, run,
                                                   links);
                if (j < n)
                  {
                    if (!mulib_strbuf_append (b,
                                              run == 2 ? "<strong>" : "<em>")
                        || !mulib_markdown_inline_internal (
                            b, s + i + run, j - i - run, depth + 1, links)
                        || !mulib_strbuf_append (b, run == 2 ? "</strong>"
                                                             : "</em>"))
                      return 0;
                    i = j + run;
                    continue;
                  }
              }
          }
        if (c == '[' && links && (!i || s[i - 1] != '!'))
          {
            size_t j, k;
            if (mulib_markdown_link_internal (s, n, i, &j, &k))
              {
                if (mulib_markdown_url_internal (s + j + 2, k - j - 2))
                  {
                    if (!mulib_strbuf_append (b, "<a href=\"")
                        || !mulib_html_escape (b, s + j + 2, k - j - 2, 1)
                        || !mulib_strbuf_append (b, "\">")
                        || !mulib_markdown_inline_internal (
                            b, s + i + 1, j - i - 1, depth + 1, 0)
                        || !mulib_strbuf_append (b, "</a>"))
                      return 0;
                  }
                else if (!mulib_markdown_literal_internal (b, s + i,
                                                           k - i + 1))
                  return 0;
                i = k + 1;
                continue;
              }
            if (j == n)
              links = 0;
          }
        if (c == '\\' || c == '`' || c == '*' || c == '_' || c == '[')
          {
            if (!mulib_markdown_literal_internal (b, s + i, 1))
              return 0;
            i++;
          }
        else
          {
            size_t begin = i++;
            while (i < n && s[i] != '\\' && s[i] != '`' && s[i] != '*'
                   && s[i] != '_' && s[i] != '[')
              i++;
            if (!mulib_markdown_literal_internal (b, s + begin, i - begin))
              return 0;
          }
      }
    return 1;
  }
  typedef struct mulib_markdown_line_internal
  {
    size_t start, length, next;
  } mulib_markdown_line_internal;
  static mulib_markdown_line_internal
  mulib_markdown_line_at_internal (const char *s, size_t n, size_t p)
  {
    mulib_markdown_line_internal l;
    l.start = p;
    while (p < n && s[p] != '\n' && s[p] != '\r')
      p++;
    l.length = p - l.start;
    if (p < n)
      {
        char c = s[p++];
        if (c == '\r' && p < n && s[p] == '\n')
          p++;
      }
    l.next = p;
    return l;
  }
  static int
  mulib_markdown_flush_internal (mulib_strbuf_t *out,
                                 mulib_strbuf_t *paragraph)
  {
    if (!paragraph->length)
      return 1;
    if (!mulib_strbuf_append (out, "<p>")
        || !mulib_markdown_inline_internal (out, paragraph->data,
                                            paragraph->length, 0, 1)
        || !mulib_strbuf_append (out, "</p>\n"))
      return 0;
    paragraph->length = 0;
    paragraph->data[0] = 0;
    return 1;
  }
  static int
  mulib_markdown_list_close_internal (mulib_strbuf_t *out, int *list)
  {
    if (*list
        && !mulib_strbuf_append (out, *list == 1 ? "</ul>\n" : "</ol>\n"))
      return 0;
    *list = 0;
    return 1;
  }
  static int
  mulib_markdown_fence_internal (const char *s, size_t n, size_t *language,
                                 size_t *language_length)
  {
    size_t p = 3, begin;
    if (n < 3 || memcmp (s, "```", 3))
      return 0;
    while (p < n && (s[p] == ' ' || s[p] == '\t'))
      p++;
    begin = p;
    while (p < n
           && ((s[p] >= 'a' && s[p] <= 'z') || (s[p] >= 'A' && s[p] <= 'Z')
               || (s[p] >= '0' && s[p] <= '9') || s[p] == '_' || s[p] == '-'))
      p++;
    *language = begin;
    *language_length = p - begin;
    while (p < n && (s[p] == ' ' || s[p] == '\t'))
      p++;
    return p == n;
  }
  char *
  mulib_markdown_to_html (const char *s, size_t n, size_t *length,
                          mulib_xml_error_t *error)
  {
    mulib_strbuf_t out, paragraph;
    size_t p = 0;
    int list = 0;
    mulib_strbuf_init (&out);
    mulib_strbuf_init (&paragraph);
    if (length)
      *length = 0;
    if (error)
      {
        error->offset = 0;
        error->message = NULL;
      }
    if (!mulib_xml_valid_text (s, n))
      {
        if (error)
          error->message = "invalid UTF-8/XML character";
        return NULL;
      }
    while (p < n)
      {
        mulib_markdown_line_internal l
            = mulib_markdown_line_at_internal (s, n, p);
        const char *line = s + l.start;
        size_t end = l.length, start = 0, language, language_length,
               content = 0;
        int kind = 0, heading = 0;
        while (end && mulib_markdown_space_internal (line[end - 1]))
          end--;
        while (start < end && mulib_markdown_space_internal (line[start]))
          start++;
        if (start == end)
          {
            if (!mulib_markdown_flush_internal (&out, &paragraph)
                || !mulib_markdown_list_close_internal (&out, &list))
              goto allocation_failure;
            p = l.next;
            continue;
          }
        if (mulib_markdown_fence_internal (line, l.length, &language,
                                           &language_length))
          {
            int closed = 0;
            if (!mulib_markdown_flush_internal (&out, &paragraph)
                || !mulib_markdown_list_close_internal (&out, &list)
                || !mulib_strbuf_append (&out, "<pre><code"))
              goto allocation_failure;
            if (language_length
                && (!mulib_strbuf_append (&out, " class=\"language-")
                    || !mulib_html_escape (&out, line + language,
                                           language_length, 1)
                    || !mulib_strbuf_putc (&out, '"')))
              goto allocation_failure;
            if (!mulib_strbuf_putc (&out, '>'))
              goto allocation_failure;
            p = l.next;
            while (p < n)
              {
                size_t lang, langn;
                l = mulib_markdown_line_at_internal (s, n, p);
                line = s + l.start;
                if (mulib_markdown_fence_internal (line, l.length, &lang,
                                                   &langn)
                    && !langn)
                  {
                    closed = 1;
                    p = l.next;
                    break;
                  }
                if (!mulib_markdown_literal_internal (&out, line, l.length)
                    || !mulib_strbuf_putc (&out, '\n'))
                  goto allocation_failure;
                p = l.next;
              }
            if (!closed)
              {
                if (error)
                  {
                    error->offset = p;
                    error->message = "unclosed triple-backtick fence";
                  }
                goto fail;
              }
            if (!mulib_strbuf_append (&out, "</code></pre>\n"))
              goto allocation_failure;
            continue;
          }
        while ((size_t)heading < end && line[heading] == '#' && heading < 7)
          heading++;
        if (heading >= 1 && heading <= 6 && (size_t)heading < l.length
            && (line[heading] == ' ' || line[heading] == '\t'))
          {
            char tag[5] = { '<', 'h', (char)('0' + heading), '>', 0 };
            content = (size_t)heading;
            while (content < end
                   && mulib_markdown_space_internal (line[content]))
              content++;
            if (!mulib_markdown_flush_internal (&out, &paragraph)
                || !mulib_markdown_list_close_internal (&out, &list)
                || !mulib_strbuf_append (&out, tag)
                || !mulib_markdown_inline_internal (&out, line + content,
                                                    end - content, 0, 1))
              goto allocation_failure;
            {
              char close[7]
                  = { '<', '/', 'h', (char)('0' + heading), '>', '\n', 0 };
              if (!mulib_strbuf_append (&out, close))
                goto allocation_failure;
            }
            p = l.next;
            continue;
          }
        if (end == 3 && (!memcmp (line, "---", 3) || !memcmp (line, "***", 3)))
          {
            if (!mulib_markdown_flush_internal (&out, &paragraph)
                || !mulib_markdown_list_close_internal (&out, &list)
                || !mulib_strbuf_append (&out, "<hr>\n"))
              goto allocation_failure;
            p = l.next;
            continue;
          }
        if (l.length >= 2
            && (line[0] == '-' || line[0] == '*' || line[0] == '+')
            && (line[1] == ' ' || line[1] == '\t'))
          {
            kind = 1;
            content = 2;
          }
        else
          {
            size_t k = 0;
            while (k < end && line[k] >= '0' && line[k] <= '9')
              k++;
            if (k && k + 1 < l.length && line[k] == '.'
                && (line[k + 1] == ' ' || line[k + 1] == '\t'))
              {
                kind = 2;
                content = k + 2;
              }
          }
        if (kind)
          {
            while (content < end
                   && mulib_markdown_space_internal (line[content]))
              content++;
            if (content > end)
              content = end;
            if (!mulib_markdown_flush_internal (&out, &paragraph))
              goto allocation_failure;
            if (list != kind)
              {
                if (!mulib_markdown_list_close_internal (&out, &list)
                    || !mulib_strbuf_append (&out,
                                             kind == 1 ? "<ul>\n" : "<ol>\n"))
                  goto allocation_failure;
                list = kind;
              }
            if (!mulib_strbuf_append (&out, "<li>")
                || !mulib_markdown_inline_internal (&out, line + content,
                                                    end - content, 0, 1)
                || !mulib_strbuf_append (&out, "</li>\n"))
              goto allocation_failure;
          }
        else
          {
            if (!mulib_markdown_list_close_internal (&out, &list)
                || (paragraph.length && !mulib_strbuf_putc (&paragraph, ' '))
                || !mulib_strbuf_append_n (&paragraph, line + start,
                                           end - start))
              goto allocation_failure;
          }
        p = l.next;
      }
    if (!mulib_markdown_flush_internal (&out, &paragraph)
        || !mulib_markdown_list_close_internal (&out, &list)
        || !mulib_strbuf_append_n (&out, "", 0))
      goto allocation_failure;
    mulib_strbuf_free (&paragraph);
    if (length)
      *length = out.length;
    return out.data;
  allocation_failure:
    if (error)
      {
        error->offset = p;
        error->message = "allocation failure or inline nesting limit (32)";
      }
  fail:
    mulib_strbuf_free (&out);
    mulib_strbuf_free (&paragraph);
    return NULL;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
