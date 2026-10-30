#include "Internal.h"

mvm_vm *mvm_create(const mvm_limits *limits, mvm_error *e) {
    mvm_limits l = limits ? *limits : mvm_default_limits();
    mvm_vm *vm;
    mvm_error_clear(e);
    if (!l.max_frames || !l.max_registers || !l.max_heap_bytes || !l.max_module_bytes || !l.max_steps) {
        (void)mvm_fail(e, MVM_INVALID, "all resource limits must be nonzero");
        return NULL;
    }
    vm = calloc(1, sizeof(*vm));
    if (!vm) { (void)mvm_fail(e, MVM_OOM, "VM allocation failed"); return NULL; }
    vm->limits = l; vm->result = mvm_nil();
    return vm;
}
static void unwind(mvm_vm *vm) {
    size_t i;
    for (i = 0; i < vm->frame_count; ++i) free(vm->frames[i].registers);
    vm->frame_count = vm->register_count = 0;
}
void mvm_destroy(mvm_vm *vm) {
    size_t i;
    if (!vm) return;
    unwind(vm);
    while (vm->roots) mvm_unpin(vm->roots);
    vm->result = mvm_nil();
    (void)mvm_collect(vm);
    for (i = 0; i < vm->binding_count; ++i) free(vm->bindings[i].name);
    free(vm->bindings); free(vm->frames); free(vm);
}
uint64_t mvm_steps(const mvm_vm *vm) { return vm ? vm->steps : 0; }
mvm_status mvm_register_native(mvm_vm *vm, const char *name, uint32_t arity,
    mvm_native callback, void *user, mvm_error *e) {
    size_t i;
    char *copy;
    void *p;
    mvm_status s;
    mvm_error_clear(e);
    if (!vm || !mvm_name_valid(name) || !callback) return mvm_fail(e, MVM_INVALID, "invalid native binding");
    if (vm->busy) return mvm_fail(e, MVM_BUSY, "cannot register during execution");
    for (i = 0; i < vm->binding_count; ++i)
        if (!strcmp(name, vm->bindings[i].name)) return mvm_fail(e, MVM_INVALID, "duplicate native: %s", name);
    if (vm->binding_count == SIZE_MAX) return mvm_fail(e, MVM_LIMIT, "too many bindings");
    copy = mvm_copy_string(name);
    if (!copy) return mvm_fail(e, MVM_OOM, "binding name allocation failed");
    p = vm->bindings;
    s = mvm_grow(&p, &vm->binding_capacity, vm->binding_count + 1, sizeof(mvm_binding));
    if (s != MVM_OK) { free(copy); return mvm_fail(e, s, "binding allocation failed"); }
    vm->bindings = p;
    vm->bindings[vm->binding_count].name = copy;
    vm->bindings[vm->binding_count].arity = arity;
    vm->bindings[vm->binding_count].callback = callback;
    vm->bindings[vm->binding_count++].user = user;
    return MVM_OK;
}
static mvm_status push_frame(mvm_vm *vm, uint32_t fn, uint32_t dst, const mvm_value *args) {
    const mvm_function *f = &vm->module->functions[fn];
    mvm_frame frame;
    mvm_status s;
    void *p;
    size_t i, bytes;
    if (vm->frame_count >= vm->limits.max_frames ||
        f->registers > vm->limits.max_registers - vm->register_count) return MVM_LIMIT;
    /* Multiplication checked in size_t even on 32-bit systems. */
    bytes = (size_t)f->registers * sizeof(mvm_value);
    if (bytes / sizeof(mvm_value) != f->registers) return MVM_LIMIT;
    frame.registers = malloc(bytes);
    if (!frame.registers) return MVM_OOM;
    for (i = 0; i < f->registers; ++i) frame.registers[i] = mvm_nil();
    if (f->arity) memcpy(frame.registers, args, (size_t)f->arity * sizeof(mvm_value));
    frame.function = fn; frame.destination = dst; frame.pc = 0;
    p = vm->frames;
    s = mvm_grow(&p, &vm->frame_capacity, vm->frame_count + 1, sizeof(frame));
    if (s != MVM_OK) { free(frame.registers); return s; }
    vm->frames = p;
    vm->frames[vm->frame_count++] = frame;
    vm->register_count += f->registers;
    return MVM_OK;
}

