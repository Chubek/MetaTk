#ifndef METAVM_ABSTRACT_MACHINE_H
#define METAVM_ABSTRACT_MACHINE_H
#include "Value.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Registers are local to a function. Branch targets are instruction indices.
 * Arithmetic is signed i64 with modulo-2^64 add/sub/mul and bit operations.
 * Division truncates toward zero; INT64_MIN/-1 wraps to INT64_MIN.
 * Shift counts must be in [0,63]; SHR is an arithmetic right shift. */
typedef enum mvm_opcode {
    MVM_OP_NOP, MVM_OP_CONST, MVM_OP_NIL, MVM_OP_BOOL, MVM_OP_MOV,
    MVM_OP_ADD, MVM_OP_SUB, MVM_OP_MUL, MVM_OP_DIV, MVM_OP_MOD,
    MVM_OP_AND, MVM_OP_OR, MVM_OP_XOR, MVM_OP_SHL, MVM_OP_SHR,
    MVM_OP_EQ, MVM_OP_LT, MVM_OP_LE, MVM_OP_NOT,
    MVM_OP_JMP, MVM_OP_JZ, MVM_OP_CALL, MVM_OP_HOST, MVM_OP_RET,
    MVM_OP_ARRAY, MVM_OP_GET, MVM_OP_SET, MVM_OP_LEN, MVM_OP_COUNT
} mvm_opcode;
typedef struct mvm_instruction {
    mvm_opcode op;
    uint32_t a, b, c;
    int64_t immediate;
} mvm_instruction;
/* CONST/BOOL a,immediate; NIL a; MOV/NOT/LEN/ARRAY a,b;
 * arithmetic/comparison/GET/SET a,b,c (SET: array=a,index=b,value=c);
 * JMP immediate; JZ a,immediate; CALL/HOST a,b,c:
 * destination=a, function/import index=b, first argument register=c.
 * Arity is taken from the callee. RET a. */
const char *mvm_opcode_name(mvm_opcode op);
/* Shared, defined integer semantics used by execution and constant folding. */
mvm_status mvm_integer_eval(mvm_opcode op, int64_t a, int64_t b, int64_t *result);
#ifdef __cplusplus
}
#endif
#endif
