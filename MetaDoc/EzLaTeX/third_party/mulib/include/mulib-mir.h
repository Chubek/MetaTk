/** @file mulib-mir.h
 * @brief Typed, checked, straight-line SSA compiler records and bounded
 * ownership. Define MULIB_IMPL_MIR in one translation unit. No implementation
 * dependencies.
 */
#ifndef MULIB_MIR_H
#define MULIB_MIR_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
#define MULIB_MIR_NONE SIZE_MAX
  typedef enum mulib_mir_type
  {
    MULIB_MIR_VOID,
    MULIB_MIR_I64,
    MULIB_MIR_BOOL
  } mulib_mir_type_t;
  typedef enum mulib_mir_opcode
  {
    MULIB_MIR_CONST,
    MULIB_MIR_PARAM,
    MULIB_MIR_COPY,
    MULIB_MIR_ADD,
    MULIB_MIR_SUB,
    MULIB_MIR_MUL,
    MULIB_MIR_DIV,
    MULIB_MIR_MOD,
    MULIB_MIR_NEG,
    MULIB_MIR_EQ,
    MULIB_MIR_LT,
    MULIB_MIR_LE,
    MULIB_MIR_NOT,
    MULIB_MIR_RETURN
  } mulib_mir_opcode_t;
  /** Record index is its SSA value ID. Unused operands must be NONE, unused
   * immediate zero. */
  typedef struct mulib_mir_instruction
  {
    mulib_mir_opcode_t opcode;
    mulib_mir_type_t type;
    size_t a, b;
    int64_t immediate;
  } mulib_mir_instruction_t;
  /** Borrowed immutable records. PARAM immediate indexes the argument array.
   */
  typedef struct mulib_mir_program
  {
    const mulib_mir_instruction_t *code;
    size_t count, parameter_count;
  } mulib_mir_program_t;
  typedef enum mulib_mir_status
  {
    MULIB_MIR_OK,
    MULIB_MIR_INVALID,
    MULIB_MIR_BAD_OPCODE,
    MULIB_MIR_BAD_REFERENCE,
    MULIB_MIR_BAD_TYPE,
    MULIB_MIR_BAD_RETURN,
    MULIB_MIR_RESOURCE_LIMIT,
    MULIB_MIR_NO_MEMORY,
    MULIB_MIR_OVERFLOW,
    MULIB_MIR_DIVIDE_BY_ZERO
  } mulib_mir_status_t;
  typedef struct mulib_mir_diagnostic
  {
    mulib_mir_status_t status;
    size_t record;
  } mulib_mir_diagnostic_t;
  /** Owned fixed-capacity builder. Do not copy a live buffer; zero initialize
   * before init. */
  typedef struct mulib_mir_buffer
  {
    mulib_mir_instruction_t *code;
    size_t count, capacity, parameter_count;
  } mulib_mir_buffer_t;
  /** Allocate capacity records; zero capacity is allowed. Destination must be
   * empty. */
  mulib_mir_status_t mulib_mir_buffer_init (mulib_mir_buffer_t *buffer,
                                            size_t capacity,
                                            size_t parameters);
  /** Copy a record without validation; failure leaves the buffer unchanged. */
  mulib_mir_status_t mulib_mir_append (mulib_mir_buffer_t *buffer,
                                       mulib_mir_instruction_t instruction);
  /** Borrow a view, invalidated by free. */
  mulib_mir_program_t mulib_mir_view (const mulib_mir_buffer_t *buffer);
  /** Release records and zero the object; NULL is harmless. */
  void mulib_mir_buffer_free (mulib_mir_buffer_t *buffer);
  /** Validate every record, including dead records; earlier-only references
   * rule out cycles. */
  mulib_mir_status_t mulib_mir_validate (const mulib_mir_program_t *program,
                                         size_t record_limit,
                                         mulib_mir_diagnostic_t *diagnostic);
  /** Operand count, or -1 for an unknown opcode. */
  int mulib_mir_arity (mulib_mir_opcode_t opcode);
  /** Evaluate a scalar operation; CONST uses immediate. PARAM/RETURN are
   * invalid here. On error *value is unchanged. Typed callers must first
   * validate their records.
   */
  mulib_mir_status_t mulib_mir_evaluate (mulib_mir_opcode_t opcode, int64_t a,
                                         int64_t b, int64_t immediate,
                                         int64_t *value);
  /** Static diagnostic description. */
  const char *mulib_mir_status_string (mulib_mir_status_t status);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MIR) && !defined(MULIB_MIR_IMPLEMENTED)