typedef struct execution {
    mvm_vm *vm;
    const mvm_binding **imports;
    mvm_error *error;
} execution;
typedef mvm_status (*handler)(execution *, const mvm_instruction *, mvm_value *);

static mvm_status op_nop(execution *x, const mvm_instruction *i, mvm_value *r) {
    (void)x; (void)i; (void)r; return MVM_OK;
}
static mvm_status op_const(execution *x, const mvm_instruction *i, mvm_value *r) {
    (void)x; r[i->a] = mvm_int(i->immediate); return MVM_OK;
}
static mvm_status op_nil(execution *x, const mvm_instruction *i, mvm_value *r) {
    (void)x; r[i->a] = mvm_nil(); return MVM_OK;
}
static mvm_status op_bool(execution *x, const mvm_instruction *i, mvm_value *r) {
    (void)x; r[i->a] = mvm_bool(i->immediate != 0); return MVM_OK;
}
static mvm_status op_mov(execution *x, const mvm_instruction *i, mvm_value *r) {
    (void)x; r[i->a] = r[i->b]; return MVM_OK;
}
static mvm_status op_integer(execution *x, const mvm_instruction *i, mvm_value *r) {
    int64_t result;
    mvm_status s;
    (void)x;
    if (r[i->b].kind != MVM_INT || r[i->c].kind != MVM_INT) return MVM_TYPE;
    s = mvm_integer_eval(i->op, r[i->b].as.integer, r[i->c].as.integer, &result);
    if (s == MVM_OK) r[i->a] = (i->op == MVM_OP_LT || i->op == MVM_OP_LE) ? mvm_bool(result != 0) : mvm_int(result);
    return s;
}
static mvm_status op_eq(execution *x, const mvm_instruction *i, mvm_value *r) {
    (void)x; r[i->a] = mvm_bool(mvm_value_equal(r[i->b], r[i->c])); return MVM_OK;
}
static mvm_status op_not(execution *x, const mvm_instruction *i, mvm_value *r) {
    (void)x; r[i->a] = mvm_bool(!mvm_value_truthy(r[i->b])); return MVM_OK;
}
static mvm_status op_jump(execution *x, const mvm_instruction *i, mvm_value *r) {
    if (i->op == MVM_OP_JMP || !mvm_value_truthy(r[i->a]))
        x->vm->frames[x->vm->frame_count - 1].pc = (size_t)i->immediate;
    return MVM_OK;
}
static mvm_status op_call(execution *x, const mvm_instruction *i, mvm_value *r) {
    return push_frame(x->vm, i->b, i->a, r + i->c);
}
static mvm_status op_host(execution *x, const mvm_instruction *i, mvm_value *r) {
    const mvm_binding *b = x->imports[i->b];
    mvm_value value = mvm_nil();
    mvm_status s = b->callback(x->vm, r + i->c, b->arity, &value, b->user, x->error);
    if (s != MVM_OK) return MVM_HOST_ERROR;
    if (!mvm_value_valid(x->vm, value)) return MVM_TYPE;
    r[i->a] = value;
    return MVM_OK;
}
static mvm_status op_return(execution *x, const mvm_instruction *i, mvm_value *r) {
    mvm_vm *vm = x->vm;
    mvm_frame *f = &vm->frames[vm->frame_count - 1];
    mvm_value value = r[i->a];
    uint32_t dst = f->destination;
    vm->register_count -= vm->module->functions[f->function].registers;
    free(f->registers);
    --vm->frame_count;
    if (vm->frame_count) vm->frames[vm->frame_count - 1].registers[dst] = value;
    else vm->result = value;
    return MVM_OK;
}
static mvm_status value_index(mvm_value v, size_t *index) {
    if (v.kind != MVM_INT) return MVM_TYPE;
    if (v.as.integer < 0 || (uint64_t)v.as.integer > SIZE_MAX) return MVM_BOUNDS;
    *index = (size_t)v.as.integer;
    return MVM_OK;
}
static mvm_status op_array(execution *x, const mvm_instruction *i, mvm_value *r) {
    size_t n;
    mvm_value result;
    mvm_status s = value_index(r[i->b], &n);
    if (s != MVM_OK) return s;
    /* Keep destination rooted until allocation/collection finishes. */
    s = mvm_array_new(x->vm, n, &result);
    if (s == MVM_OK) r[i->a] = result;
    return s;
}
static mvm_status op_get(execution *x, const mvm_instruction *i, mvm_value *r) {
    size_t index;
    mvm_status s = value_index(r[i->c], &index);
    return s == MVM_OK ? mvm_array_get(x->vm, r[i->b], index, &r[i->a]) : s;
}
static mvm_status op_set(execution *x, const mvm_instruction *i, mvm_value *r) {
    size_t index;
    mvm_status s = value_index(r[i->b], &index);
    return s == MVM_OK ? mvm_array_set(x->vm, r[i->a], index, r[i->c]) : s;
}
static mvm_status op_len(execution *x, const mvm_instruction *i, mvm_value *r) {
    size_t n;
    mvm_status s = mvm_object_length(x->vm, r[i->b], &n);
    if (s != MVM_OK) return s;
    if (n > INT64_MAX) return MVM_LIMIT;
    r[i->a] = mvm_int((int64_t)n);
    return MVM_OK;
}
static handler select_handler(mvm_opcode op) {
    switch (op) {
    case MVM_OP_NOP: return op_nop;
    case MVM_OP_CONST: return op_const;
    case MVM_OP_NIL: return op_nil;
    case MVM_OP_BOOL: return op_bool;
    case MVM_OP_MOV: return op_mov;
    case MVM_OP_ADD: case MVM_OP_SUB: case MVM_OP_MUL: case MVM_OP_DIV: case MVM_OP_MOD:
    case MVM_OP_AND: case MVM_OP_OR: case MVM_OP_XOR: case MVM_OP_SHL: case MVM_OP_SHR:
    case MVM_OP_LT: case MVM_OP_LE: return op_integer;
    case MVM_OP_EQ: return op_eq;
    case MVM_OP_NOT: return op_not;
    case MVM_OP_JMP: case MVM_OP_JZ: return op_jump;
    case MVM_OP_CALL: return op_call;
    case MVM_OP_HOST: return op_host;
    case MVM_OP_RET: return op_return;
    case MVM_OP_ARRAY: return op_array;
    case MVM_OP_GET: return op_get;
    case MVM_OP_SET: return op_set;
    case MVM_OP_LEN: return op_len;
    default: return NULL;
    }
}
static mvm_status resolve_imports(execution *x) {
    const mvm_module *m = x->vm->module;
    size_t i, j;
    if (!m->import_count) return MVM_OK;
    if (m->import_count > SIZE_MAX / sizeof(*x->imports)) return MVM_LIMIT;
    x->imports = malloc(m->import_count * sizeof(*x->imports));
    if (!x->imports) return MVM_OOM;
    for (i = 0; i < m->import_count; ++i) {
        for (j = 0; j < x->vm->binding_count; ++j) {
            const mvm_binding *b = &x->vm->bindings[j];
            if (!strcmp(b->name, m->imports[i].name) && b->arity == m->imports[i].arity) break;
        }
        if (j == x->vm->binding_count)
            return mvm_fail(x->error, MVM_NOT_FOUND, "unresolved import %s/%lu", m->imports[i].name,
                (unsigned long)m->imports[i].arity);
        x->imports[i] = &x->vm->bindings[j];
    }
    return MVM_OK;
}
mvm_status mvm_run(mvm_vm *vm, const mvm_module *m, uint32_t fn,
    const mvm_value *args, size_t count, mvm_engine engine, mvm_value *out, mvm_error *e) {
    mvm_status s;
    execution x;
    handler **plan = NULL;
    size_t i, j, total = 0;
    mvm_error_clear(e);
    if (!vm || !m || !out || (count && !args) ||
        (engine != MVM_ENGINE_BYTECODE && engine != MVM_ENGINE_THREADED)) {
        if (out) *out = mvm_nil();
        return mvm_fail(e, MVM_INVALID, "invalid execution arguments");
    }
    if (vm->busy) { *out = mvm_nil(); return mvm_fail(e, MVM_BUSY, "VM already executing"); }
    vm->steps = 0;
    s = mvm_module_verify(m, e);
    if (s != MVM_OK) { *out = mvm_nil(); return s; }
    if (fn >= m->function_count || count != m->functions[fn].arity) {
        *out = mvm_nil(); return mvm_fail(e, MVM_INVALID, "entry function/argument count mismatch");
    }
    for (i = 0; i < count; ++i) {
        if (!mvm_value_valid(vm, args[i])) {
            *out = mvm_nil(); return mvm_fail(e, MVM_INVALID, "invalid or foreign argument %zu", i);
        }
    }
    /* Bound resident instruction and dispatch memory independently of input format. */
    for (i = 0; i < m->function_count; ++i) {
        size_t n = m->functions[i].count;
        if (n > (vm->limits.max_module_bytes - total) / sizeof(mvm_instruction)) {
            *out = mvm_nil(); return mvm_fail(e, MVM_LIMIT, "module exceeds execution byte limit");
        }
        total += n * sizeof(mvm_instruction);
    }
    vm->busy = 1; vm->module = m;
    x.vm = vm; x.imports = NULL; x.error = e;
    s = push_frame(vm, fn, 0, args); /* Copy before clearing possibly aliased out. */
    *out = mvm_nil();
    vm->result = mvm_nil();
    if (s != MVM_OK) goto done;
    s = resolve_imports(&x);
    if (s != MVM_OK) goto done;
    if (engine == MVM_ENGINE_THREADED) {
        if (m->function_count > SIZE_MAX / sizeof(*plan)) { s = MVM_LIMIT; goto done; }
        plan = calloc(m->function_count, sizeof(*plan));
        if (!plan) { s = MVM_OOM; goto done; }
        for (i = 0; i < m->function_count; ++i) {
            if (m->functions[i].count > SIZE_MAX / sizeof(handler)) { s = MVM_LIMIT; goto done; }
            plan[i] = malloc(m->functions[i].count * sizeof(handler));
            if (!plan[i]) { s = MVM_OOM; goto done; }
            for (j = 0; j < m->functions[i].count; ++j) plan[i][j] = select_handler(m->functions[i].code[j].op);
        }
    }
    while (vm->frame_count) {
        mvm_frame *frame = &vm->frames[vm->frame_count - 1];
        uint32_t function = frame->function;
        size_t pc = frame->pc++;
        const mvm_instruction *in = &m->functions[frame->function].code[pc];
        handler h = plan ? plan[frame->function][pc] : select_handler(in->op);
        if (e) { e->function = frame->function; e->instruction = pc; }
        if (vm->steps == vm->limits.max_steps) { s = MVM_LIMIT; break; }
        ++vm->steps;
        s = h(&x, in, frame->registers);
        if (s != MVM_OK) {
            /* Callbacks may use another API with the same error object. Keep
             * their message, but attach the actual failing guest location. */
            if (e) { e->function = function; e->instruction = pc; }
            break;
        }
    }
done:
    if (plan) {
        for (i = 0; i < m->function_count; ++i) free(plan[i]);
        free(plan);
    }
    free(x.imports);
    unwind(vm);
    vm->module = NULL; vm->busy = 0;
    if (s == MVM_OK) { *out = vm->result; mvm_error_clear(e); }
    else {
        vm->result = mvm_nil();
        if (!e || !e->message[0]) (void)mvm_fail(e, s, "%s", mvm_status_string(s));
        else e->status = s;
    }
    return s;
}
mvm_status mvm_run_bytecode(mvm_vm *vm, const mvm_module *m, uint32_t f,
    const mvm_value *a, size_t n, mvm_value *out, mvm_error *e) {
    return mvm_run(vm, m, f, a, n, MVM_ENGINE_BYTECODE, out, e);
}
mvm_status mvm_run_threaded(mvm_vm *vm, const mvm_module *m, uint32_t f,
    const mvm_value *a, size_t n, mvm_value *out, mvm_error *e) {
    return mvm_run(vm, m, f, a, n, MVM_ENGINE_THREADED, out, e);
}
