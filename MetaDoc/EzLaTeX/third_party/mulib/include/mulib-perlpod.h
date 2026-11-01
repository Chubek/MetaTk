/**
 * @file mulib-perlpod.h
 * @brief Strict focused POD-to-HTML/plain-text renderer; not a Perl
 * interpreter. Define MULIB_IMPL_PERLPOD once, plus STRBUF and UTF8
 * implementations.
 */
#ifndef MULIB_PERLPOD_H
#define MULIB_PERLPOD_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum mulib_perlpod_format
  {
    MULIB_PERLPOD_TEXT,
    MULIB_PERLPOD_HTML
  } mulib_perlpod_format_t;
  typedef enum mulib_perlpod_status
  {
    MULIB_PERLPOD_OK,
    MULIB_PERLPOD_INVALID,
    MULIB_PERLPOD_SYNTAX,
    MULIB_PERLPOD_UNSUPPORTED,
    MULIB_PERLPOD_LIMIT,
    MULIB_PERLPOD_MEMORY
  } mulib_perlpod_status_t;
  typedef struct mulib_perlpod_error
  {
    mulib_perlpod_status_t code;
    size_t offset, line, column;
    const char *message;
  } mulib_perlpod_error_t;
  /** @brief Hard limits; NULL selects 1 MiB input, 4 MiB output, nesting 32
   * (max 64). */
  typedef struct mulib_perlpod_limits
  {
    size_t input_bytes, output_bytes, depth;
  } mulib_perlpod_limits_t;
  /** @brief Render an owned NUL-terminated fragment; free with free().
   * Supports =pod/=cut, =head1..4, =over [1..16], =item, =back, paragraphs,
   * verbatim lines, B/I/C/F/S and E<lt|gt|sol|verbar|amp|quot|apos>.
   * Unknown directives/format codes are errors, never silently discarded.
   * length is zero on failure; error is cleared on success. NULL/0 is empty.
   */
  char *mulib_perlpod_render (const char *source, size_t source_length,
                              mulib_perlpod_format_t format,
                              const mulib_perlpod_limits_t *limits,
                              size_t *length, mulib_perlpod_error_t *error);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_PERLPOD) && !defined(MULIB_PERLPOD_IMPLEMENTED)
