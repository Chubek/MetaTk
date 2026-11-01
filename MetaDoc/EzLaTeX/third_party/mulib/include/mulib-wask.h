/**
 * @file mulib-wask.h
 * @brief Validated, bounded i32 WebAssembly module interpreter.
 * Define MULIB_IMPL_WASK and the explicit dsymspec/JSON/parser dependencies.
 * See manual/mulib-wask.md for the supported instruction and section profile.
 */
#ifndef MULIB_WASK_H
#define MULIB_WASK_H
#include "mulib-dsymspec.h"
#include "mulib-utf8.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Return API revision 1. */
  unsigned mulib_wask_version (void);
  typedef struct mulib_wask mulib_wask_t;
  typedef enum mulib_wask_status
  {
    MULIB_WASK_OK,
    MULIB_WASK_INVALID,
    MULIB_WASK_UNSUPPORTED,
    MULIB_WASK_LIMIT,
    MULIB_WASK_MEMORY,
    MULIB_WASK_TRAP,
    MULIB_WASK_NOT_FOUND
  } mulib_wask_status_t;
  /** @brief Static message and byte offset in the original module. */
  typedef struct mulib_wask_error
  {
    mulib_wask_status_t code;
    size_t offset;
    const char *message;
  } mulib_wask_error_t;
  /** @brief Return 1 for an instruction in this execution profile, else 0.
   * Exact mnemonic matching; NULL returns 0. Does not imply section support.
   */
  int mulib_wask_supports (const char *mnemonic);
  /** @brief Copy and validate a version-1 binary module, or NULL with error.
   * No imports, host access or mutable shared state. Maximum input: 1 MiB.
   */
  mulib_wask_t *mulib_wask_load (const void *bytes, size_t length,
                                 mulib_wask_error_t *error);
  /** @brief Release owned bytes and module; NULL safe. */
  void mulib_wask_destroy (mulib_wask_t *module);
  /** @brief Invoke an exported function with i32 bit-pattern arguments.
   * Output changes only on success; void functions write zero. Zero fuel
   * allows no instructions. Module is immutable; independent calls may run
   * concurrently.
   */
  mulib_wask_status_t mulib_wask_call (const mulib_wask_t *module,
                                       const char *name,
                                       const uint32_t *arguments, size_t count,
                                       size_t fuel, uint32_t *out,
                                       mulib_wask_error_t *error);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WASK) && !defined(MULIB_WASK_IMPLEMENTED)
