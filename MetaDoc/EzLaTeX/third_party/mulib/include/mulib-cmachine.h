/** @file mulib-cmachine.h
 * @brief Checked lowering of common MIR to the existing minivm stack machine.
 * Define MULIB_IMPL_CMACHINE; explicitly implement MIR and MINIVM as well.
 */
#ifndef MULIB_CMACHINE_H
#define MULIB_CMACHINE_H
#include "mulib-minivm.h"
#include "mulib-mir.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** Owned code, with a borrowed minivm view. Zero initialize; do not copy
   * live objects. */
  typedef struct mulib_cmachine
  {
    mulib_minivm_instruction_t *code;
    mulib_minivm_program_t program;
  } mulib_cmachine_t;
  /** Validate then specialize PARAM records to borrowed argument values, which
   * are copied. Code limit bounds emitted instructions; destination must be
   * empty. Failure is atomic. Diagnostics identify a MIR record, not a
   * bytecode PC. All MIR operations are supported.
   */
  mulib_mir_status_t mulib_cmachine_lower (
      const mulib_mir_program_t *program, const int64_t *arguments,
      size_t argument_count, size_t record_limit, size_t code_limit,
      mulib_cmachine_t *output, mulib_mir_diagnostic_t *diagnostic);
  /** Execute lowered code with minivm limits and exact minivm fault/PC
   * reporting. */
  mulib_minivm_status_t
  mulib_cmachine_run (const mulib_cmachine_t *machine,
                      const mulib_minivm_limits_t *limits,
                      mulib_minivm_result_t *result);
  /** Free owned bytecode and zero the object. */
  void mulib_cmachine_free (mulib_cmachine_t *machine);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CMACHINE) && !defined(MULIB_CMACHINE_IMPLEMENTED)
#define MULIB_CMACHINE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static mulib_minivm_opcode_t
  mulib_cmachine__opcode (mulib_mir_opcode_t op)
  {
    switch (op)
      {
      case MULIB_MIR_ADD:
        return MULIB_MINIVM_ADD;
      case MULIB_MIR_SUB:
        return MULIB_MINIVM_SUB;
      case MULIB_MIR_MUL:
        return MULIB_MINIVM_MUL;
      case MULIB_MIR_DIV:
        return MULIB_MINIVM_DIV;
      case MULIB_MIR_MOD:
        return MULIB_MINIVM_MOD;
      case MULIB_MIR_NEG:
        return MULIB_MINIVM_NEG;
      case MULIB_MIR_EQ:
        return MULIB_MINIVM_EQ;
      case MULIB_MIR_LT:
        return MULIB_MINIVM_LT;
      case MULIB_MIR_LE:
        return MULIB_MINIVM_LE;
      case MULIB_MIR_NOT:
        return MULIB_MINIVM_NOT;
      default:
        return MULIB_MINIVM_HALT;
      }
  }
  mulib_mir_status_t
  mulib_cmachine_lower (const mulib_mir_program_t *p, const int64_t *args,
                        size_t argc, size_t records, size_t limit,
                        mulib_cmachine_t *out, mulib_mir_diagnostic_t *d)
  {
    mulib_mir_status_t s;
    mulib_minivm_instruction_t *code = NULL;
    size_t k = 0, count = 0, at = 0;
    if (!out || out->code || out->program.code || out->program.code_count
        || out->program.local_count || out->program.hosts
        || out->program.host_count || (!args && argc))
      {
        s = MULIB_MIR_INVALID;
        goto done;
      }
    s = mulib_mir_validate (p, records, d);
    if (s != MULIB_MIR_OK)
      return s;
    if (argc != p->parameter_count)
      {
        s = MULIB_MIR_INVALID;
        goto done;
      }
    if ((uintmax_t)p->count > (uintmax_t)INT64_MAX)
      {
        s = MULIB_MIR_RESOURCE_LIMIT;
        goto done;
      }
    for (k = 0; k < p->count; ++k)
      {
        const mulib_mir_instruction_t *i = &p->code[k];
        size_t n = i->opcode == MULIB_MIR_RETURN
                       ? 2
                       : (size_t)mulib_mir_arity (i->opcode) + 2;
        if (i->opcode == MULIB_MIR_COPY)
          n = 2;
        if (i->opcode == MULIB_MIR_PARAM && i->type == MULIB_MIR_BOOL
            && args[(size_t)i->immediate] != 0
            && args[(size_t)i->immediate] != 1)
          {
            s = MULIB_MIR_BAD_TYPE;
            goto done;
          }
        if (n > limit - count)
          {
            s = MULIB_MIR_RESOURCE_LIMIT;
            goto done;
          }
        count += n;
      }
    if (count > SIZE_MAX / sizeof (*code))
      {
        s = MULIB_MIR_RESOURCE_LIMIT;
        goto done;
      }
    code = (mulib_minivm_instruction_t *)malloc (count * sizeof (*code));
    if (!code)
      {
        s = MULIB_MIR_NO_MEMORY;
        goto done;
      }
    for (k = 0; k < p->count; ++k)
      {
        const mulib_mir_instruction_t *i = &p->code[k];
        int arity = mulib_mir_arity (i->opcode);
        if (!arity)
          {
            code[at].opcode = MULIB_MINIVM_PUSH;
            code[at++].operand = i->opcode == MULIB_MIR_CONST
                                     ? i->immediate
                                     : args[(size_t)i->immediate];
          }
        else
          {
            code[at].opcode = MULIB_MINIVM_LOAD;
            code[at++].operand = (int64_t)i->a;
            if (arity == 2)
              {
                code[at].opcode = MULIB_MINIVM_LOAD;
                code[at++].operand = (int64_t)i->b;
              }
            if (i->opcode != MULIB_MIR_COPY && i->opcode != MULIB_MIR_RETURN)
              {
                code[at].opcode = mulib_cmachine__opcode (i->opcode);
                code[at++].operand = 0;
              }
          }
        code[at].opcode = i->opcode == MULIB_MIR_RETURN ? MULIB_MINIVM_HALT
                                                        : MULIB_MINIVM_STORE;
        code[at++].operand = i->opcode == MULIB_MIR_RETURN ? 0 : (int64_t)k;
      }
    out->code = code;
    out->program.code = code;
    out->program.code_count = at;
    out->program.local_count = p->count - 1;
    out->program.hosts = NULL;
    out->program.host_count = 0;
    s = MULIB_MIR_OK;
  done:
    if (d)
      {
        d->status = s;
        d->record = k;
      }
    return s;
  }
  mulib_minivm_status_t
  mulib_cmachine_run (const mulib_cmachine_t *m,
                      const mulib_minivm_limits_t *l, mulib_minivm_result_t *r)
  {
    return mulib_minivm_run (m ? &m->program : NULL, l, r);
  }
  void
  mulib_cmachine_free (mulib_cmachine_t *m)
  {
    if (m)
      {
        free (m->code);
        memset (m, 0, sizeof (*m));
      }
  }
#ifdef __cplusplus
}
#endif
#endif
