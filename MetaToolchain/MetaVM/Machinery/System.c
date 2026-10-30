#include "Internal.h"
#include <stdarg.h>
#include <stdio.h>

mvm_limits mvm_default_limits(void) {
    mvm_limits l = {1024, 1048576, 64u * 1024u * 1024u,
                    16u * 1024u * 1024u, UINT64_C(10000000)};
    return l;
}
const char *mvm_status_string(mvm_status s) {
    static const char *const names[] = {
        "success", "invalid argument", "out of memory", "resource limit",
        "I/O error", "invalid format", "verification failed", "type error",
        "out of bounds", "division by zero", "not found", "host error", "VM busy"
    };
    return (unsigned)s < sizeof(names) / sizeof(names[0]) ? names[s] : "unknown status";
}
void mvm_error_clear(mvm_error *e) {
    if (e) {
        memset(e, 0, sizeof(*e));
        e->function = e->instruction = SIZE_MAX;
    }
}
mvm_status mvm_fail(mvm_error *e, mvm_status s, const char *format, ...) {
    if (e) {
        va_list ap;
        e->status = s;
        va_start(ap, format);
        (void)vsnprintf(e->message, sizeof(e->message), format, ap);
        va_end(ap);
    }
    return s;
}
int mvm_name_valid(const char *s) {
    size_t n;
    if (!s || !*s) return 0;
    for (n = 0; s[n]; ++n) {
        unsigned char c = (unsigned char)s[n];
        if (n >= 255) return 0;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') continue;
        if (n && c >= '0' && c <= '9') continue;
        return 0;
    }
    return 1;
}
char *mvm_copy_string(const char *s) {
    size_t n = strlen(s);
    char *p;
    if (n == SIZE_MAX) return NULL;
    p = malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}
mvm_status mvm_grow(void **data, size_t *capacity, size_t need, size_t element) {
    size_t n;
    void *p;
    if (!element || need > SIZE_MAX / element) return MVM_LIMIT;
    if (need <= *capacity) return MVM_OK;
    n = *capacity ? *capacity : 8;
    while (n < need) {
        if (n > SIZE_MAX / 2) { n = need; break; }
        n *= 2;
    }
    if (n > SIZE_MAX / element) n = need;
    p = realloc(*data, n * element);
    if (!p) return MVM_OOM;
    *data = p;
    *capacity = n;
    return MVM_OK;
}
int64_t mvm_from_bits(uint64_t v) {
    if (v <= INT64_MAX) return (int64_t)v;
    return -1 - (int64_t)(UINT64_MAX - v);
}
mvm_value mvm_nil(void) { mvm_value v = {MVM_NIL, {0}}; return v; }
mvm_value mvm_bool(int b) { mvm_value v = {MVM_BOOL, {0}}; v.as.boolean = !!b; return v; }
mvm_value mvm_int(int64_t i) { mvm_value v = {MVM_INT, {0}}; v.as.integer = i; return v; }
int mvm_value_equal(mvm_value a, mvm_value b) {
    if (a.kind != b.kind) return 0;
    switch (a.kind) {
    case MVM_NIL: return 1;
    case MVM_BOOL: return !!a.as.boolean == !!b.as.boolean;
    case MVM_INT: return a.as.integer == b.as.integer;
    case MVM_OBJECT: return a.as.object == b.as.object;
    default: return 0;
    }
}
int mvm_value_truthy(mvm_value v) {
    return v.kind != MVM_NIL && (v.kind != MVM_BOOL || v.as.boolean != 0);
}
const char *mvm_opcode_name(mvm_opcode op) {
    static const char *const names[] = {
        "nop", "const", "nil", "bool", "mov", "add", "sub", "mul", "div", "mod",
        "and", "or", "xor", "shl", "shr", "eq", "lt", "le", "not", "jmp", "jz",
        "call", "host", "ret", "array", "get", "set", "len"
    };
    return (unsigned)op < MVM_OP_COUNT ? names[op] : NULL;
}
mvm_status mvm_integer_eval(mvm_opcode op, int64_t a, int64_t b, int64_t *out) {
    uint64_t x = (uint64_t)a, y = (uint64_t)b, z;
    if (!out) return MVM_INVALID;
    switch (op) {
    case MVM_OP_ADD: z = x + y; break;
    case MVM_OP_SUB: z = x - y; break;
    case MVM_OP_MUL: z = x * y; break;
    case MVM_OP_DIV:
    case MVM_OP_MOD:
        if (!b) return MVM_DIV_ZERO;
        if (a == INT64_MIN && b == -1) { *out = op == MVM_OP_DIV ? INT64_MIN : 0; return MVM_OK; }
        *out = op == MVM_OP_DIV ? a / b : a % b;
        return MVM_OK;
    case MVM_OP_AND: z = x & y; break;
    case MVM_OP_OR: z = x | y; break;
    case MVM_OP_XOR: z = x ^ y; break;
    case MVM_OP_SHL:
    case MVM_OP_SHR:
        if (b < 0 || b > 63) return MVM_BOUNDS;
        if (op == MVM_OP_SHL) z = x << (unsigned)b;
        else {
            z = x >> (unsigned)b;
            if (a < 0 && b) z |= UINT64_MAX << (64 - (unsigned)b);
        }
        break;
    case MVM_OP_EQ: *out = a == b; return MVM_OK;
    case MVM_OP_LT: *out = a < b; return MVM_OK;
    case MVM_OP_LE: *out = a <= b; return MVM_OK;
    default: return MVM_INVALID;
    }
    *out = mvm_from_bits(z);
    return MVM_OK;
}
mvm_status mvm_select_integer_operation(const char *name, mvm_opcode *op) {
    unsigned i;
    if (!name || !op) return MVM_INVALID;
    for (i = MVM_OP_ADD; i <= MVM_OP_LE; ++i) {
        if (!strcmp(name, mvm_opcode_name((mvm_opcode)i))) { *op = (mvm_opcode)i; return MVM_OK; }
    }
    return MVM_NOT_FOUND;
}
mvm_status mvm_fold_integer_operation(const char *name, int64_t a, int64_t b, int64_t *out) {
    mvm_opcode op;
    mvm_status s = mvm_select_integer_operation(name, &op);
    return s == MVM_OK ? mvm_integer_eval(op, a, b, out) : s;
}
