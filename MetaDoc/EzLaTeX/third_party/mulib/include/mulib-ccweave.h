/** @file mulib-ccweave.h
 * @brief Bounded, standalone checked C11 source generation for common MIR.
 * Define MULIB_IMPL_CCWEAVE; explicitly implement MIR and STRBUF as well.
 */
#ifndef MULIB_CCWEAVE_H
#define MULIB_CCWEAVE_H
#include "mulib-mir.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** Generate a complete C translation unit. Caller frees *source; set
   * *source=NULL first. Function has signature int name(const int64_t *,
   * size_t, int64_t *), returning numeric mulib_mir_status_t values. No MuLIB
   * dependency in generated source. symbol must start with "mulib_generated_",
   * have a nonempty ASCII identifier suffix, and contain at most 127 bytes.
   * byte_limit includes the NUL terminator. All common MIR opcodes are
   * supported.
   */
  mulib_mir_status_t
  mulib_ccweave_generate (const mulib_mir_program_t *program,
                          const char *symbol, size_t record_limit,
                          size_t byte_limit, char **source,
                          mulib_mir_diagnostic_t *diagnostic);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CCWEAVE) && !defined(MULIB_CCWEAVE_IMPLEMENTED)
#define MULIB_CCWEAVE_IMPLEMENTED
#include "mulib-strbuf.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_ccweave__symbol (const char *s)
  {
    size_t k;
    if (!s || strncmp (s, "mulib_generated_", 16) || !s[16])
      return 0;
    for (k = 0; s[k]; ++k)
      if (k == 127
          || !((s[k] >= 'A' && s[k] <= 'Z') || (s[k] >= 'a' && s[k] <= 'z')
               || (s[k] >= '0' && s[k] <= '9') || s[k] == '_'))
        return 0;
    return 1;
  }
  static mulib_mir_status_t
  mulib_ccweave__put (mulib_strbuf_t *b, size_t limit, const char *format, ...)
  {
    char text[2048];
    int n;
    size_t need;
    va_list ap;
    va_start (ap, format);
    n = vsnprintf (text, sizeof (text), format, ap);
    va_end (ap);
    if (n < 0 || (size_t)n >= sizeof (text) || b->length >= limit
        || (size_t)n > limit - b->length - 1)
      return MULIB_MIR_RESOURCE_LIMIT;
    need = b->length + (size_t)n + 1;
    if (need > b->capacity)
      {
        size_t capacity
            = b->capacity ? b->capacity : (limit < 256 ? limit : 256);
        char *next;
        while (capacity < need)
          {
            if (capacity > limit / 2)
              {
                capacity = limit;
                break;
              }
            capacity *= 2;
          }
        next = (char *)realloc (b->data, capacity);
        if (!next)
          return MULIB_MIR_NO_MEMORY;
        b->data = next;
        b->capacity = capacity;
      }
    return mulib_strbuf_append_n (b, text, (size_t)n) ? MULIB_MIR_OK
                                                      : MULIB_MIR_NO_MEMORY;
  }
  mulib_mir_status_t
  mulib_ccweave_generate (const mulib_mir_program_t *p, const char *name,
                          size_t records, size_t bytes, char **out,
                          mulib_mir_diagnostic_t *d)
  {
    mulib_strbuf_t b;
    mulib_mir_status_t s;
    size_t k = 0;
    mulib_strbuf_init (&b);
    if (!out || *out || !mulib_ccweave__symbol (name))
      {
        s = MULIB_MIR_INVALID;
        goto done;
      }
    s = mulib_mir_validate (p, records, d);
    if (s != MULIB_MIR_OK)
      {
        mulib_strbuf_free (&b);
        return s;
      }
#define MULIB_CCWEAVE_PUT(...)                                                \
  do                                                                          \
    {                                                                         \
      s = mulib_ccweave__put (&b, bytes, __VA_ARGS__);                        \
      if (s != MULIB_MIR_OK)                                                  \
        goto done;                                                            \
    }                                                                         \
  while (0)
    MULIB_CCWEAVE_PUT (
        "/* Status numbers are mulib_mir_status_t; output is unchanged on "
        "failure. */\n#include <stdint.h>\n#include <stddef.h>\n#include "
        "<limits.h>\nint %s(const int64_t *args, size_t argc, int64_t *out) "
        "{\n",
        name);
    MULIB_CCWEAVE_PUT ("  (void)args; (void)argc;\n  if (!out || argc != "
                       "(size_t)UINTMAX_C(%" PRIuMAX
                       ") || (!args && argc)) return %d;\n",
                       (uintmax_t)p->parameter_count, (int)MULIB_MIR_INVALID);
    for (k = 0; k < p->count; ++k)
      if (p->code[k].opcode == MULIB_MIR_PARAM
          && p->code[k].type == MULIB_MIR_BOOL)
        MULIB_CCWEAVE_PUT ("  if (args[%" PRId64 "] != 0 && args[%" PRId64
                           "] != 1) return %d;\n",
                           p->code[k].immediate, p->code[k].immediate,
                           (int)MULIB_MIR_BAD_TYPE);
    for (k = 0; k < p->count; ++k)
      {
        const mulib_mir_instruction_t *i = &p->code[k];
        size_t a = i->a, c = i->b;
        if (i->opcode == MULIB_MIR_RETURN)
          {
            MULIB_CCWEAVE_PUT ("  *out = v%zu; return 0;\n}\n", a);
            continue;
          }
        if (i->opcode == MULIB_MIR_ADD)
          MULIB_CCWEAVE_PUT (
              "  if ((v%zu > 0 && v%zu > INT64_MAX - v%zu) || (v%zu < 0 && "
              "v%zu < INT64_MIN - v%zu)) return %d;\n",
              c, a, c, c, a, c, (int)MULIB_MIR_OVERFLOW);
        if (i->opcode == MULIB_MIR_SUB)
          MULIB_CCWEAVE_PUT (
              "  if ((v%zu > 0 && v%zu < INT64_MIN + v%zu) || (v%zu < 0 && "
              "v%zu > INT64_MAX + v%zu)) return %d;\n",
              c, a, c, c, a, c, (int)MULIB_MIR_OVERFLOW);
        if (i->opcode == MULIB_MIR_MUL)
          {
            MULIB_CCWEAVE_PUT (
                "  if (v%zu > 0) {\n    if ((v%zu > 0 && v%zu > INT64_MAX / "
                "v%zu) || (v%zu < 0 && v%zu < INT64_MIN / v%zu)) return %d;\n "
                " } else if (v%zu < 0) {\n",
                a, c, a, c, c, c, a, (int)MULIB_MIR_OVERFLOW, a);
            MULIB_CCWEAVE_PUT (
                "    if ((v%zu > 0 && v%zu < INT64_MIN / v%zu) || (v%zu < 0 "
                "&& v%zu < INT64_MAX / v%zu)) return %d;\n  }\n",
                c, a, c, c, a, c, (int)MULIB_MIR_OVERFLOW);
          }
        if (i->opcode == MULIB_MIR_DIV || i->opcode == MULIB_MIR_MOD)
          MULIB_CCWEAVE_PUT ("  if (!v%zu) return %d;\n  if (v%zu == "
                             "INT64_MIN && v%zu == -1) return %d;\n",
                             c, (int)MULIB_MIR_DIVIDE_BY_ZERO, a, c,
                             (int)MULIB_MIR_OVERFLOW);
        if (i->opcode == MULIB_MIR_NEG)
          MULIB_CCWEAVE_PUT ("  if (v%zu == INT64_MIN) return %d;\n", a,
                             (int)MULIB_MIR_OVERFLOW);
        switch (i->opcode)
          {
          case MULIB_MIR_CONST:
            if (i->immediate == INT64_MIN)
              {
                MULIB_CCWEAVE_PUT ("  int64_t v%zu = INT64_MIN;\n", k);
              }
            else if (i->immediate < 0)
              {
                MULIB_CCWEAVE_PUT ("  int64_t v%zu = -INT64_C(%" PRId64 ");\n",
                                   k, -i->immediate);
              }
            else
              {
                MULIB_CCWEAVE_PUT ("  int64_t v%zu = INT64_C(%" PRId64 ");\n",
                                   k, i->immediate);
              }
            break;
          case MULIB_MIR_PARAM:
            MULIB_CCWEAVE_PUT ("  int64_t v%zu = args[%" PRId64 "];\n", k,
                               i->immediate);
            break;
          case MULIB_MIR_COPY:
            MULIB_CCWEAVE_PUT ("  int64_t v%zu = v%zu;\n", k, a);
            break;
          case MULIB_MIR_NEG:
          case MULIB_MIR_NOT:
            MULIB_CCWEAVE_PUT ("  int64_t v%zu = %sv%zu;\n", k,
                               i->opcode == MULIB_MIR_NEG ? "-" : "!", a);
            break;
          default:
            {
              const char *op = i->opcode == MULIB_MIR_ADD   ? "+"
                               : i->opcode == MULIB_MIR_SUB ? "-"
                               : i->opcode == MULIB_MIR_MUL ? "*"
                               : i->opcode == MULIB_MIR_DIV ? "/"
                               : i->opcode == MULIB_MIR_MOD ? "%"
                               : i->opcode == MULIB_MIR_EQ  ? "=="
                               : i->opcode == MULIB_MIR_LT  ? "<"
                                                            : "<=";
              MULIB_CCWEAVE_PUT ("  int64_t v%zu = v%zu %s v%zu;\n", k, a, op,
                                 c);
              break;
            }
          }
        MULIB_CCWEAVE_PUT ("  (void)v%zu;\n", k);
      }
    *out = b.data;
    b.data = NULL;
    s = MULIB_MIR_OK;
  done:
#undef MULIB_CCWEAVE_PUT
    mulib_strbuf_free (&b);
    if (d)
      {
        d->status = s;
        d->record = k;
      }
    return s;
  }
#ifdef __cplusplus
}
#endif
#endif
