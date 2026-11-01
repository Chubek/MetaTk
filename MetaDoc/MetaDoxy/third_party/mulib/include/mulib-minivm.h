/**
 * @file mulib-minivm.h
 * @brief A bounded, checked signed-64-bit stack bytecode interpreter.
 *
 * Instructions are C records, not a portable serialized bytecode format.
 * Define MULIB_IMPL_MINIVM in one translation unit; no other MuLIB dependencies.
 */
#ifndef MULIB_MINIVM_H
#define MULIB_MINIVM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/** @brief Binary operations consume left then right; comparisons push 0 or 1. */
typedef enum mulib_minivm_opcode {
    MULIB_MINIVM_HALT, MULIB_MINIVM_PUSH, MULIB_MINIVM_DROP,
    MULIB_MINIVM_DUP, MULIB_MINIVM_SWAP, MULIB_MINIVM_LOAD, MULIB_MINIVM_STORE,
    MULIB_MINIVM_ADD, MULIB_MINIVM_SUB, MULIB_MINIVM_MUL, MULIB_MINIVM_DIV,
    MULIB_MINIVM_MOD, MULIB_MINIVM_NEG, MULIB_MINIVM_EQ, MULIB_MINIVM_LT,
    MULIB_MINIVM_LE, MULIB_MINIVM_NOT, MULIB_MINIVM_JMP, MULIB_MINIVM_JZ,
    MULIB_MINIVM_JNZ, MULIB_MINIVM_CALL
} mulib_minivm_opcode_t;

/** @brief Operand is a literal, local index, absolute jump target, or host index. */
typedef struct mulib_minivm_instruction {
    mulib_minivm_opcode_t opcode;
    int64_t operand;
} mulib_minivm_instruction_t;

/**
 * @brief A host function: return 0 on success and write one result, else fault.
 * @details Arguments are borrowed, read-only, and ordered left to right. Do not
 * retain their pointer. Calls may have side effects; VM faults do not undo them.
 */
typedef int (*mulib_minivm_host_fn)(void *user, const int64_t *arguments,
                                  size_t count, int64_t *result);
/** @brief Borrowed host callback descriptor. */
typedef struct mulib_minivm_host {
    mulib_minivm_host_fn function;
    void *user;
    size_t argument_count;
} mulib_minivm_host_t;
/** @brief Borrowed immutable program; each run starts with zeroed locals. */
typedef struct mulib_minivm_program {
    const mulib_minivm_instruction_t *code;
    size_t code_count, local_count;
    const mulib_minivm_host_t *hosts;
    size_t host_count;
} mulib_minivm_program_t;
/** @brief Hard per-run allocation and instruction limits; zero is an actual zero. */
typedef struct mulib_minivm_limits {
    size_t stack, locals, steps;
} mulib_minivm_limits_t;

/** @brief All VM failures are explicit, including signed arithmetic overflow. */
typedef enum mulib_minivm_status {
    MULIB_MINIVM_OK, MULIB_MINIVM_INVALID_PROGRAM, MULIB_MINIVM_NO_MEMORY,
    MULIB_MINIVM_RESOURCE_LIMIT, MULIB_MINIVM_STACK_OVERFLOW,
    MULIB_MINIVM_STACK_UNDERFLOW, MULIB_MINIVM_BAD_OPCODE,
    MULIB_MINIVM_BAD_JUMP, MULIB_MINIVM_BAD_LOCAL, MULIB_MINIVM_BAD_HOST,
    MULIB_MINIVM_OVERFLOW, MULIB_MINIVM_DIVIDE_BY_ZERO, MULIB_MINIVM_STEP_LIMIT,
    MULIB_MINIVM_HOST_ERROR, MULIB_MINIVM_FELL_OFF_END
} mulib_minivm_status_t;
/** @brief Snapshot at HALT or fault; value is stack top, or zero for empty stack. */
typedef struct mulib_minivm_result {
    mulib_minivm_status_t status;
    size_t pc, steps, stack_size;
    int64_t value;
    int host_status;
} mulib_minivm_result_t;

/** @brief Return default limits: 1024 stack slots, 1024 locals, 100000 steps. */
mulib_minivm_limits_t mulib_minivm_default_limits(void);
/**
 * @brief Run synchronously, release all temporary memory, and return a status.
 * @details NULL limits selects defaults; result may be NULL. A faulting
 * instruction counts as a step, but a step-limit fault does not. pc is the
 * faulting instruction (or code_count on fallthrough). Invalid operands are
 * checked when executed, including untaken conditional jump targets. Faulting
 * VM instructions do not change the stack or locals. HALT may use an empty stack.
 */
mulib_minivm_status_t mulib_minivm_run(const mulib_minivm_program_t *program,
    const mulib_minivm_limits_t *limits, mulib_minivm_result_t *result);
/** @brief Return a static status description. */
const char *mulib_minivm_status_string(mulib_minivm_status_t status);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MINIVM_H */

