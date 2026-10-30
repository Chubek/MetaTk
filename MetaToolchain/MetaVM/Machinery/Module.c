#include "Internal.h"

mvm_module *mvm_module_create(void) { return calloc(1, sizeof(mvm_module)); }
void mvm_module_destroy(mvm_module *m) {
    size_t i;
    if (!m) return;
    for (i = 0; i < m->function_count; ++i) {
        free(m->functions[i].name);
        free(m->functions[i].code);
    }
    for (i = 0; i < m->import_count; ++i) free(m->imports[i].name);
    free(m->functions);
    free(m->imports);
    free(m);
}
mvm_status mvm_module_add_function(mvm_module *m, const char *name,
    uint32_t arity, uint32_t registers, uint32_t *index, mvm_error *e) {
    char *copy;
    void *p;
    size_t i;
    mvm_status s;
    mvm_function f;
    mvm_error_clear(e);
    if (!m || !mvm_name_valid(name) || !registers || arity > registers)
        return mvm_fail(e, MVM_INVALID, "invalid function declaration");
    for (i = 0; i < m->function_count; ++i)
        if (!strcmp(name, m->functions[i].name)) return mvm_fail(e, MVM_INVALID, "duplicate function: %s", name);
    if (m->function_count >= UINT32_MAX) return mvm_fail(e, MVM_LIMIT, "too many functions");
    copy = mvm_copy_string(name);
    if (!copy) return mvm_fail(e, MVM_OOM, "function name allocation failed");
    p = m->functions;
    s = mvm_grow(&p, &m->function_capacity, m->function_count + 1, sizeof(f));
    if (s != MVM_OK) { free(copy); return mvm_fail(e, s, "function allocation failed"); }
    m->functions = p;
    memset(&f, 0, sizeof(f));
    f.name = copy; f.arity = arity; f.registers = registers;
    m->functions[m->function_count] = f;
    if (index) *index = (uint32_t)m->function_count;
    ++m->function_count;
    return MVM_OK;
}
mvm_status mvm_module_add_import(mvm_module *m, const char *name,
    uint32_t arity, uint32_t *index, mvm_error *e) {
    char *copy;
    void *p;
    size_t i;
    mvm_status s;
    mvm_error_clear(e);
    if (!m || !mvm_name_valid(name)) return mvm_fail(e, MVM_INVALID, "invalid import declaration");
    for (i = 0; i < m->import_count; ++i)
        if (!strcmp(name, m->imports[i].name)) return mvm_fail(e, MVM_INVALID, "duplicate import: %s", name);
    if (m->import_count >= UINT32_MAX) return mvm_fail(e, MVM_LIMIT, "too many imports");
    copy = mvm_copy_string(name);
    if (!copy) return mvm_fail(e, MVM_OOM, "import name allocation failed");
    p = m->imports;
    s = mvm_grow(&p, &m->import_capacity, m->import_count + 1, sizeof(mvm_import));
    if (s != MVM_OK) { free(copy); return mvm_fail(e, s, "import allocation failed"); }
    m->imports = p;
    m->imports[m->import_count].name = copy;
    m->imports[m->import_count].arity = arity;
    if (index) *index = (uint32_t)m->import_count;
    ++m->import_count;
    return MVM_OK;
}
mvm_status mvm_module_emit(mvm_module *m, uint32_t fn, mvm_instruction in, mvm_error *e) {
    mvm_function *f;
    mvm_status s;
    void *p;
    mvm_error_clear(e);
    if (!m || fn >= m->function_count) return mvm_fail(e, MVM_INVALID, "invalid function index");
    f = &m->functions[fn];
    if (f->count >= UINT32_MAX) return mvm_fail(e, MVM_LIMIT, "too many instructions");
    p = f->code;
    s = mvm_grow(&p, &f->capacity, f->count + 1, sizeof(in));
    if (s != MVM_OK) return mvm_fail(e, s, "instruction allocation failed");
    f->code = p;
    f->code[f->count++] = in;
    return MVM_OK;
}
size_t mvm_module_function_count(const mvm_module *m) { return m ? m->function_count : 0; }
size_t mvm_module_import_count(const mvm_module *m) { return m ? m->import_count : 0; }
mvm_status mvm_module_function(const mvm_module *m, uint32_t i, mvm_function_info *out) {
    const mvm_function *f;
    if (!m || !out || i >= m->function_count) return MVM_INVALID;
    f = &m->functions[i];
    out->name = f->name; out->arity = f->arity; out->registers = f->registers;
    out->code = f->code; out->code_count = f->count;
    return MVM_OK;
}
mvm_status mvm_module_import(const mvm_module *m, uint32_t i, const char **name, uint32_t *arity) {
    if (!m || i >= m->import_count || !name || !arity) return MVM_INVALID;
    *name = m->imports[i].name; *arity = m->imports[i].arity;
    return MVM_OK;
}
mvm_status mvm_module_find(const mvm_module *m, const char *name, uint32_t *out) {
    size_t i;
    if (!m || !name || !out) return MVM_INVALID;
    for (i = 0; i < m->function_count; ++i)
        if (!strcmp(name, m->functions[i].name)) { *out = (uint32_t)i; return MVM_OK; }
    return MVM_NOT_FOUND;
}
static const char *verify_instruction(const mvm_module *m, const mvm_function *f, size_t pc) {
    const mvm_instruction *in = &f->code[pc];
    uint32_t n = f->registers, arity;
    if ((unsigned)in->op >= MVM_OP_COUNT) return "unknown opcode";
    if (in->op != MVM_OP_JMP && in->op != MVM_OP_RET && pc + 1 == f->count)
        return "instruction falls off the end of the function";
    if (in->op == MVM_OP_JMP || in->op == MVM_OP_JZ) {
        if (in->immediate < 0 || (uint64_t)in->immediate >= f->count) return "invalid branch target";
    }
    if (in->op == MVM_OP_NOP || in->op == MVM_OP_JMP) return NULL;
    if (in->a >= n) return "register a out of range";
    switch (in->op) {
    case MVM_OP_BOOL: return (in->immediate == 0 || in->immediate == 1) ? NULL : "bool requires 0 or 1";
    case MVM_OP_CONST: case MVM_OP_NIL: case MVM_OP_RET: case MVM_OP_JZ: return NULL;
    case MVM_OP_CALL: case MVM_OP_HOST:
        if (in->op == MVM_OP_CALL) {
            if (in->b >= m->function_count) return "invalid callee index";
            arity = m->functions[in->b].arity;
        } else {
            if (in->b >= m->import_count) return "invalid import index";
            arity = m->imports[in->b].arity;
        }
        if (in->c > n || arity > n - in->c) return "argument register window out of range";
        return NULL;
    default: break;
    }
    if (in->b >= n) return "register b out of range";
    if (in->op == MVM_OP_MOV || in->op == MVM_OP_NOT || in->op == MVM_OP_ARRAY || in->op == MVM_OP_LEN) return NULL;
    return in->c >= n ? "register c out of range" : NULL;
}
mvm_status mvm_module_verify(const mvm_module *m, mvm_error *e) {
    size_t i, j;
    mvm_error_clear(e);
    if (!m) return mvm_fail(e, MVM_INVALID, "NULL module");
    if (!m->function_count) return mvm_fail(e, MVM_VERIFY, "module has no functions");
    for (i = 0; i < m->function_count; ++i) {
        const mvm_function *f = &m->functions[i];
        if (e) e->function = i;
        if (!f->count) return mvm_fail(e, MVM_VERIFY, "function %s is empty", f->name);
        for (j = 0; j < f->count; ++j) {
            const char *message = verify_instruction(m, f, j);
            if (message) {
                if (e) e->instruction = j;
                return mvm_fail(e, MVM_VERIFY, "%s[%zu]: %s", f->name, j, message);
            }
        }
    }
    mvm_error_clear(e);
    return MVM_OK;
}
mvm_status mvm_module_clone(const mvm_module *m, mvm_module **out, mvm_error *e) {
    mvm_module *copy;
    mvm_status s;
    size_t i, j;
    if (!out) return mvm_fail(e, MVM_INVALID, "NULL output");
    *out = NULL;
    s = mvm_module_verify(m, e);
    if (s != MVM_OK) return s;
    copy = mvm_module_create();
    if (!copy) return mvm_fail(e, MVM_OOM, "module allocation failed");
    for (i = 0; i < m->import_count; ++i) {
        s = mvm_module_add_import(copy, m->imports[i].name, m->imports[i].arity, NULL, e);
        if (s != MVM_OK) goto fail;
    }
    for (i = 0; i < m->function_count; ++i) {
        const mvm_function *f = &m->functions[i];
        s = mvm_module_add_function(copy, f->name, f->arity, f->registers, NULL, e);
        if (s != MVM_OK) goto fail;
        for (j = 0; j < f->count; ++j) {
            s = mvm_module_emit(copy, (uint32_t)i, f->code[j], e);
            if (s != MVM_OK) goto fail;
        }
    }
    *out = copy;
    return MVM_OK;
fail:
    mvm_module_destroy(copy);
    return s;
}
mvm_status mvm_module_replace(mvm_module *m, uint32_t fn, size_t pc,
    mvm_instruction in, mvm_error *e) {
    mvm_instruction old;
    mvm_status s;
    mvm_error_clear(e);
    if (!m || fn >= m->function_count || pc >= m->functions[fn].count)
        return mvm_fail(e, MVM_INVALID, "invalid instruction index");
    old = m->functions[fn].code[pc];
    m->functions[fn].code[pc] = in;
    s = mvm_module_verify(m, e);
    if (s != MVM_OK) m->functions[fn].code[pc] = old;
    return s;
}
mvm_status mvm_optimize(mvm_module *m, size_t *changes, mvm_error *e) {
    size_t i, j, n = 0, maximum = 0;
    unsigned char *targets;
    mvm_status s = mvm_module_verify(m, e);
    if (changes) *changes = 0;
    if (s != MVM_OK) return s;
    for (i = 0; i < m->function_count; ++i)
        if (m->functions[i].count > maximum) maximum = m->functions[i].count;
    targets = malloc(maximum);
    if (!targets) return mvm_fail(e, MVM_OOM, "optimizer allocation failed");
    for (i = 0; i < m->function_count; ++i) {
        mvm_function *f = &m->functions[i];
        memset(targets, 0, f->count);
        for (j = 0; j < f->count; ++j) {
            const mvm_instruction *in = &f->code[j];
            if (in->op == MVM_OP_JMP || in->op == MVM_OP_JZ) targets[(size_t)in->immediate] = 1;
        }
        for (j = 0; j < f->count; ++j) {
            mvm_instruction *in = &f->code[j];
            if ((in->op == MVM_OP_MOV && in->a == in->b) ||
                (in->op == MVM_OP_JMP && (uint64_t)in->immediate == j + 1)) {
                mvm_instruction nop = {MVM_OP_NOP, 0, 0, 0, 0};
                *in = nop; ++n;
            } else if (j >= 2 && !targets[j - 1] && !targets[j] &&
                       in->op >= MVM_OP_ADD && in->op <= MVM_OP_LE) {
                const mvm_instruction *a = &f->code[j - 2], *b = &f->code[j - 1];
                int64_t left, right, value;
                if (a->op != MVM_OP_CONST || b->op != MVM_OP_CONST || a->a == b->a) continue;
                if ((in->b != a->a && in->b != b->a) || (in->c != a->a && in->c != b->a)) continue;
                left = in->b == a->a ? a->immediate : b->immediate;
                right = in->c == a->a ? a->immediate : b->immediate;
                if (mvm_integer_eval(in->op, left, right, &value) == MVM_OK) {
                    mvm_instruction folded = {MVM_OP_CONST, 0, 0, 0, 0};
                    folded.op = in->op >= MVM_OP_EQ ? MVM_OP_BOOL : MVM_OP_CONST;
                    folded.a = in->a; folded.immediate = value;
                    *in = folded; ++n;
                }
            }
        }
    }
    free(targets);
    if (changes) *changes = n;
    return MVM_OK;
}
