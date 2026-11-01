/**
 * @file mulib-terminfo.h
 * @brief Compiled terminfo core-table reader and bounded numeric expansion.
 *
 * Reads legacy and 32-bit-number compiled records, in either byte order. Named
 * lookup covers common standard capabilities; indexed lookup covers every core
 * entry. Extended ncurses tables, source parsing, database search, padding
 * delays and string-valued tparm arguments are not implemented. Padding
 * annotations are stripped on expansion without sleeping. The explicit ANSI
 * fallback is a capability profile, not a database reader. See
 * manual/mulib-terminfo.md. Define MULIB_IMPL_TERMINFO and MULIB_IMPL_STRBUF
 * in one translation unit.
 */
#ifndef MULIB_TERMINFO_H
#define MULIB_TERMINFO_H
#include "mulib-strbuf.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Owned record. Initialize before loading; treat members as
   * read-only. */
  typedef struct mulib_terminfo
  {
    unsigned char *data;
    size_t length, names_size, bool_count, num_count, str_count;
    size_t bool_offset, num_offset, str_offset, table_offset, table_size;
    unsigned number_width, big_endian, ansi;
  } mulib_terminfo_t;

  /** @brief Initialize an empty record; does not release an existing record.
   */
  void mulib_terminfo_init (mulib_terminfo_t *info);
  /** @brief Release a record and zero it. NULL is allowed. */
  void mulib_terminfo_free (mulib_terminfo_t *info);
  /** @brief Copy a validated compiled core record. Failure leaves info
   * unchanged. */
  int mulib_terminfo_load (mulib_terminfo_t *info, const void *data,
                           size_t length);
  /** @brief Load an explicit binary file (at most 1 MiB); no implicit path
   * search. */
  int mulib_terminfo_load_file (mulib_terminfo_t *info, const char *path);
  /** @brief Replace a record with the allocation-free ANSI/256-color profile.
   */
  void mulib_terminfo_ansi (mulib_terminfo_t *info);
  /** @brief Borrow the NUL-terminated name/alias field, or NULL for an empty
   * object. */
  const char *mulib_terminfo_names (const mulib_terminfo_t *info);
  /** @brief Indexed boolean: 0/1, or -1 for absent, cancelled or out-of-range.
   */
  int mulib_terminfo_bool_at (const mulib_terminfo_t *info, size_t index);
  /** @brief Indexed number; -1 means absent, cancelled or out-of-range. */
  int32_t mulib_terminfo_num_at (const mulib_terminfo_t *info, size_t index);
  /** @brief Borrow an indexed string; NULL means
   * absent/cancelled/out-of-range. */
  const char *mulib_terminfo_str_at (const mulib_terminfo_t *info,
                                     size_t index);
  /** @brief Lookup a supported short boolean name; unknown names return -1. */
  int mulib_terminfo_bool (const mulib_terminfo_t *info, const char *name);
  /** @brief Lookup a supported short numeric name; unknown names return -1. */
  int32_t mulib_terminfo_num (const mulib_terminfo_t *info, const char *name);
  /** @brief Borrow a string by supported short name; unknown names return
   * NULL. */
  const char *mulib_terminfo_str (const mulib_terminfo_t *info,
                                  const char *name);
  /**
   * @brief Append numeric tparm-subset expansion; return 1/0, atomically.
   * params is nine int32_t values, or NULL for zeroes. Supports arithmetic,
   * conditionals, local variables and bounded numeric formatting, not strings.
   * Unsupported syntax, overflow, stack errors and output over 64 KiB fail.
   */
  int mulib_terminfo_expand (const char *format, const int32_t params[9],
                             mulib_strbuf_t *output);
  /** @brief Lookup and expand a capability; missing/unsupported returns 0. */
  int mulib_terminfo_emit (const mulib_terminfo_t *info, const char *name,
                           const int32_t params[9], mulib_strbuf_t *output);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_TERMINFO_H */