#define MULIB_PERLPOD_IMPLEMENTED
#include "mulib-strbuf.h"
#include "mulib-utf8.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_perlpod_state_
  {
    mulib_strbuf_t output;
    mulib_perlpod_limits_t limits;
    mulib_perlpod_error_t error;
    int html;
    size_t at;
  } mulib_perlpod_state_;
  static int
  mulib_perlpod_fail_ (mulib_perlpod_state_ *p, mulib_perlpod_status_t code,
                       const char *s)
  {
    if (!p->error.code)
      {
        p->error.code = code;
        p->error.offset = p->at;
        p->error.message = s;
      }
    return 0;
  }
  static int
  mulib_perlpod_put_ (mulib_perlpod_state_ *p, const char *s, size_t n)
  {
    if (n > p->limits.output_bytes - p->output.length)
      return mulib_perlpod_fail_ (p, MULIB_PERLPOD_LIMIT, "output byte limit");
    if (!mulib_strbuf_append_n (&p->output, s, n))
      return mulib_perlpod_fail_ (p, MULIB_PERLPOD_MEMORY,
                                  "output allocation failed");
    return 1;
  }
  static int
  mulib_perlpod_literal_ (mulib_perlpod_state_ *p, const char *s)
  {
    return mulib_perlpod_put_ (p, s, strlen (s));
  }
  static int
  mulib_perlpod_byte_ (mulib_perlpod_state_ *p, char c)
  {
    if (p->html)
      {
        if (c == '&')
          return mulib_perlpod_literal_ (p, "&amp;");
        if (c == '<')
          return mulib_perlpod_literal_ (p, "&lt;");
        if (c == '>')
          return mulib_perlpod_literal_ (p, "&gt;");
      }
    return mulib_perlpod_put_ (p, &c, 1);
  }
  static int
  mulib_perlpod_inline_ (mulib_perlpod_state_ *p, const char *s, size_t n,
                         size_t *index, size_t base, size_t depth, int closing)
  {
    while (*index < n)
      {
        size_t at = (*index)++;
        unsigned char c = (unsigned char)s[at];
        p->at = base + at;
        if (c == '>' && closing)
          return 1;
        if (c >= 'A' && c <= 'Z' && *index < n && s[*index] == '<')
          {
            const char *open, *close;
            ++*index;
            if (depth == p->limits.depth)
              return mulib_perlpod_fail_ (p, MULIB_PERLPOD_LIMIT,
                                          "format nesting limit");
            if (*index < n && s[*index] == '<')
              return mulib_perlpod_fail_ (
                  p, MULIB_PERLPOD_UNSUPPORTED,
                  "multi-angle formatting unsupported");
            if (c == 'E')
              {
                static const char *const names[]
                    = { "lt", "gt", "sol", "verbar", "amp", "quot", "apos" };
                static const char values[] = "<>/|&\"'";
                size_t begin = *index, i;
                while (*index < n && s[*index] != '>')
                  ++*index;
                if (*index == n)
                  return mulib_perlpod_fail_ (p, MULIB_PERLPOD_SYNTAX,
                                              "unclosed entity");
                for (i = 0; i < sizeof names / sizeof *names; ++i)
                  if (*index - begin == strlen (names[i])
                      && !memcmp (s + begin, names[i], *index - begin))
                    break;
                if (i == sizeof names / sizeof *names)
                  return mulib_perlpod_fail_ (p, MULIB_PERLPOD_UNSUPPORTED,
                                              "unsupported entity");
                ++*index;
                if (!mulib_perlpod_byte_ (p, values[i]))
                  return 0;
                continue;
              }
            switch (c)
              {
              case 'B':
                open = "<strong>";
                close = "</strong>";
                break;
              case 'I':
              case 'F':
                open = "<em>";
                close = "</em>";
                break;
              case 'C':
                open = "<code>";
                close = "</code>";
                break;
              case 'S':
                open = "<span style=\"white-space:nowrap\">";
                close = "</span>";
                break;
              default:
                return mulib_perlpod_fail_ (p, MULIB_PERLPOD_UNSUPPORTED,
                                            "unsupported formatting code");
              }
            if (p->html && !mulib_perlpod_literal_ (p, open))
              return 0;
            if (!mulib_perlpod_inline_ (p, s, n, index, base, depth + 1, 1))
              return 0;
            if (p->html && !mulib_perlpod_literal_ (p, close))
              return 0;
          }
        else if (!mulib_perlpod_byte_ (p, (char)c))
          return 0;
      }
    if (closing)
      return mulib_perlpod_fail_ (p, MULIB_PERLPOD_SYNTAX,
                                  "unclosed formatting code");
    return 1;
  }
  static int
  mulib_perlpod_text_ (mulib_perlpod_state_ *p, const char *s, size_t n,
                       size_t base)
  {
    size_t i = 0;
    return mulib_perlpod_inline_ (p, s, n, &i, base, 0, 0);
  }
  static int
  mulib_perlpod_close_ (mulib_perlpod_state_ *p, int *paragraph, int *verbatim)
  {
    if (*paragraph && !mulib_perlpod_literal_ (p, p->html ? "</p>\n" : "\n\n"))
      return 0;
    if (*verbatim && !mulib_perlpod_literal_ (p, p->html ? "</pre>\n" : "\n"))
      return 0;
    *paragraph = *verbatim = 0;
    return 1;
  }
  static int
  mulib_perlpod_equal_ (const char *s, size_t n, const char *word)
  {
    return n == strlen (word) && !memcmp (s, word, n);
  }
  char *
  mulib_perlpod_render (const char *s, size_t n, mulib_perlpod_format_t format,
                        const mulib_perlpod_limits_t *limits, size_t *length,
                        mulib_perlpod_error_t *error)
  {
    mulib_perlpod_state_ p;
    size_t pos = 0, list_depth = 0, i;
    int active = 0, paragraph = 0, verbatim = 0, items[64] = { 0 };
    memset (&p, 0, sizeof p);
    p.html = format == MULIB_PERLPOD_HTML;
    p.limits.input_bytes = 1048576;
    p.limits.output_bytes = 4194304;
    p.limits.depth = 32;
    if (limits)
      p.limits = *limits;
    if (length)
      *length = 0;
    if ((!s && n)
        || (format != MULIB_PERLPOD_TEXT && format != MULIB_PERLPOD_HTML)
        || p.limits.depth > 64)
      {
        mulib_perlpod_fail_ (&p, MULIB_PERLPOD_INVALID, "invalid arguments");
        goto done;
      }
    if (n > p.limits.input_bytes)
      {
        mulib_perlpod_fail_ (&p, MULIB_PERLPOD_LIMIT, "input byte limit");
        goto done;
      }
    if (!mulib_utf8_validate ((const unsigned char *)s, n))
      {
        mulib_perlpod_fail_ (&p, MULIB_PERLPOD_SYNTAX, "invalid UTF-8");
        goto done;
      }
    for (i = 0; i < n; ++i)
      if ((unsigned char)s[i] < 32 && s[i] != '\n' && s[i] != '\r'
          && s[i] != '\t')
        {
          p.at = i;
          mulib_perlpod_fail_ (&p, MULIB_PERLPOD_SYNTAX,
                               "control byte in source");
          goto done;
        }
    while (pos < n)
      {
        size_t begin = pos, end, cmd_end, arg;
        int blank = 1;
        while (pos < n && s[pos] != '\n' && s[pos] != '\r')
          ++pos;
        end = pos;
        if (pos < n && s[pos++] == '\r' && pos < n && s[pos] == '\n')
          ++pos;
        p.at = begin;
        for (i = begin; i < end; ++i)
          if (s[i] != ' ' && s[i] != '\t')
            blank = 0;
        if (begin < end && s[begin] == '=')
          {
            const char *cmd = s + begin + 1;
            size_t cmd_len;
            if (!mulib_perlpod_close_ (&p, &paragraph, &verbatim))
              goto done;
            cmd_end = begin + 1;
            while (cmd_end < end && s[cmd_end] != ' ' && s[cmd_end] != '\t')
              ++cmd_end;
            cmd_len = cmd_end - begin - 1;
            arg = cmd_end;
            while (arg < end && (s[arg] == ' ' || s[arg] == '\t'))
              ++arg;
            while (end > arg && (s[end - 1] == ' ' || s[end - 1] == '\t'))
              --end;
            if (mulib_perlpod_equal_ (cmd, cmd_len, "pod")
                || mulib_perlpod_equal_ (cmd, cmd_len, "cut"))
              {
                if (arg != end || list_depth)
                  {
                    mulib_perlpod_fail_ (
                        &p, MULIB_PERLPOD_SYNTAX,
                        "pod/cut require no arguments or open lists");
                    goto done;
                  }
                active = cmd[0] == 'p';
              }
            else if (cmd_len == 5 && !memcmp (cmd, "head", 4) && cmd[4] >= '1'
                     && cmd[4] <= '4')
              {
                char open[] = "<h1>", close[] = "</h1>\n";
                if (list_depth || arg == end)
                  {
                    mulib_perlpod_fail_ (
                        &p, MULIB_PERLPOD_SYNTAX,
                        "heading requires text outside lists");
                    goto done;
                  }
                active = 1;
                open[2] = close[3] = cmd[4];
                if ((p.html && !mulib_perlpod_literal_ (&p, open))
                    || !mulib_perlpod_text_ (&p, s + arg, end - arg, arg)
                    || !mulib_perlpod_literal_ (&p, p.html ? close : "\n\n"))
                  goto done;
              }
            else if (mulib_perlpod_equal_ (cmd, cmd_len, "over"))
              {
                unsigned indent = 0;
                if (arg != end)
                  {
                    for (i = arg; i < end; ++i)
                      {
                        if (s[i] < '0' || s[i] > '9' || indent > 16)
                          {
                            mulib_perlpod_fail_ (
                                &p, MULIB_PERLPOD_SYNTAX,
                                "over indentation must be 1..16");
                            goto done;
                          }
                        indent = indent * 10 + (unsigned)(s[i] - '0');
                      }
                    if (!indent || indent > 16)
                      {
                        mulib_perlpod_fail_ (&p, MULIB_PERLPOD_SYNTAX,
                                             "over indentation must be 1..16");
                        goto done;
                      }
                  }
                if (list_depth == p.limits.depth)
                  {
                    mulib_perlpod_fail_ (&p, MULIB_PERLPOD_LIMIT,
                                         "list nesting limit");
                    goto done;
                  }
                if (list_depth && !items[list_depth - 1])
                  {
                    mulib_perlpod_fail_ (&p, MULIB_PERLPOD_SYNTAX,
                                         "nested list requires item");
                    goto done;
                  }
                active = 1;
                items[list_depth++] = 0;
                if (p.html && !mulib_perlpod_literal_ (&p, "<ul>\n"))
                  goto done;
              }
            else if (mulib_perlpod_equal_ (cmd, cmd_len, "item"))
              {
                if (!active || !list_depth)
                  {
                    mulib_perlpod_fail_ (&p, MULIB_PERLPOD_SYNTAX,
                                         "item outside list");
                    goto done;
                  }
                if (items[list_depth - 1] && p.html
                    && !mulib_perlpod_literal_ (&p, "</li>\n"))
                  goto done;
                items[list_depth - 1] = 1;
                if (!mulib_perlpod_literal_ (&p, p.html ? "<li>" : "* "))
                  goto done;
                if (!(end - arg == 1 && s[arg] == '*')
                    && !mulib_perlpod_text_ (&p, s + arg, end - arg, arg))
                  goto done;
                if (!mulib_perlpod_literal_ (&p, "\n"))
                  goto done;
              }
            else if (mulib_perlpod_equal_ (cmd, cmd_len, "back"))
              {
                if (!active || !list_depth || arg != end)
                  {
                    mulib_perlpod_fail_ (&p, MULIB_PERLPOD_SYNTAX,
                                         "unmatched back or arguments");
                    goto done;
                  }
                --list_depth;
                if (p.html
                    && ((!items[list_depth]
                             ? 0
                             : !mulib_perlpod_literal_ (&p, "</li>\n"))
                        || !mulib_perlpod_literal_ (&p, "</ul>\n")))
                  goto done;
              }
            else
              {
                mulib_perlpod_fail_ (&p, MULIB_PERLPOD_UNSUPPORTED,
                                     "unsupported POD directive");
                goto done;
              }
            continue;
          }
        if (!active)
          continue;
        if (blank)
          {
            if (!mulib_perlpod_close_ (&p, &paragraph, &verbatim))
              goto done;
            continue;
          }
        if (list_depth && !items[list_depth - 1])
          {
            mulib_perlpod_fail_ (&p, MULIB_PERLPOD_SYNTAX,
                                 "list content before item");
            goto done;
          }
        if (s[begin] == ' ' || s[begin] == '\t')
          {
            if (paragraph && !mulib_perlpod_close_ (&p, &paragraph, &verbatim))
              goto done;
            if (!verbatim && p.html && !mulib_perlpod_literal_ (&p, "<pre>"))
              goto done;
            verbatim = 1;
            for (i = begin; i < end; ++i)
              if (!mulib_perlpod_byte_ (&p, s[i]))
                goto done;
            if (!mulib_perlpod_literal_ (&p, "\n"))
              goto done;
          }
        else
          {
            if (verbatim && !mulib_perlpod_close_ (&p, &paragraph, &verbatim))
              goto done;
            if (!mulib_perlpod_literal_ (&p, paragraph ? " "
                                             : p.html  ? "<p>"
                                                       : ""))
              goto done;
            paragraph = 1;
            if (!mulib_perlpod_text_ (&p, s + begin, end - begin, begin))
              goto done;
          }
      }
    p.at = n;
    if (list_depth)
      {
        mulib_perlpod_fail_ (&p, MULIB_PERLPOD_SYNTAX, "unclosed over list");
        goto done;
      }
    if (!mulib_perlpod_close_ (&p, &paragraph, &verbatim)
        || !mulib_perlpod_literal_ (&p, ""))
      goto done;
  done:
    if (p.error.code)
      {
        p.error.line = p.error.column = 1;
        for (i = 0; i < p.error.offset; ++i)
          {
            if (s[i] == '\n')
              {
                ++p.error.line;
                p.error.column = 1;
              }
            else
              ++p.error.column;
          }
        mulib_strbuf_free (&p.output);
      }
    else if (length)
      *length = p.output.length;
    if (error)
      *error = p.error;
    return p.error.code ? NULL : p.output.data;
  }
#ifdef __cplusplus
}
#endif
#endif
