/** @file mulib-vmweave.h
 * @brief Fault-preserving constant folding, copy propagation and dead MIR
 * elimination. Define MULIB_IMPL_VMWEAVE; explicitly implement MIR as well.
 */
#ifndef MULIB_VMWEAVE_H
#define MULIB_VMWEAVE_H
#include "mulib-mir.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** Optimize valid MIR into an empty owned buffer. record_limit bounds input
   * and scratch. Dead potentially trapping arithmetic and PARAM Boolean checks
   * are retained. Faulting constant expressions remain operations;
   * optimization itself does not execute them. Failure leaves output empty.
   * Input/output must not alias.
   */
  mulib_mir_status_t
  mulib_vmweave_optimize (const mulib_mir_program_t *program,
                          size_t record_limit, mulib_mir_buffer_t *output,
                          mulib_mir_diagnostic_t *diagnostic);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VMWEAVE) && !defined(MULIB_VMWEAVE_IMPLEMENTED)
#define MULIB_VMWEAVE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_mir_status_t
  mulib_vmweave_optimize (const mulib_mir_program_t *p, size_t limit,
                          mulib_mir_buffer_t *out, mulib_mir_diagnostic_t *d)
  {
    mulib_mir_buffer_t temp = { NULL, 0, 0, 0 }, result = { NULL, 0, 0, 0 };
    size_t *map = NULL, k = 0;
    unsigned char *live = NULL;
    mulib_mir_status_t s;
    if (!out || out->code || out->count || out->capacity)
      {
        s = MULIB_MIR_INVALID;
        goto done;
      }
    s = mulib_mir_validate (p, limit, d);
    if (s != MULIB_MIR_OK)
      return s;
    if (p->count > SIZE_MAX / sizeof (*map))
      {
        s = MULIB_MIR_RESOURCE_LIMIT;
        goto done;
      }
    s = mulib_mir_buffer_init (&temp, p->count, p->parameter_count);
    if (s != MULIB_MIR_OK)
      goto done;
    map = (size_t *)malloc (p->count * sizeof (*map));
    live = (unsigned char *)calloc (p->count, 1);
    if (!map || !live)
      {
        s = MULIB_MIR_NO_MEMORY;
        goto done;
      }
    for (k = 0; k < p->count; ++k)
      {
        mulib_mir_instruction_t i = p->code[k];
        int arity = mulib_mir_arity (i.opcode);
        int64_t value = 0;
        if (arity)
          i.a = map[i.a];
        if (arity == 2)
          i.b = map[i.b];
        if (i.opcode == MULIB_MIR_COPY)
          {
            map[k] = i.a;
            continue;
          }
        if (arity && i.opcode != MULIB_MIR_RETURN
            && temp.code[i.a].opcode == MULIB_MIR_CONST
            && (arity == 1 || temp.code[i.b].opcode == MULIB_MIR_CONST)
            && mulib_mir_evaluate (i.opcode, temp.code[i.a].immediate,
                                   arity == 2 ? temp.code[i.b].immediate : 0,
                                   0, &value)
                   == MULIB_MIR_OK)
          {
            i.opcode = MULIB_MIR_CONST;
            i.a = i.b = MULIB_MIR_NONE;
            i.immediate = value;
          }
        map[k] = temp.count;
        temp.code[temp.count++] = i;
      }
    for (k = temp.count; k-- > 0;)
      {
        mulib_mir_instruction_t *i = &temp.code[k];
        int arity = mulib_mir_arity (i->opcode);
        int trapping
            = i->opcode >= MULIB_MIR_ADD && i->opcode <= MULIB_MIR_NEG;
        if (i->opcode == MULIB_MIR_RETURN || i->opcode == MULIB_MIR_PARAM
            || trapping)
          live[k] = 1;
        if (live[k])
          {
            if (arity)
              live[i->a] = 1;
            if (arity == 2)
              live[i->b] = 1;
          }
      }
    s = mulib_mir_buffer_init (&result, temp.count, p->parameter_count);
    if (s != MULIB_MIR_OK)
      goto done;
    for (k = 0; k < temp.count; ++k)
      if (live[k])
        {
          mulib_mir_instruction_t i = temp.code[k];
          int arity = mulib_mir_arity (i.opcode);
          if (arity)
            i.a = map[i.a];
          if (arity == 2)
            i.b = map[i.b];
          map[k] = result.count;
          result.code[result.count++] = i;
        }
    *out = result;
    memset (&result, 0, sizeof (result));
    s = MULIB_MIR_OK;
  done:
    free (map);
    free (live);
    mulib_mir_buffer_free (&temp);
    mulib_mir_buffer_free (&result);
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
