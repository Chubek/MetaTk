/**
 * @file mulib-assembler.h
 * @brief Typed AMD64 subset driven by generated dsymspec asmtk metadata.
 * Define MULIB_IMPL_ASSEMBLER and its documented dependencies explicitly once.
 * No text parser, memory operands, general pseudo-code interpreter or ELF
 * writer.
 */
#ifndef MULIB_ASSEMBLER_H
#define MULIB_ASSEMBLER_H
#include "mulib-dsymspec.h"
#include "mulib-emitter.h"
#include "mulib-linker.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum mulib_assembler_operand_kind
  {
    MULIB_ASSEMBLER_REGISTER,
    MULIB_ASSEMBLER_IMMEDIATE,
    MULIB_ASSEMBLER_SYMBOL
  } mulib_assembler_operand_kind;
  /** @brief Borrowed register/symbol name, or immediate; symbol value is an
   * addend. */
  typedef struct mulib_assembler_operand
  {
    mulib_assembler_operand_kind kind;
    const char *name;
    int64_t value;
  } mulib_assembler_operand_t;
  typedef enum mulib_assembler_result
  {
    MULIB_ASSEMBLER_OK = 0,
    MULIB_ASSEMBLER_EINVAL = -1,
    MULIB_ASSEMBLER_EUNSUPPORTED = -2,
    MULIB_ASSEMBLER_EREGISTER = -3,
    MULIB_ASSEMBLER_ERANGE = -4,
    MULIB_ASSEMBLER_ENOSPACE = -5,
    MULIB_ASSEMBLER_ELINK = -6
  } mulib_assembler_result;
  /** @brief Static diagnostic, plus original linker error if ELINK. */
  typedef struct mulib_assembler_error
  {
    const char *message;
    int linker_result;
  } mulib_assembler_error_t;
  /** @brief Make a borrowed register-name operand. */
  mulib_assembler_operand_t mulib_assembler_reg (const char *name);
  /** @brief Make a signed 64-bit immediate operand (32-bit patterns also
   * allowed). */
  mulib_assembler_operand_t mulib_assembler_imm (int64_t value);
  /** @brief Make a named branch target with a byte addend. */
  mulib_assembler_operand_t mulib_assembler_symbol (const char *name,
                                                    int64_t addend);
  /** @brief Encode one exact, case-sensitive mnemonic into a little-endian
   * image. Uses the specification's resolved opcode, group, operand direction
   * and register encoding data. Failure leaves image and linker unchanged.
   * Linker may be NULL except for Relative forms. Labels are defined using
   * mulib_linker_define.
   */
  int mulib_assembler_emit (const mulib_dsymspec_t *specification,
                            mulib_emitter_t *image, mulib_linker_t *linker,
                            const char *mnemonic,
                            const mulib_assembler_operand_t *operands,
                            size_t count, mulib_assembler_error_t *error);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_ASSEMBLER) && !defined(MULIB_ASSEMBLER_IMPLEMENTED)