#if defined(MULIB_IMPL_TERMINFO) && !defined(MULIB_TERMINFO_IMPLEMENTED)
#define MULIB_TERMINFO_IMPLEMENTED
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_terminfo_name_
  {
    const char *name;
    size_t index;
    const char *ansi;
  } mulib_terminfo_name_;

  static const mulib_terminfo_name_ mulib_terminfo_bools_[]
      = { { "bw", 0, NULL },   { "am", 1, NULL },   { "xenl", 4, NULL },
          { "km", 8, NULL },   { "mir", 13, NULL }, { "msgr", 14, NULL },
          { "xon", 20, NULL }, { "npc", 25, NULL }, { "bce", 28, NULL } };
  static const mulib_terminfo_name_ mulib_terminfo_nums_[]
      = { { "cols", 0, NULL },    { "it", 1, NULL },     { "lines", 2, NULL },
          { "colors", 13, NULL }, { "pairs", 14, NULL }, { "ncv", 15, NULL } };
  static const mulib_terminfo_name_ mulib_terminfo_strings_[]
      = { { "cbt", 0, "\033[Z" },
          { "bel", 1, "\007" },
          { "cr", 2, "\r" },
          { "csr", 3, "\033[%i%p1%d;%p2%dr" },
          { "tbc", 4, "\033[3g" },
          { "clear", 5, "\033[H\033[2J" },
          { "el", 6, "\033[K" },
          { "ed", 7, "\033[J" },
          { "hpa", 8, "\033[%i%p1%dG" },
          { "cup", 10, "\033[%i%p1%d;%p2%dH" },
          { "cud1", 11, "\n" },
          { "home", 12, "\033[H" },
          { "civis", 13, "\033[?25l" },
          { "cub1", 14, "\b" },
          { "cnorm", 16, "\033[?25h" },
          { "cuf1", 17, "\033[C" },
          { "cuu1", 19, "\033[A" },
          { "cvvis", 20, "\033[?25h" },
          { "dch1", 21, "\033[P" },
          { "dl1", 22, "\033[M" },
          { "blink", 26, "\033[5m" },
          { "bold", 27, "\033[1m" },
          { "smcup", 28, "\033[?1049h" },
          { "dim", 30, "\033[2m" },
          { "smir", 31, "\033[4h" },
          { "invis", 32, "\033[8m" },
          { "rev", 34, "\033[7m" },
          { "smso", 35, "\033[7m" },
          { "smul", 36, "\033[4m" },
          { "ech", 37, "\033[%p1%dX" },
          { "sgr0", 39, "\033[0m" },
          { "rmcup", 40, "\033[?1049l" },
          { "rmir", 42, "\033[4l" },
          { "rmso", 43, "\033[27m" },
          { "rmul", 44, "\033[24m" },
          { "ich1", 52, "\033[@" },
          { "il1", 53, "\033[L" },
          { "kbs", 55, "\177" },
          { "kdch1", 59, "\033[3~" },
          { "kcud1", 61, "\033[B" },
          { "kf1", 66, "\033OP" },
          { "kf10", 67, "\033[21~" },
          { "kf2", 68, "\033OQ" },
          { "kf3", 69, "\033OR" },
          { "kf4", 70, "\033OS" },
          { "kf5", 71, "\033[15~" },
          { "kf6", 72, "\033[17~" },
          { "kf7", 73, "\033[18~" },
          { "kf8", 74, "\033[19~" },
          { "kf9", 75, "\033[20~" },
          { "khome", 76, "\033[H" },
          { "kich1", 77, "\033[2~" },
          { "kcub1", 79, "\033[D" },
          { "knp", 81, "\033[6~" },
          { "kpp", 82, "\033[5~" },
          { "kcuf1", 83, "\033[C" },
          { "kcuu1", 87, "\033[A" },
          { "rmkx", 88, "\033[?1l\033>" },
          { "smkx", 89, "\033[?1h\033=" },
          { "dch", 105, "\033[%p1%dP" },
          { "dl", 106, "\033[%p1%dM" },
          { "cud", 107, "\033[%p1%dB" },
          { "ich", 108, "\033[%p1%d@" },
          { "il", 110, "\033[%p1%dL" },
          { "cub", 111, "\033[%p1%dD" },
          { "cuf", 112, "\033[%p1%dC" },
          { "cuu", 114, "\033[%p1%dA" },
          { "rc", 126, "\0338" },
          { "vpa", 127, "\033[%i%p1%dd" },
          { "sc", 128, "\0337" },
          { "ind", 129, "\n" },
          { "ri", 130, "\033M" },
          { "hts", 132, "\033H" },
          { "ht", 134, "\t" },
          { "kcbt", 148, "\033[Z" },
          { "smam", 151, "\033[?7h" },
          { "rmam", 152, "\033[?7l" },
          { "kend", 164, "\033[F" },
          { "kent", 165, "\r" },
          { "kf11", 216, "\033[23~" },
          { "kf12", 217, "\033[24~" },
          { "el1", 269, "\033[1K" },
          { "op", 297, "\033[39;49m" },
          { "sitm", 311, "\033[3m" },
          { "ritm", 321, "\033[23m" },
          { "setaf", 359, "\033[38;5;%p1%dm" },
          { "setab", 360, "\033[48;5;%p1%dm" } };

  static unsigned
  mulib_terminfo_u16_ (const unsigned char *p, unsigned be)
  {
    return be ? (unsigned)p[0] * 256u + p[1] : (unsigned)p[1] * 256u + p[0];
  }
  static uint32_t
  mulib_terminfo_u32_ (const unsigned char *p, unsigned be)
  {
    uint32_t n = 0;
    unsigned i;
    for (i = 0; i < 4; ++i)
      n = (n << 8) | p[be ? i : 3u - i];
    return n;
  }
  static const mulib_terminfo_name_ *
  mulib_terminfo_find_ (const mulib_terminfo_name_ *table, size_t count,
                        const char *name)
  {
    size_t i;
    if (!name)
      return NULL;
    for (i = 0; i < count; ++i)
      if (!strcmp (name, table[i].name))
        return table + i;
    return NULL;
  }

  void
  mulib_terminfo_init (mulib_terminfo_t *t)
  {
    if (t)
      memset (t, 0, sizeof (*t));
  }
  void
  mulib_terminfo_free (mulib_terminfo_t *t)
  {
    if (t)
      {
        free (t->data);
        memset (t, 0, sizeof (*t));
      }
  }
  int
  mulib_terminfo_load (mulib_terminfo_t *t, const void *data, size_t n)
  {
    const unsigned char *p = (const unsigned char *)data;
    mulib_terminfo_t v;
    unsigned magic;
    size_t offset, i;
    if (!t || !p || n < 12 || n > 1048576u)
      return 0;
    mulib_terminfo_init (&v);
    magic = mulib_terminfo_u16_ (p, 0);
    if (magic != 0x011au && magic != 0x021eu)
      {
        magic = mulib_terminfo_u16_ (p, 1);
        if (magic != 0x011au && magic != 0x021eu)
          return 0;
        v.big_endian = 1;
      }
    v.number_width = magic == 0x021eu ? 4u : 2u;
    v.names_size = mulib_terminfo_u16_ (p + 2, v.big_endian);
    v.bool_count = mulib_terminfo_u16_ (p + 4, v.big_endian);
    v.num_count = mulib_terminfo_u16_ (p + 6, v.big_endian);
    v.str_count = mulib_terminfo_u16_ (p + 8, v.big_endian);
    v.table_size = mulib_terminfo_u16_ (p + 10, v.big_endian);
    if (!v.names_size || v.names_size > 32767 || v.bool_count > 32767
        || v.num_count > 32767 || v.str_count > 32767 || v.table_size > 32767)
      return 0;
    offset = 12;
    if (v.names_size > n - offset)
      return 0;
    if (p[offset + v.names_size - 1] != 0)
      return 0;
    offset += v.names_size;
    v.bool_offset = offset;
    if (v.bool_count > n - offset)
      return 0;
    for (i = 0; i < v.bool_count; ++i)
      if (p[offset + i] > 1 && p[offset + i] < 254)
        return 0;
    offset += v.bool_count;
    if (offset & 1u)
      {
        if (offset == n)
          return 0;
        ++offset;
      }
    v.num_offset = offset;
    if (v.num_count > (n - offset) / v.number_width)
      return 0;
    offset += v.num_count * v.number_width;
    v.str_offset = offset;
    if (v.str_count > (n - offset) / 2u)
      return 0;
    offset += v.str_count * 2u;
    v.table_offset = offset;
    if (v.table_size > n - offset)
      return 0;
    offset += v.table_size;
    for (i = 0; i < v.str_count; ++i)
      {
        unsigned s
            = mulib_terminfo_u16_ (p + v.str_offset + i * 2, v.big_endian);
        if (s == 65535u || s == 65534u)
          continue;
        if (s >= v.table_size
            || !memchr (p + v.table_offset + s, 0, v.table_size - s))
          return 0;
      }
    v.data = (unsigned char *)malloc (offset);
    if (!v.data)
      return 0;
    memcpy (v.data, p, offset);
    v.length = offset;
    mulib_terminfo_free (t);
    *t = v;
    return 1;
  }
  int
  mulib_terminfo_load_file (mulib_terminfo_t *t, const char *path)
  {
    FILE *f;
    mulib_strbuf_t bytes;
    char block[4096];
    size_t n;
    int ok = 1;
    if (!t || !path)
      return 0;
    f = fopen (path, "rb");
    if (!f)
      return 0;
    mulib_strbuf_init (&bytes);
    while ((n = fread (block, 1, sizeof (block), f)) != 0)
      {
        if (n > 1048576u - bytes.length
            || !mulib_strbuf_append_n (&bytes, block, n))
          {
            ok = 0;
            break;
          }
      }
    if (ferror (f))
      ok = 0;
    if (fclose (f) != 0)
      ok = 0;
    if (ok)
      ok = mulib_terminfo_load (t, bytes.data, bytes.length);
    mulib_strbuf_free (&bytes);
    return ok;
  }
  void
  mulib_terminfo_ansi (mulib_terminfo_t *t)
  {
    if (t)
      {
        mulib_terminfo_free (t);
        t->ansi = 1;
      }
  }
  const char *
  mulib_terminfo_names (const mulib_terminfo_t *t)
  {
    if (!t)
      return NULL;
    if (t->ansi)
      return "mulib-ansi|explicit ANSI 256-color fallback";
    return t->data ? (const char *)t->data + 12 : NULL;
  }
  int
  mulib_terminfo_bool_at (const mulib_terminfo_t *t, size_t i)
  {
    if (!t)
      return -1;
    if (t->ansi)
      {
        if (i == 1 || i == 4 || i == 8 || i == 14 || i == 25 || i == 28)
          return 1;
        return i < 37 ? 0 : -1;
      }
    if (!t->data || i >= t->bool_count)
      return -1;
    return t->data[t->bool_offset + i] <= 1 ? t->data[t->bool_offset + i] : -1;
  }
  int32_t
  mulib_terminfo_num_at (const mulib_terminfo_t *t, size_t i)
  {
    uint32_t n;
    if (!t)
      return -1;
    if (t->ansi)
      {
        if (i == 1)
          return 8;
        if (i == 13)
          return 256;
        if (i == 14)
          return 65536;
        if (i == 15)
          return 0;
        return -1;
      }
    if (!t->data || i >= t->num_count)
      return -1;
    if (t->number_width == 4)
      {
        n = mulib_terminfo_u32_ (t->data + t->num_offset + i * 4,
                                 t->big_endian);
        return n <= INT32_MAX ? (int32_t)n : -1;
      }
    n = mulib_terminfo_u16_ (t->data + t->num_offset + i * 2, t->big_endian);
    return n <= 32767u ? (int32_t)n : -1;
  }
  const char *
  mulib_terminfo_str_at (const mulib_terminfo_t *t, size_t i)
  {
    unsigned offset;
    size_t k;
    if (!t)
      return NULL;
    if (t->ansi)
      {
        for (k = 0; k < sizeof (mulib_terminfo_strings_)
                            / sizeof (mulib_terminfo_strings_[0]);
             ++k)
          if (mulib_terminfo_strings_[k].index == i)
            return mulib_terminfo_strings_[k].ansi;
        return NULL;
      }
    if (!t->data || i >= t->str_count)
      return NULL;
    offset
        = mulib_terminfo_u16_ (t->data + t->str_offset + i * 2, t->big_endian);
    return offset < t->table_size
               ? (const char *)t->data + t->table_offset + offset
               : NULL;
  }
  int
  mulib_terminfo_bool (const mulib_terminfo_t *t, const char *name)
  {
    const mulib_terminfo_name_ *v = mulib_terminfo_find_ (
        mulib_terminfo_bools_,
        sizeof (mulib_terminfo_bools_) / sizeof (mulib_terminfo_bools_[0]),
        name);
    return v ? mulib_terminfo_bool_at (t, v->index) : -1;
  }
  int32_t
  mulib_terminfo_num (const mulib_terminfo_t *t, const char *name)
  {
    const mulib_terminfo_name_ *v = mulib_terminfo_find_ (
        mulib_terminfo_nums_,
        sizeof (mulib_terminfo_nums_) / sizeof (mulib_terminfo_nums_[0]),
        name);
    return v ? mulib_terminfo_num_at (t, v->index) : -1;
  }
  const char *
  mulib_terminfo_str (const mulib_terminfo_t *t, const char *name)
  {
    const mulib_terminfo_name_ *v;
    if (t && t->ansi && name && !strcmp (name, "smxx"))
      return "\033[9m";
    v = mulib_terminfo_find_ (mulib_terminfo_strings_,
                              sizeof (mulib_terminfo_strings_)
                                  / sizeof (mulib_terminfo_strings_[0]),
                              name);
    return v ? mulib_terminfo_str_at (t, v->index) : NULL;
  }

  static int
  mulib_terminfo_append_ (mulib_strbuf_t *s, const char *p, size_t n)
  {
    return n <= 65536u - s->length && mulib_strbuf_append_n (s, p, n);
  }
  int
  mulib_terminfo_expand (const char *fmt, const int32_t args[9],
                         mulib_strbuf_t *out)
  {
    int32_t params[9] = { 0 }, vars[26] = { 0 }, stack[32];
    struct
    {
      int parent, condition, stage;
    } frames[16];
    size_t sp = 0, depth = 0, steps = 0;
    int active = 1, ok = 0;
    mulib_strbuf_t text;
    const unsigned char *p = (const unsigned char *)fmt;
    if (!fmt || !out)
      return 0;
    if (args)
      memcpy (params, args, sizeof (params));
    mulib_strbuf_init (&text);
    while (*p)
      {
        unsigned c;
        int32_t value = 0, a, b;
        if (++steps > 65536u)
          goto done;
        if (p[0] == '$' && p[1] == '<')
          {
            size_t digits = 0;
            unsigned flags = 0;
            p += 2;
            while (*p >= '0' && *p <= '9')
              {
                ++p;
                if (++digits > 32)
                  goto done;
              }
            if (!digits)
              goto done;
            if (*p == '.')
              {
                ++p;
                digits = 0;
                while (*p >= '0' && *p <= '9')
                  {
                    ++p;
                    if (++digits > 32)
                      goto done;
                  }
                if (!digits)
                  goto done;
              }
            while (*p == '*' || *p == '/')
              {
                unsigned bit = *p++ == '*' ? 1u : 2u;
                if (flags & bit)
                  goto done;
                flags |= bit;
              }
            if (*p++ != '>')
              goto done;
            continue;
          }
        if (*p != '%')
          {
            if (active && !mulib_terminfo_append_ (&text, (const char *)p, 1))
              goto done;
            ++p;
            continue;
          }
        ++p;
        c = *p++;
        if (!c)
          goto done;
        if (c == '?')
          {
            if (depth == 16)
              goto done;
            frames[depth].parent = active;
            frames[depth].condition = 0;
            frames[depth++].stage = 0;
          }
        else if (c == 't')
          {
            int eligible;
            if (!depth || frames[depth - 1].stage == 1)
              goto done;
            eligible = active;
            if (eligible)
              {
                if (!sp)
                  goto done;
                value = stack[--sp] != 0;
                frames[depth - 1].condition |= value;
              }
            frames[depth - 1].stage = 1;
            active = eligible && value;
          }
        else if (c == 'e')
          {
            if (!depth || frames[depth - 1].stage != 1)
              goto done;
            frames[depth - 1].stage = 2;
            active = frames[depth - 1].parent && !frames[depth - 1].condition;
          }
        else if (c == ';')
          {
            if (!depth || !frames[depth - 1].stage)
              goto done;
            active = frames[--depth].parent;
          }
        else if (c == '%')
          {
            if (active && !mulib_terminfo_append_ (&text, "%", 1))
              goto done;
          }
        else if (c == 'i')
          {
            if (active)
              {
                if (params[0] == INT32_MAX || params[1] == INT32_MAX)
                  goto done;
                ++params[0];
                ++params[1];
              }
          }
        else if (c == 'p' || c == 'g' || c == '{' || c == '\'')
          {
            if (c == 'p')
              {
                if (*p < '1' || *p > '9')
                  goto done;
                value = params[*p++ - '1'];
              }
            else if (c == 'g')
              {
                if (*p < 'a' || *p > 'z')
                  goto done;
                value = vars[*p++ - 'a'];
              }
            else if (c == '\'')
              {
                if (!*p)
                  goto done;
                value = *p++;
                if (*p++ != '\'')
                  goto done;
              }
            else
              {
                int negative = 0;
                uint64_t n = 0, limit;
                if (*p == '-')
                  {
                    negative = 1;
                    ++p;
                  }
                if (*p < '0' || *p > '9')
                  goto done;
                limit = negative ? (uint64_t)INT32_MAX + 1u : INT32_MAX;
                while (*p >= '0' && *p <= '9')
                  {
                    unsigned digit = *p++ - '0';
                    if (n > (limit - digit) / 10u)
                      goto done;
                    n = n * 10u + digit;
                  }
                if (*p++ != '}')
                  goto done;
                value = negative ? (int32_t)(-(int64_t)n) : (int32_t)n;
              }
            if (active)
              {
                if (sp == 32)
                  goto done;
                stack[sp++] = value;
              }
          }
        else if (c == 'P')
          {
            if (*p < 'a' || *p > 'z')
              goto done;
            if (active)
              {
                if (!sp)
                  goto done;
                vars[*p - 'a'] = stack[--sp];
              }
            ++p;
          }
        else if (c == '!' || c == '~')
          {
            if (active)
              {
                if (!sp)
                  goto done;
                stack[sp - 1] = c == '!' ? !stack[sp - 1]
                                         : -(stack[sp - 1] + (int64_t)1);
              }
          }
        else if (strchr ("+-*/m&|^=><AO", (int)c))
          {
            int64_t n = 0;
            if (!active)
              continue;
            if (sp < 2)
              goto done;
            b = stack[--sp];
            a = stack[--sp];
            switch (c)
              {
              case '+':
                n = (int64_t)a + b;
                break;
              case '-':
                n = (int64_t)a - b;
                break;
              case '*':
                n = (int64_t)a * b;
                break;
              case '/':
                if (!b)
                  goto done;
                n = (int64_t)a / b;
                break;
              case 'm':
                if (!b)
                  goto done;
                n = (int64_t)a % b;
                break;
              case '&':
                n = a & b;
                break;
              case '|':
                n = a | b;
                break;
              case '^':
                n = a ^ b;
                break;
              case '=':
                n = a == b;
                break;
              case '>':
                n = a > b;
                break;
              case '<':
                n = a < b;
                break;
              case 'A':
                n = a && b;
                break;
              case 'O':
                n = a || b;
                break;
              }
            if (n < INT32_MIN || n > INT32_MAX)
              goto done;
            stack[sp++] = (int32_t)n;
          }
        else if (c == 'd' || c == 'o' || c == 'x' || c == 'X' || c == 'c'
                 || c == ':' || (c >= '0' && c <= '9'))
          {
            unsigned width = 0;
            int zero = 0, count;
            char number[64];
            if (c == ':')
              {
                c = *p++;
                if (!c)
                  goto done;
              }
            if (c == '0')
              zero = 1;
            while (c >= '0' && c <= '9')
              {
                width = width * 10u + c - '0';
                if (width > 32)
                  goto done;
                c = *p++;
                if (!c)
                  goto done;
              }
            if (c != 'd' && c != 'o' && c != 'x' && c != 'X' && c != 'c')
              goto done;
            if (!active)
              continue;
            if (!sp)
              goto done;
            value = stack[--sp];
            if (c == 'c')
              {
                unsigned char ch;
                if (width || value < 1 || value > 255)
                  goto done;
                ch = (unsigned char)value;
                if (!mulib_terminfo_append_ (&text, (const char *)&ch, 1))
                  goto done;
                continue;
              }
            if (c == 'd')
              count
                  = snprintf (number, sizeof (number), zero ? "%0*ld" : "%*ld",
                              (int)width, (long)value);
            else
              {
                const char *f = c == 'o'   ? (zero ? "%0*lo" : "%*lo")
                                : c == 'x' ? (zero ? "%0*lx" : "%*lx")
                                           : (zero ? "%0*lX" : "%*lX");
                count = snprintf (number, sizeof (number), f, (int)width,
                                  (unsigned long)(uint32_t)value);
              }
            if (count < 0 || (size_t)count >= sizeof (number)
                || !mulib_terminfo_append_ (&text, number, (size_t)count))
              goto done;
          }
        else
          goto done;
      }
    if (depth)
      goto done;
    ok = mulib_strbuf_append_n (out, text.data, text.length);
  done:
    mulib_strbuf_free (&text);
    return ok;
  }
  int
  mulib_terminfo_emit (const mulib_terminfo_t *t, const char *name,
                       const int32_t params[9], mulib_strbuf_t *out)
  {
    const char *fmt = mulib_terminfo_str (t, name);
    return fmt ? mulib_terminfo_expand (fmt, params, out) : 0;
  }
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TERMINFO_IMPLEMENTED */