#if defined(MULIB_IMPL_MINIVM) && !defined(MULIB_MINIVM_IMPLEMENTED)
#define MULIB_MINIVM_IMPLEMENTED
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
mulib_minivm_limits_t mulib_minivm_default_limits(void) {
    mulib_minivm_limits_t limits = { 1024, 1024, 100000 };
    return limits;
}

static int mulib_minivm__index(int64_t operand, size_t count) {
    return operand >= 0 && (uintmax_t)operand < (uintmax_t)count;
}

static int mulib_minivm__arithmetic(mulib_minivm_opcode_t op,
    int64_t a, int64_t b, int64_t *out) {
    switch (op) {
    case MULIB_MINIVM_ADD:
        if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return 0;
        *out = a + b; break;
    case MULIB_MINIVM_SUB:
        if ((b > 0 && a < INT64_MIN + b) || (b < 0 && a > INT64_MAX + b)) return 0;
        *out = a - b; break;
    case MULIB_MINIVM_MUL:
        if (a > 0) {
            if ((b > 0 && a > INT64_MAX / b) || (b < 0 && b < INT64_MIN / a)) return 0;
        } else if (a < 0) {
            if ((b > 0 && a < INT64_MIN / b) || (b < 0 && a < INT64_MAX / b)) return 0;
        }
        *out = a * b; break;
    case MULIB_MINIVM_DIV:
    case MULIB_MINIVM_MOD:
        if (a == INT64_MIN && b == -1) return 0;
        *out = op == MULIB_MINIVM_DIV ? a / b : a % b; break;
    case MULIB_MINIVM_EQ: *out = a == b; break;
    case MULIB_MINIVM_LT: *out = a < b; break;
    case MULIB_MINIVM_LE: *out = a <= b; break;
    default: return 0;
    }
    return 1;
}