#define MULIB_ASSEMBLER_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_assembler_operand_t
  mulib_assembler_reg (const char *name)
  {
    mulib_assembler_operand_t o;
    o.kind = MULIB_ASSEMBLER_REGISTER;
    o.name = name;
    o.value = 0;
    return o;
  }
  mulib_assembler_operand_t
  mulib_assembler_imm (int64_t value)
  {
    mulib_assembler_operand_t o;
    o.kind = MULIB_ASSEMBLER_IMMEDIATE;
    o.name = NULL;
    o.value = value;
    return o;
  }
  mulib_assembler_operand_t
  mulib_assembler_symbol (const char *name, int64_t addend)
  {
    mulib_assembler_operand_t o;
    o.kind = MULIB_ASSEMBLER_SYMBOL;
    o.name = name;
    o.value = addend;
    return o;
  }
  static int
  mulib_assembler_fail_ (mulib_assembler_error_t *e, int result,
                         const char *message)
  {
    if (e)
      e->message = message;
    return result;
  }
  static const char *
  mulib_assembler_text_ (const mulib_json_t *object, const char *key)
  {
    size_t n;
    const char *text = mulib_json_text (mulib_json_get (object, key), &n);
    return text && !memchr (text, 0, n) ? text : NULL;
  }
  static unsigned
  mulib_assembler_number_ (const mulib_json_t *object, const char *key)
  {
    return (unsigned)mulib_json_numeric (mulib_json_get (object, key));
  }
  static int
  mulib_assembler_width_ (const mulib_json_t *record, unsigned width)
  {
    const mulib_json_t *widths = mulib_json_get (record, "widths");
    size_t i;
    for (i = 0; i < mulib_json_size (widths); ++i)
      if (mulib_json_numeric (mulib_json_at (widths, i)) == width)
        return 1;
    return 0;
  }
  static int
  mulib_assembler_match_ (const mulib_json_t *r,
                          const mulib_assembler_operand_t *o, size_t n,
                          const unsigned widths[2])
  {
    const char *f = mulib_assembler_text_ (r, "form");
    if (!strcmp (f, "Fixed"))
      return n == 0;
    if (!strcmp (f, "Relative"))
      return n == 1 && o[0].kind == MULIB_ASSEMBLER_SYMBOL;
    if (!n || o[0].kind != MULIB_ASSEMBLER_REGISTER
        || (widths[0] != 32 && widths[0] != 64)
        || !mulib_assembler_width_ (r, widths[0]))
      return 0;
    if (!strcmp (f, "RegisterRegister"))
      return n == 2 && o[1].kind == MULIB_ASSEMBLER_REGISTER
             && widths[1] == widths[0];
    if (!strcmp (f, "MoveImmediate") || !strcmp (f, "ArithmeticImmediate")
        || !strcmp (f, "ShiftImmediate"))
      return n == 2 && o[1].kind == MULIB_ASSEMBLER_IMMEDIATE;
    if (!strcmp (f, "ShiftCL"))
      {
        const char *implicit = mulib_assembler_text_ (r, "implicit_register");
        return n == 2 && o[1].kind == MULIB_ASSEMBLER_REGISTER && implicit
               && !strcmp (o[1].name, implicit);
      }
    if (!strcmp (f, "Unary") || !strcmp (f, "Indirect")
        || !strcmp (f, "OpcodeRegister"))
      return n == 1;
    return 0;
  }
  int
  mulib_assembler_emit (const mulib_dsymspec_t *spec, mulib_emitter_t *image,
                        mulib_linker_t *linker, const char *mnemonic,
                        const mulib_assembler_operand_t *o, size_t n,
                        mulib_assembler_error_t *error)
  {
    const mulib_json_t *target, *encodings, *selected = NULL, *opcode;
    const char *arch, *order, *form;
    unsigned regs[2] = { 0, 0 }, widths[2] = { 0, 0 };
    size_t i, count;
    unsigned char bytes[16], rex = 0x40, reg = 0, rm = 0;
    mulib_emitter_t out;
    int result;
    if (error)
      {
        error->message = NULL;
        error->linker_result = 0;
      }
    if (!spec || !image || !mnemonic || (!o && n) || n > 2
        || image->size > image->capacity || (!image->data && image->capacity)
        || image->order != MULIB_EMITTER_LITTLE)
      return mulib_assembler_fail_ (
          error, MULIB_ASSEMBLER_EINVAL,
          "invalid arguments; AMD64 requires a little-endian image");
    target = mulib_dsymspec_record (spec, "targets", 0);
    arch = mulib_assembler_text_ (target, "arch");
    order = mulib_assembler_text_ (target, "byte_order");
    encodings = mulib_dsymspec_encodings (spec);
    if (!arch || strcmp (arch, "amd64") || !order
        || (strcmp (order, "little") && strcmp (order, "le"))
        || mulib_assembler_number_ (target, "addr_size") != 64 || !encodings)
      return mulib_assembler_fail_ (
          error, MULIB_ASSEMBLER_EUNSUPPORTED,
          "only generated AMD64 asmtk metadata is executable");
    for (i = 0; i < n; ++i)
      {
        if (o[i].kind == MULIB_ASSEMBLER_REGISTER)
          {
            const mulib_json_t *r
                = mulib_dsymspec_find (spec, "registers", o[i].name);
            const char *class_name = mulib_assembler_text_ (r, "class");
            if (!r || !class_name || strcmp (class_name, "gpr"))
              return mulib_assembler_fail_ (error, MULIB_ASSEMBLER_EREGISTER,
                                            "unknown or non-GPR register");
            regs[i] = mulib_assembler_number_ (r, "enc");
            widths[i] = mulib_assembler_number_ (r, "width");
            if (regs[i] > 15)
              return mulib_assembler_fail_ (
                  error, MULIB_ASSEMBLER_EUNSUPPORTED,
                  "APX register encodings are unsupported");
          }
        else if (o[i].kind == MULIB_ASSEMBLER_SYMBOL)
          {
            if (!o[i].name || !*o[i].name)
              return mulib_assembler_fail_ (error, MULIB_ASSEMBLER_EINVAL,
                                            "empty symbol name");
          }
        else if (o[i].kind != MULIB_ASSEMBLER_IMMEDIATE)
          return mulib_assembler_fail_ (error, MULIB_ASSEMBLER_EINVAL,
                                        "invalid operand kind");
      }
    for (i = 0; i < mulib_json_size (encodings); ++i)
      {
        const mulib_json_t *r = mulib_json_at (encodings, i);
        if (!strcmp (mulib_assembler_text_ (r, "mnemonic"), mnemonic)
            && mulib_assembler_match_ (r, o, n, widths))
          {
            selected = r;
            break;
          }
      }
    if (!selected)
      return mulib_assembler_fail_ (
          error, MULIB_ASSEMBLER_EUNSUPPORTED,
          "mnemonic/operand form is outside the supported metadata subset");
    form = mulib_assembler_text_ (selected, "form");
    opcode = mulib_json_get (selected, "opcode");
    count = mulib_json_size (opcode);
    (void)mulib_emitter_init (&out, bytes, sizeof (bytes),
                              MULIB_EMITTER_LITTLE);
    if (strcmp (form, "Fixed") && strcmp (form, "Relative"))
      {
        if (widths[0] == 64
            && mulib_json_boolean (mulib_json_get (selected, "rex_w")))
          rex |= 8;
        if (!strcmp (form, "RegisterRegister"))
          {
            unsigned index = mulib_assembler_number_ (selected, "reg_operand");
            reg = regs[index];
            rm = regs[1 - index];
            rex |= (reg >> 3) << 2;
          }
        else
          {
            reg = mulib_assembler_number_ (selected, "group");
            rm = regs[0];
          }
        rex |= rm >> 3;
        if (rex != 0x40)
          (void)mulib_emitter_uint (&out, rex, 1);
      }
    for (i = 0; i < count; ++i)
      {
        unsigned byte
            = (unsigned)mulib_json_numeric (mulib_json_at (opcode, i));
        if (i + 1 == count
            && (!strcmp (form, "OpcodeRegister")
                || !strcmp (form, "MoveImmediate")))
          byte += regs[0] & 7;
        if (byte > 255)
          return mulib_assembler_fail_ (error, MULIB_ASSEMBLER_ERANGE,
                                        "opcode-register addition overflows");
        (void)mulib_emitter_uint (&out, byte, 1);
      }
    if (!strcmp (form, "MoveImmediate"))
      {
        if (widths[0] == 32
            && (o[1].value < INT32_MIN || o[1].value > (int64_t)UINT32_MAX))
          return mulib_assembler_fail_ (error, MULIB_ASSEMBLER_ERANGE,
                                        "32-bit immediate is out of range");
        (void)mulib_emitter_uint (&out,
                                  widths[0] == 32 ? (uint32_t)o[1].value
                                                  : (uint64_t)o[1].value,
                                  widths[0] / 8);
      }
    else if (strcmp (form, "Fixed") && strcmp (form, "Relative")
             && strcmp (form, "OpcodeRegister"))
      {
        (void)mulib_emitter_uint (&out, 0xc0U | ((reg & 7) << 3) | (rm & 7),
                                  1);
        if (!strcmp (form, "ShiftImmediate"))
          {
            if (o[1].value < 0 || o[1].value > 255)
              return mulib_assembler_fail_ (
                  error, MULIB_ASSEMBLER_ERANGE,
                  "shift immediate must be in 0..255");
            (void)mulib_emitter_uint (&out, (uint64_t)o[1].value, 1);
          }
        else if (!strcmp (form, "ArithmeticImmediate"))
          {
            if (o[1].value < INT32_MIN
                || o[1].value > (widths[0] == 64 ? (int64_t)INT32_MAX
                                                 : (int64_t)UINT32_MAX))
              return mulib_assembler_fail_ (error, MULIB_ASSEMBLER_ERANGE,
                                            "arithmetic immediate does not "
                                            "fit its encoded 32-bit field");
            (void)mulib_emitter_uint (&out, (uint32_t)o[1].value, 4);
          }
      }
    if (!strcmp (form, "Relative"))
      (void)mulib_emitter_uint (&out, 0, 4);
    if (out.size > image->capacity - image->size)
      return mulib_assembler_fail_ (error, MULIB_ASSEMBLER_ENOSPACE,
                                    "image capacity exceeded");
    if (!strcmp (form, "Relative"))
      {
        const char *name = mulib_assembler_text_ (selected, "relocation_name");
        const mulib_json_t *r = mulib_dsymspec_find (spec, "relocs", name);
        const char *expr = mulib_assembler_text_ (r, "expr");
        if (!r || mulib_assembler_number_ (r, "width") != 32
            || !mulib_json_boolean (mulib_json_get (r, "signed"))
            || !mulib_json_boolean (mulib_json_get (r, "pcrel")) || !expr
            || (strcmp (expr, "S + A - P") && strcmp (expr, "PLT(S) + A - P")))
          return mulib_assembler_fail_ (error, MULIB_ASSEMBLER_EUNSUPPORTED,
                                        "unsupported relocation semantics");
        if (!linker)
          return mulib_assembler_fail_ (error, MULIB_ASSEMBLER_EINVAL,
                                        "relative branches require a linker");
        if (o[0].value < INT64_MIN + 4)
          return mulib_assembler_fail_ (error, MULIB_ASSEMBLER_ERANGE,
                                        "branch addend underflows");
        result = mulib_linker_relocate (linker, o[0].name,
                                        image->size + out.size - 4,
                                        MULIB_LINKER_PC32, o[0].value - 4);
        if (result)
          {
            if (error)
              error->linker_result = result;
            return mulib_assembler_fail_ (error, MULIB_ASSEMBLER_ELINK,
                                          "cannot record branch relocation");
          }
      }
    return mulib_emitter_bytes (image, bytes, out.size);
  }
#ifdef __cplusplus
}
#endif
#endif