#define MULIB_WASK_IMPLEMENTED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  unsigned
  mulib_wask_version (void)
  {
    return 1;
  }
  /* Limits bound metadata, operand/local stacks, and work before execution. */
  typedef struct mulib_wask_type_
  {
    uint32_t parameters, results;
  } mulib_wask_type_;
  typedef struct mulib_wask_function_
  {
    uint32_t type, locals;
    size_t start, end;
  } mulib_wask_function_;
  typedef struct mulib_wask_export_
  {
    size_t start, length;
    uint32_t function;
  } mulib_wask_export_;
  struct mulib_wask
  {
    unsigned char *bytes;
    size_t length;
    uint32_t type_count, function_count, export_count;
    mulib_wask_type_ types[128];
    mulib_wask_function_ functions[128];
    mulib_wask_export_ exports[128];
    unsigned char operations[256];
  };
  typedef struct mulib_wask_reader_
  {
    const unsigned char *bytes;
    size_t at, end;
    mulib_wask_error_t *error;
  } mulib_wask_reader_;
  static int
  mulib_wask_fail_ (mulib_wask_reader_ *r, mulib_wask_status_t code,
                    const char *message)
  {
    if (r->error)
      {
        r->error->code = code;
        r->error->offset = r->at;
        r->error->message = message;
      }
    return 0;
  }
  static int
  mulib_wask_byte_ (mulib_wask_reader_ *r, unsigned *out)
  {
    if (r->at == r->end)
      return mulib_wask_fail_ (r, MULIB_WASK_INVALID, "truncated input");
    *out = r->bytes[r->at++];
    return 1;
  }
  static int
  mulib_wask_leb_ (mulib_wask_reader_ *r, uint32_t *out, int sign)
  {
    uint32_t value = 0;
    for (unsigned i = 0; i < 5; ++i)
      {
        unsigned b;
        if (!mulib_wask_byte_ (r, &b))
          return 0;
        if (i == 4
            && (sign ? ((b & 0x78) != 0 && (b & 0x78) != 0x78)
                     : (b & 0x70) != 0))
          return mulib_wask_fail_ (r, MULIB_WASK_INVALID, "LEB128 overflow");
        value |= (uint32_t)(b & 127) << (7 * i);
        if (!(b & 128))
          {
            if (sign && i < 4 && (b & 64))
              value |= UINT32_MAX << (7 * (i + 1));
            *out = value;
            return 1;
          }
      }
    return mulib_wask_fail_ (r, MULIB_WASK_INVALID, "unterminated LEB128");
  }
  static int
  mulib_wask_name_ (mulib_wask_reader_ *r, size_t *start, size_t *length)
  {
    uint32_t n;
    if (!mulib_wask_leb_ (r, &n, 0))
      return 0;
    if (n > r->end - r->at || !mulib_utf8_validate (r->bytes + r->at, n))
      return mulib_wask_fail_ (r, MULIB_WASK_INVALID, "invalid UTF-8 name");
    *start = r->at;
    *length = n;
    r->at += n;
    return 1;
  }
  /* Semantic identifiers are private; wire opcodes come from the supplied ISA.
   */
  static const char *const mulib_wask_names_[]
      = { "unreachable", "nop",       "end",       "drop",      "select",
          "local.get",   "local.set", "local.tee", "i32.const", "i32.add",
          "i32.sub",     "i32.mul",   "i32.div_s", "i32.and",   "i32.or",
          "i32.xor",     "i32.eqz" };
  int
  mulib_wask_supports (const char *name)
  {
    if (!name)
      return 0;
    /* unreachable has an ISA mapping, but polymorphic tails are unsupported.
     */
    for (size_t i = 1;
         i < sizeof (mulib_wask_names_) / sizeof (mulib_wask_names_[0]); ++i)
      if (!strcmp (name, mulib_wask_names_[i]))
        return 1;
    return 0;
  }
  static int
  mulib_wask_isa_ (mulib_wask_t *m)
  {
    mulib_dsymspec_t *spec = mulib_dsymspec_load_builtin ("wasm", NULL);
    int ok = spec != NULL;
    if (!spec)
      return -1;
    for (size_t i = 0;
         ok && i < sizeof (mulib_wask_names_) / sizeof (mulib_wask_names_[0]);
         ++i)
      {
        const mulib_json_t *form
            = mulib_dsymspec_find (spec, "forms", mulib_wask_names_[i]);
        const char *encoder
            = mulib_json_text (mulib_json_get (form, "encoder"), NULL);
        const mulib_json_t *record
            = encoder ? mulib_dsymspec_find (spec, "encoders", encoder) : NULL;
        const char *body
            = mulib_json_text (mulib_json_get (record, "body"), NULL);
        unsigned opcode;
        ok = body && sscanf (body, " EMIT8 %x", &opcode) == 1 && opcode < 256;
        if (ok && !m->operations[opcode])
          m->operations[opcode] = (unsigned char)(i + 1);
        else
          ok = 0;
      }
    mulib_dsymspec_destroy (spec);
    return ok;
  }
  /* Validation and execution share decoding; validation never performs
   * arithmetic. */
  static int
  mulib_wask_body_ (const mulib_wask_t *m, uint32_t index,
                    const uint32_t *args, size_t fuel, uint32_t *out,
                    int execute, mulib_wask_error_t *error)
  {
    const mulib_wask_function_ *f = &m->functions[index];
    const mulib_wask_type_ *t = &m->types[f->type];
    mulib_wask_reader_ r = { m->bytes, f->start, f->end, error };
    uint32_t stack[256], locals[256] = { 0 };
    size_t sp = 0;
    if (execute && t->parameters)
      memcpy (locals, args, t->parameters * sizeof (*args));
    while (r.at < r.end)
      {
        unsigned byte, op;
        uint32_t imm = 0, a, b, value;
        if (execute && !fuel--)
          return mulib_wask_fail_ (&r, MULIB_WASK_LIMIT,
                                   "instruction fuel exhausted");
        if (!mulib_wask_byte_ (&r, &byte))
          return 0;
        op = m->operations[byte];
        if (!op)
          return mulib_wask_fail_ (&r, MULIB_WASK_UNSUPPORTED,
                                   "unsupported instruction");
        if (op == 3)
          {
            if (r.at != r.end || sp != t->results)
              return mulib_wask_fail_ (&r, MULIB_WASK_INVALID,
                                       "invalid function result stack or end");
            if (execute)
              *out = sp ? stack[0] : 0;
            return 1;
          }
        if (op >= 6 && op <= 9)
          {
            if (!mulib_wask_leb_ (&r, &imm, op == 9))
              return 0;
            if (op != 9 && imm >= t->parameters + f->locals)
              return mulib_wask_fail_ (&r, MULIB_WASK_INVALID,
                                       "local index out of range");
          }
        if (op == 1)
          {
            /* Reject during validation too: polymorphic unreachable tails are
             * outside this straight-line profile; no invalid dead code can be
             * hidden. */
            return mulib_wask_fail_ (
                &r, execute ? MULIB_WASK_TRAP : MULIB_WASK_UNSUPPORTED,
                "unreachable instruction");
          }
        if (op == 2)
          continue;
        if ((op == 4 || op == 7 || op == 8 || op == 17) && sp < 1)
          return mulib_wask_fail_ (&r, MULIB_WASK_INVALID,
                                   "operand stack underflow");
        if (op == 4)
          {
            --sp;
            continue;
          }
        if (op == 7 || op == 8)
          {
            if (execute)
              locals[imm] = stack[sp - 1];
            if (op == 7)
              --sp;
            continue;
          }
        if (op == 17)
          {
            if (execute)
              stack[sp - 1] = !stack[sp - 1];
            continue;
          }
        if (op == 6 || op == 9)
          {
            if (sp == 256)
              return mulib_wask_fail_ (&r, MULIB_WASK_LIMIT,
                                       "operand stack limit");
            stack[sp++] = execute ? (op == 6 ? locals[imm] : imm) : 0;
            continue;
          }
        if (op == 5)
          {
            if (sp < 3)
              return mulib_wask_fail_ (&r, MULIB_WASK_INVALID,
                                       "select stack underflow");
            if (execute)
              stack[sp - 3] = stack[sp - 1] ? stack[sp - 3] : stack[sp - 2];
            sp -= 2;
            continue;
          }
        if (sp < 2)
          return mulib_wask_fail_ (&r, MULIB_WASK_INVALID,
                                   "binary stack underflow");
        a = stack[sp - 2];
        b = stack[sp - 1];
        value = 0;
        if (execute)
          switch (op)
            {
            case 10:
              value = a + b;
              break;
            case 11:
              value = a - b;
              break;
            case 12:
              value = (uint32_t)((uint64_t)a * b);
              break;
            case 13:
              {
                int64_t sa = a <= INT32_MAX
                                 ? (int64_t)a
                                 : (int64_t)a - INT64_C (4294967296);
                int64_t sb = b <= INT32_MAX
                                 ? (int64_t)b
                                 : (int64_t)b - INT64_C (4294967296);
                if (!b || (sa == INT32_MIN && sb == -1))
                  return mulib_wask_fail_ (&r, MULIB_WASK_TRAP,
                                           "integer division trap");
                value = (uint32_t)(sa / sb);
                break;
              }
            case 14:
              value = a & b;
              break;
            case 15:
              value = a | b;
              break;
            case 16:
              value = a ^ b;
              break;
            default:
              return mulib_wask_fail_ (&r, MULIB_WASK_UNSUPPORTED,
                                       "unsupported operation");
            }
        stack[sp - 2] = value;
        --sp;
      }
    return mulib_wask_fail_ (&r, MULIB_WASK_INVALID, "missing function end");
  }
  void
  mulib_wask_destroy (mulib_wask_t *m)
  {
    if (m)
      {
        free (m->bytes);
        free (m);
      }
  }
  mulib_wask_t *
  mulib_wask_load (const void *bytes, size_t n, mulib_wask_error_t *error)
  {
    mulib_wask_t *m = NULL;
    mulib_wask_reader_ r = { (const unsigned char *)bytes, 0, n, error };
    unsigned last = 0, seen_code = 0;
    static const unsigned char magic[] = { 0, 97, 115, 109, 1, 0, 0, 0 };
    if (error)
      {
        error->code = MULIB_WASK_OK;
        error->offset = 0;
        error->message = "ok";
      }
    if (!bytes || n < 8 || n > 1024U * 1024U || memcmp (bytes, magic, 8))
      {
        mulib_wask_fail_ (
            &r, n > 1024U * 1024U ? MULIB_WASK_LIMIT : MULIB_WASK_INVALID,
            "invalid module header or size");
        return NULL;
      }
    m = (mulib_wask_t *)calloc (1, sizeof (*m));
    if (!m)
      {
        mulib_wask_fail_ (&r, MULIB_WASK_MEMORY, "allocation failed");
        return NULL;
      }
    m->bytes = (unsigned char *)malloc (n);
    m->length = n;
    if (!m->bytes)
      {
        mulib_wask_fail_ (&r, MULIB_WASK_MEMORY, "allocation failed");
        goto fail;
      }
    memcpy (m->bytes, bytes, n);
    r.bytes = m->bytes;
    r.at = 8;
    {
      int isa = mulib_wask_isa_ (m);
      if (isa <= 0)
        {
          mulib_wask_fail_ (
              &r, isa < 0 ? MULIB_WASK_MEMORY : MULIB_WASK_UNSUPPORTED,
              "failed to load WASM ISA metadata");
          goto fail;
        }
    }
    while (r.at < n)
      {
        unsigned id;
        uint32_t length, count;
        size_t end;
        r.end = n;
        if (!mulib_wask_byte_ (&r, &id) || !mulib_wask_leb_ (&r, &length, 0))
          goto fail;
        if (length > n - r.at)
          {
            mulib_wask_fail_ (&r, MULIB_WASK_INVALID,
                              "section exceeds module");
            goto fail;
          }
        end = r.at + length;
        r.end = end;
        if (!id)
          {
            size_t start, size;
            if (!mulib_wask_name_ (&r, &start, &size))
              goto fail;
            r.at = end;
            continue;
          }
        if (id <= last)
          {
            mulib_wask_fail_ (&r, MULIB_WASK_INVALID,
                              "duplicate or unordered section");
            goto fail;
          }
        last = id;
        if (id != 1 && id != 3 && id != 7 && id != 10)
          {
            mulib_wask_fail_ (&r, MULIB_WASK_UNSUPPORTED,
                              "unsupported section");
            goto fail;
          }
        if (!mulib_wask_leb_ (&r, &count, 0))
          goto fail;
        if (count > 128)
          {
            mulib_wask_fail_ (&r, MULIB_WASK_LIMIT, "record count limit");
            goto fail;
          }
        if (id == 1)
          {
            m->type_count = count;
            for (uint32_t i = 0; i < count; ++i)
              {
                unsigned b;
                uint32_t p, results;
                if (!mulib_wask_byte_ (&r, &b))
                  goto fail;
                if (b != 0x60)
                  {
                    mulib_wask_fail_ (&r, MULIB_WASK_UNSUPPORTED,
                                      "unsupported function type");
                    goto fail;
                  }
                if (!mulib_wask_leb_ (&r, &p, 0))
                  goto fail;
                if (p > 256)
                  {
                    mulib_wask_fail_ (&r, MULIB_WASK_LIMIT, "parameter limit");
                    goto fail;
                  }
                for (uint32_t j = 0; j < p; ++j)
                  {
                    if (!mulib_wask_byte_ (&r, &b))
                      goto fail;
                    if (b != 0x7f)
                      {
                        mulib_wask_fail_ (&r, MULIB_WASK_UNSUPPORTED,
                                          "only i32 parameters supported");
                        goto fail;
                      }
                  }
                if (!mulib_wask_leb_ (&r, &results, 0))
                  goto fail;
                if (results > 1)
                  {
                    mulib_wask_fail_ (&r, MULIB_WASK_UNSUPPORTED,
                                      "multiple results unsupported");
                    goto fail;
                  }
                if (results)
                  {
                    if (!mulib_wask_byte_ (&r, &b))
                      goto fail;
                    if (b != 0x7f)
                      {
                        mulib_wask_fail_ (&r, MULIB_WASK_UNSUPPORTED,
                                          "only i32 result supported");
                        goto fail;
                      }
                  }
                m->types[i].parameters = p;
                m->types[i].results = results;
              }
          }
        else if (id == 3)
          {
            m->function_count = count;
            for (uint32_t i = 0; i < count; ++i)
              {
                if (!mulib_wask_leb_ (&r, &m->functions[i].type, 0))
                  goto fail;
                if (m->functions[i].type >= m->type_count)
                  {
                    mulib_wask_fail_ (&r, MULIB_WASK_INVALID,
                                      "invalid type index");
                    goto fail;
                  }
              }
          }
        else if (id == 7)
          {
            m->export_count = count;
            for (uint32_t i = 0; i < count; ++i)
              {
                unsigned kind;
                mulib_wask_export_ *e = &m->exports[i];
                if (!mulib_wask_name_ (&r, &e->start, &e->length)
                    || !mulib_wask_byte_ (&r, &kind)
                    || !mulib_wask_leb_ (&r, &e->function, 0))
                  goto fail;
                if (kind != 0)
                  {
                    mulib_wask_fail_ (&r, MULIB_WASK_UNSUPPORTED,
                                      "only function exports supported");
                    goto fail;
                  }
                if (e->function >= m->function_count)
                  {
                    mulib_wask_fail_ (&r, MULIB_WASK_INVALID,
                                      "invalid exported function");
                    goto fail;
                  }
                for (uint32_t j = 0; j < i; ++j)
                  if (e->length == m->exports[j].length
                      && !memcmp (m->bytes + e->start,
                                  m->bytes + m->exports[j].start, e->length))
                    {
                      mulib_wask_fail_ (&r, MULIB_WASK_INVALID,
                                        "duplicate export name");
                      goto fail;
                    }
              }
          }
        else
          {
            seen_code = 1;
            if (count != m->function_count)
              {
                mulib_wask_fail_ (&r, MULIB_WASK_INVALID,
                                  "function/code count mismatch");
                goto fail;
              }
            for (uint32_t i = 0; i < count; ++i)
              {
                uint32_t size, groups;
                size_t body_end;
                mulib_wask_function_ *f = &m->functions[i];
                if (!mulib_wask_leb_ (&r, &size, 0))
                  goto fail;
                if (size > end - r.at)
                  {
                    mulib_wask_fail_ (&r, MULIB_WASK_INVALID,
                                      "function exceeds section");
                    goto fail;
                  }
                body_end = r.at + size;
                r.end = body_end;
                if (!mulib_wask_leb_ (&r, &groups, 0))
                  goto fail;
                if (groups > 256)
                  {
                    mulib_wask_fail_ (&r, MULIB_WASK_LIMIT,
                                      "local group limit");
                    goto fail;
                  }
                for (uint32_t j = 0; j < groups; ++j)
                  {
                    uint32_t locals;
                    unsigned type;
                    if (!mulib_wask_leb_ (&r, &locals, 0)
                        || !mulib_wask_byte_ (&r, &type))
                      goto fail;
                    if (type != 0x7f)
                      {
                        mulib_wask_fail_ (&r, MULIB_WASK_UNSUPPORTED,
                                          "only i32 locals supported");
                        goto fail;
                      }
                    if (locals
                        > 256 - m->types[f->type].parameters - f->locals)
                      {
                        mulib_wask_fail_ (&r, MULIB_WASK_LIMIT,
                                          "local count limit");
                        goto fail;
                      }
                    f->locals += locals;
                  }
                f->start = r.at;
                f->end = body_end;
                if (!mulib_wask_body_ (m, i, NULL, 0, NULL, 0, error))
                  goto fail;
                r.at = body_end;
                r.end = end;
              }
          }
        if (r.at != end)
          {
            mulib_wask_fail_ (&r, MULIB_WASK_INVALID,
                              "trailing section bytes");
            goto fail;
          }
      }
    if (m->function_count && !seen_code)
      {
        mulib_wask_fail_ (&r, MULIB_WASK_INVALID, "missing code section");
        goto fail;
      }
    return m;
  fail:
    mulib_wask_destroy (m);
    return NULL;
  }
  mulib_wask_status_t
  mulib_wask_call (const mulib_wask_t *m, const char *name,
                   const uint32_t *args, size_t count, size_t fuel,
                   uint32_t *out, mulib_wask_error_t *error)
  {
    mulib_wask_error_t local = { MULIB_WASK_OK, 0, "ok" };
    mulib_wask_reader_ r = { NULL, 0, 0, error ? error : &local };
    *r.error = local;
    if (!m || !name || !out || (!args && count))
      {
        mulib_wask_fail_ (&r, MULIB_WASK_INVALID, "invalid call arguments");
        return r.error->code;
      }
    for (uint32_t i = 0; i < m->export_count; ++i)
      {
        const mulib_wask_export_ *e = &m->exports[i];
        if (strlen (name) != e->length
            || memcmp (name, m->bytes + e->start, e->length))
          continue;
        if (count != m->types[m->functions[e->function].type].parameters)
          {
            mulib_wask_fail_ (&r, MULIB_WASK_INVALID,
                              "argument count mismatch");
            return r.error->code;
          }
        (void)mulib_wask_body_ (m, e->function, args, fuel, out, 1, r.error);
        return r.error->code;
      }
    mulib_wask_fail_ (&r, MULIB_WASK_NOT_FOUND, "export not found");
    return r.error->code;
  }
#ifdef __cplusplus
}
#endif
#endif