#define MULIB_MIR_IMPLEMENTED
#include <stdlib.h>
#ifdef __cplusplus
extern "C"
{
#endif
  mulib_mir_status_t
  mulib_mir_buffer_init (mulib_mir_buffer_t *b, size_t capacity,
                         size_t parameters)
  {
    mulib_mir_instruction_t *p = NULL;
    if (!b || b->code || b->count || b->capacity)
      return MULIB_MIR_INVALID;
    if (capacity > SIZE_MAX / sizeof (*p))
      return MULIB_MIR_RESOURCE_LIMIT;
    if (capacity)
      {
        p = (mulib_mir_instruction_t *)malloc (capacity * sizeof (*p));
        if (!p)
          return MULIB_MIR_NO_MEMORY;
      }
    b->code = p;
    b->capacity = capacity;
    b->parameter_count = parameters;
    return MULIB_MIR_OK;
  }
  mulib_mir_status_t
  mulib_mir_append (mulib_mir_buffer_t *b, mulib_mir_instruction_t i)
  {
    if (!b || b->count > b->capacity || (b->capacity && !b->code))
      return MULIB_MIR_INVALID;
    if (b->count == b->capacity)
      return MULIB_MIR_RESOURCE_LIMIT;
    b->code[b->count++] = i;
    return MULIB_MIR_OK;
  }
  mulib_mir_program_t
  mulib_mir_view (const mulib_mir_buffer_t *b)
  {
    mulib_mir_program_t p = { NULL, 0, 0 };
    if (b)
      {
        p.code = b->code;
        p.count = b->count;
        p.parameter_count = b->parameter_count;
      }
    return p;
  }
  void
  mulib_mir_buffer_free (mulib_mir_buffer_t *b)
  {
    if (b)
      {
        free (b->code);
        b->code = NULL;
        b->count = b->capacity = b->parameter_count = 0;
      }
  }
  int
  mulib_mir_arity (mulib_mir_opcode_t op)
  {
    switch (op)
      {
      case MULIB_MIR_CONST:
      case MULIB_MIR_PARAM:
        return 0;
      case MULIB_MIR_COPY:
      case MULIB_MIR_NEG:
      case MULIB_MIR_NOT:
      case MULIB_MIR_RETURN:
        return 1;
      case MULIB_MIR_ADD:
      case MULIB_MIR_SUB:
      case MULIB_MIR_MUL:
      case MULIB_MIR_DIV:
      case MULIB_MIR_MOD:
      case MULIB_MIR_EQ:
      case MULIB_MIR_LT:
      case MULIB_MIR_LE:
        return 2;
      default:
        return -1;
      }
  }
  mulib_mir_status_t
  mulib_mir_validate (const mulib_mir_program_t *p, size_t limit,
                      mulib_mir_diagnostic_t *d)
  {
    size_t k = 0;
    mulib_mir_status_t s = MULIB_MIR_OK;
    if (!p || !p->code || !p->count)
      {
        s = MULIB_MIR_INVALID;
        goto done;
      }
    if (p->count > limit || p->count > SIZE_MAX / sizeof (*p->code))
      {
        s = MULIB_MIR_RESOURCE_LIMIT;
        goto done;
      }
    for (k = 0; k < p->count; ++k)
      {
        const mulib_mir_instruction_t *i = &p->code[k];
        int arity = mulib_mir_arity (i->opcode);
        if (arity < 0)
          {
            s = MULIB_MIR_BAD_OPCODE;
            break;
          }
        if ((arity >= 1 && i->a >= k) || (arity == 2 && i->b >= k)
            || (arity == 0 && i->a != MULIB_MIR_NONE)
            || (arity < 2 && i->b != MULIB_MIR_NONE))
          {
            s = MULIB_MIR_BAD_REFERENCE;
            break;
          }
        if (i->opcode == MULIB_MIR_RETURN)
          {
            if (k != p->count - 1 || i->type != MULIB_MIR_VOID || i->immediate)
              {
                s = MULIB_MIR_BAD_RETURN;
                break;
              }
          }
        else if (i->type != MULIB_MIR_I64 && i->type != MULIB_MIR_BOOL)
          {
            s = MULIB_MIR_BAD_TYPE;
            break;
          }
        if (i->opcode != MULIB_MIR_CONST && i->opcode != MULIB_MIR_PARAM
            && i->immediate)
          {
            s = MULIB_MIR_INVALID;
            break;
          }
        if (i->opcode == MULIB_MIR_CONST && i->type == MULIB_MIR_BOOL
            && i->immediate != 0 && i->immediate != 1)
          {
            s = MULIB_MIR_BAD_TYPE;
            break;
          }
        if (i->opcode == MULIB_MIR_PARAM
            && (i->immediate < 0
                || (uintmax_t)i->immediate >= (uintmax_t)p->parameter_count))
          {
            s = MULIB_MIR_BAD_REFERENCE;
            break;
          }
        if (arity && i->opcode != MULIB_MIR_RETURN)
          {
            mulib_mir_type_t a = p->code[i->a].type;
            mulib_mir_type_t b = arity == 2 ? p->code[i->b].type : a;
            int valid;
            if (i->opcode == MULIB_MIR_COPY)
              valid = i->type == a;
            else if (i->opcode == MULIB_MIR_NOT)
              valid = a == MULIB_MIR_BOOL && i->type == MULIB_MIR_BOOL;
            else if (i->opcode == MULIB_MIR_EQ)
              valid = a == b && i->type == MULIB_MIR_BOOL;
            else if (i->opcode == MULIB_MIR_LT || i->opcode == MULIB_MIR_LE)
              valid
                  = a == MULIB_MIR_I64 && b == a && i->type == MULIB_MIR_BOOL;
            else
              valid = a == MULIB_MIR_I64 && b == a && i->type == MULIB_MIR_I64;
            if (!valid)
              {
                s = MULIB_MIR_BAD_TYPE;
                break;
              }
          }
      }
    if (s == MULIB_MIR_OK && p->code[p->count - 1].opcode != MULIB_MIR_RETURN)
      {
        s = MULIB_MIR_BAD_RETURN;
        k = p->count - 1;
      }
  done:
    if (d)
      {
        d->status = s;
        d->record = k;
      }
    return s;
  }
  mulib_mir_status_t
  mulib_mir_evaluate (mulib_mir_opcode_t op, int64_t a, int64_t b, int64_t imm,
                      int64_t *out)
  {
    int64_t v;
    if (!out)
      return MULIB_MIR_INVALID;
    switch (op)
      {
      case MULIB_MIR_CONST:
        v = imm;
        break;
      case MULIB_MIR_COPY:
        v = a;
        break;
      case MULIB_MIR_ADD:
        if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b))
          return MULIB_MIR_OVERFLOW;
        v = a + b;
        break;
      case MULIB_MIR_SUB:
        if ((b > 0 && a < INT64_MIN + b) || (b < 0 && a > INT64_MAX + b))
          return MULIB_MIR_OVERFLOW;
        v = a - b;
        break;
      case MULIB_MIR_MUL:
        if (a > 0)
          {
            if ((b > 0 && a > INT64_MAX / b) || (b < 0 && b < INT64_MIN / a))
              return MULIB_MIR_OVERFLOW;
          }
        else if (a < 0)
          {
            if ((b > 0 && a < INT64_MIN / b) || (b < 0 && a < INT64_MAX / b))
              return MULIB_MIR_OVERFLOW;
          }
        v = a * b;
        break;
      case MULIB_MIR_DIV:
      case MULIB_MIR_MOD:
        if (!b)
          return MULIB_MIR_DIVIDE_BY_ZERO;
        if (a == INT64_MIN && b == -1)
          return MULIB_MIR_OVERFLOW;
        v = op == MULIB_MIR_DIV ? a / b : a % b;
        break;
      case MULIB_MIR_NEG:
        if (a == INT64_MIN)
          return MULIB_MIR_OVERFLOW;
        v = -a;
        break;
      case MULIB_MIR_EQ:
        v = a == b;
        break;
      case MULIB_MIR_LT:
        v = a < b;
        break;
      case MULIB_MIR_LE:
        v = a <= b;
        break;
      case MULIB_MIR_NOT:
        v = !a;
        break;
      default:
        return MULIB_MIR_INVALID;
      }
    *out = v;
    return MULIB_MIR_OK;
  }
  const char *
  mulib_mir_status_string (mulib_mir_status_t s)
  {
    switch (s)
      {
      case MULIB_MIR_OK:
        return "ok";
      case MULIB_MIR_INVALID:
        return "invalid descriptor or immediate";
      case MULIB_MIR_BAD_OPCODE:
        return "unknown opcode";
      case MULIB_MIR_BAD_REFERENCE:
        return "invalid, forward, cyclic, or unused reference";
      case MULIB_MIR_BAD_TYPE:
        return "type mismatch or noncanonical Boolean";
      case MULIB_MIR_BAD_RETURN:
        return "expected one final void RETURN";
      case MULIB_MIR_RESOURCE_LIMIT:
        return "resource or allocation-size limit";
      case MULIB_MIR_NO_MEMORY:
        return "allocation failed";
      case MULIB_MIR_OVERFLOW:
        return "signed arithmetic overflow";
      case MULIB_MIR_DIVIDE_BY_ZERO:
        return "division by zero";
      default:
        return "unknown MIR status";
      }
  }
#ifdef __cplusplus
}
#endif
#endif