mulib_minivm_status_t mulib_minivm_run(const mulib_minivm_program_t *p,
    const mulib_minivm_limits_t *requested, mulib_minivm_result_t *result) {
    mulib_minivm_limits_t limits = requested ? *requested : mulib_minivm_default_limits();
    mulib_minivm_result_t r = { MULIB_MINIVM_OK, 0, 0, 0, 0, 0 };
    int64_t *stack = NULL, *locals = NULL;
    size_t sp = 0, pc = 0;
    if (!p || !p->code || !p->code_count || (!p->hosts && p->host_count)) {
        r.status = MULIB_MINIVM_INVALID_PROGRAM;
        goto done;
    }
    if (p->local_count > limits.locals || limits.stack > SIZE_MAX / sizeof(*stack) ||
        p->local_count > SIZE_MAX / sizeof(*locals)) {
        r.status = MULIB_MINIVM_RESOURCE_LIMIT;
        goto done;
    }
    if (limits.stack) stack = (int64_t *)malloc(limits.stack * sizeof(*stack));
    if (p->local_count) locals = (int64_t *)calloc(p->local_count, sizeof(*locals));
    if ((limits.stack && !stack) || (p->local_count && !locals)) {
        r.status = MULIB_MINIVM_NO_MEMORY;
        goto done;
    }
    for (;;) {
        mulib_minivm_instruction_t ins;
        int64_t value;
        size_t index;
        if (pc >= p->code_count) { r.status = MULIB_MINIVM_FELL_OFF_END; break; }
        if (r.steps >= limits.steps) { r.status = MULIB_MINIVM_STEP_LIMIT; break; }
        ins = p->code[pc];
        ++r.steps;
        switch (ins.opcode) {
        case MULIB_MINIVM_HALT: goto done;
        case MULIB_MINIVM_PUSH:
            if (sp == limits.stack) { r.status = MULIB_MINIVM_STACK_OVERFLOW; goto done; }
            stack[sp++] = ins.operand;
            break;
        case MULIB_MINIVM_DROP:
            if (!sp) { r.status = MULIB_MINIVM_STACK_UNDERFLOW; goto done; }
            --sp;
            break;
        case MULIB_MINIVM_DUP:
            if (!sp) { r.status = MULIB_MINIVM_STACK_UNDERFLOW; goto done; }
            if (sp == limits.stack) { r.status = MULIB_MINIVM_STACK_OVERFLOW; goto done; }
            stack[sp] = stack[sp - 1];
            ++sp;
            break;
        case MULIB_MINIVM_SWAP:
            if (sp < 2) { r.status = MULIB_MINIVM_STACK_UNDERFLOW; goto done; }
            value = stack[sp - 1]; stack[sp - 1] = stack[sp - 2]; stack[sp - 2] = value;
            break;
        case MULIB_MINIVM_LOAD:
        case MULIB_MINIVM_STORE:
            if (!mulib_minivm__index(ins.operand, p->local_count)) {
                r.status = MULIB_MINIVM_BAD_LOCAL; goto done;
            }
            index = (size_t)ins.operand;
            if (ins.opcode == MULIB_MINIVM_LOAD) {
                if (sp == limits.stack) { r.status = MULIB_MINIVM_STACK_OVERFLOW; goto done; }
                stack[sp++] = locals[index];
            } else {
                if (!sp) { r.status = MULIB_MINIVM_STACK_UNDERFLOW; goto done; }
                locals[index] = stack[--sp];
            }
            break;
        case MULIB_MINIVM_ADD: case MULIB_MINIVM_SUB: case MULIB_MINIVM_MUL:
        case MULIB_MINIVM_DIV: case MULIB_MINIVM_MOD: case MULIB_MINIVM_EQ:
        case MULIB_MINIVM_LT: case MULIB_MINIVM_LE:
            if (sp < 2) { r.status = MULIB_MINIVM_STACK_UNDERFLOW; goto done; }
            if ((ins.opcode == MULIB_MINIVM_DIV || ins.opcode == MULIB_MINIVM_MOD) && !stack[sp - 1]) {
                r.status = MULIB_MINIVM_DIVIDE_BY_ZERO; goto done;
            }
            if (!mulib_minivm__arithmetic(ins.opcode, stack[sp - 2], stack[sp - 1], &value)) {
                r.status = MULIB_MINIVM_OVERFLOW; goto done;
            }
            stack[sp - 2] = value;
            --sp;
            break;
        case MULIB_MINIVM_NEG:
        case MULIB_MINIVM_NOT:
            if (!sp) { r.status = MULIB_MINIVM_STACK_UNDERFLOW; goto done; }
            if (ins.opcode == MULIB_MINIVM_NEG && stack[sp - 1] == INT64_MIN) {
                r.status = MULIB_MINIVM_OVERFLOW; goto done;
            }
            stack[sp - 1] = ins.opcode == MULIB_MINIVM_NEG ? -stack[sp - 1] : !stack[sp - 1];
            break;
        case MULIB_MINIVM_JMP: case MULIB_MINIVM_JZ: case MULIB_MINIVM_JNZ:
            if (!mulib_minivm__index(ins.operand, p->code_count)) {
                r.status = MULIB_MINIVM_BAD_JUMP; goto done;
            }
            if (ins.opcode != MULIB_MINIVM_JMP && !sp) {
                r.status = MULIB_MINIVM_STACK_UNDERFLOW; goto done;
            }
            if (ins.opcode == MULIB_MINIVM_JMP) { pc = (size_t)ins.operand; continue; }
            value = stack[--sp];
            if ((ins.opcode == MULIB_MINIVM_JZ && !value) ||
                (ins.opcode == MULIB_MINIVM_JNZ && value)) { pc = (size_t)ins.operand; continue; }
            break;
        case MULIB_MINIVM_CALL:
            if (!mulib_minivm__index(ins.operand, p->host_count) ||
                !p->hosts[(size_t)ins.operand].function) { r.status = MULIB_MINIVM_BAD_HOST; goto done; }
            index = (size_t)ins.operand;
            if (sp < p->hosts[index].argument_count) { r.status = MULIB_MINIVM_STACK_UNDERFLOW; goto done; }
            if (sp - p->hosts[index].argument_count == limits.stack) {
                r.status = MULIB_MINIVM_STACK_OVERFLOW; goto done;
            }
            value = 0;
            r.host_status = p->hosts[index].function(p->hosts[index].user,
                stack ? stack + sp - p->hosts[index].argument_count : NULL,
                p->hosts[index].argument_count, &value);
            if (r.host_status) { r.status = MULIB_MINIVM_HOST_ERROR; goto done; }
            sp -= p->hosts[index].argument_count;
            stack[sp++] = value;
            break;
        default: r.status = MULIB_MINIVM_BAD_OPCODE; goto done;
        }
        ++pc;
    }
done:
    r.pc = pc;
    r.stack_size = sp;
    if (sp) r.value = stack[sp - 1];
    free(stack);
    free(locals);
    if (result) *result = r;
    return r.status;
}

const char *mulib_minivm_status_string(mulib_minivm_status_t s) {
    switch (s) {
    case MULIB_MINIVM_OK: return "halted";
    case MULIB_MINIVM_INVALID_PROGRAM: return "invalid program descriptor";
    case MULIB_MINIVM_NO_MEMORY: return "allocation failed";
    case MULIB_MINIVM_RESOURCE_LIMIT: return "resource limit exceeded";
    case MULIB_MINIVM_STACK_OVERFLOW: return "stack limit exceeded";
    case MULIB_MINIVM_STACK_UNDERFLOW: return "stack underflow";
    case MULIB_MINIVM_BAD_OPCODE: return "unknown opcode";
    case MULIB_MINIVM_BAD_JUMP: return "invalid absolute jump target";
    case MULIB_MINIVM_BAD_LOCAL: return "invalid local index";
    case MULIB_MINIVM_BAD_HOST: return "invalid host function";
    case MULIB_MINIVM_OVERFLOW: return "signed arithmetic overflow";
    case MULIB_MINIVM_DIVIDE_BY_ZERO: return "division by zero";
    case MULIB_MINIVM_STEP_LIMIT: return "instruction limit exceeded";
    case MULIB_MINIVM_HOST_ERROR: return "host function failed";
    case MULIB_MINIVM_FELL_OFF_END: return "execution fell off program end";
    default: return "unknown VM status";
    }
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
