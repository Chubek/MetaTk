/* Internal full-ISA block compiler. Included by MetaVM.c after semantic handlers.
 * Native code is a sequence of specialized runtime calls, not an interpreter
 * trampoline. Calls/branches/returns delimit blocks; C owns the guest frames. */
typedef struct xvm_item xvm_item;
typedef int (*xvm_stepper)(void *, const void *);
struct xvm_item {
    const mvm_instruction *instruction;
    xvm_stepper step;
    uint32_t function;
    size_t pc;
};
typedef struct xvm_block { size_t begin, end, native_offset; } xvm_block;
typedef struct xvm_function {
    xvm_item *items;
    size_t *entries;
    unsigned char *leaders;
    xvm_block *blocks;
    size_t block_count;
} xvm_function;
typedef struct xvm_plan {
    xvm_function *functions;
    void *native;
    size_t native_size;
} xvm_plan;

mvm_xvm_options mvm_xvm_default_options(void) {
    mvm_xvm_options options;
    options.max_plan_bytes = 64u * 1024u * 1024u;
    options.max_native_bytes = 64u * 1024u * 1024u;
    options.native_mode = MVM_XVM_NATIVE_AUTO;
    return options;
}
mvm_status mvm_xvm_configure(mvm_vm *vm, const mvm_xvm_options *options, mvm_error *e) {
    mvm_error_clear(e);
    if (!vm || !options || !options->max_plan_bytes || !options->max_native_bytes ||
        (options->native_mode != MVM_XVM_NATIVE_AUTO && options->native_mode != MVM_XVM_NATIVE_OFF &&
         options->native_mode != MVM_XVM_NATIVE_REQUIRED))
        return mvm_fail(e, MVM_INVALID, "invalid XVM options");
    if (vm->busy) return mvm_fail(e, MVM_BUSY, "cannot configure XVM during execution");
    vm->xvm_options = *options;
    return MVM_OK;
}
mvm_xvm_statistics mvm_xvm_stats(const mvm_vm *vm) {
    mvm_xvm_statistics empty;
    memset(&empty, 0, sizeof(empty));
    return vm ? vm->xvm_stats : empty;
}
static int xvm_step(execution *x, const xvm_item *item, handler operation) {
    mvm_vm *vm = x->vm;
    mvm_frame *frame = &vm->frames[vm->frame_count - 1];
    frame->pc = item->pc + 1;
    if (x->error) { x->error->function = item->function; x->error->instruction = item->pc; }
    if (vm->steps == vm->limits.max_steps) { x->xvm_status = MVM_LIMIT; return 1; }
    ++vm->steps;
    x->xvm_status = operation(x, item->instruction, frame->registers);
    if (x->xvm_status != MVM_OK && x->error) {
        x->error->function = item->function; x->error->instruction = item->pc;
    }
    return x->xvm_status != MVM_OK;
}
#define XVM_STEP(name, operation) \
    static int xvm_##name(void *context, const void *instruction) { \
        return xvm_step(context, instruction, operation); \
    }
XVM_STEP(nop, op_nop) XVM_STEP(const, op_const) XVM_STEP(nil, op_nil)
XVM_STEP(bool, op_bool) XVM_STEP(mov, op_mov) XVM_STEP(integer, op_integer)
XVM_STEP(eq, op_eq) XVM_STEP(not, op_not) XVM_STEP(jump, op_jump)
XVM_STEP(call, op_call) XVM_STEP(host, op_host) XVM_STEP(return, op_return)
XVM_STEP(array, op_array) XVM_STEP(get, op_get) XVM_STEP(set, op_set) XVM_STEP(len, op_len)
#undef XVM_STEP
static xvm_stepper xvm_select(mvm_opcode op) {
    switch (op) {
    case MVM_OP_NOP: return xvm_nop;
    case MVM_OP_CONST: return xvm_const;
    case MVM_OP_NIL: return xvm_nil;
    case MVM_OP_BOOL: return xvm_bool;
    case MVM_OP_MOV: return xvm_mov;
    case MVM_OP_ADD: case MVM_OP_SUB: case MVM_OP_MUL: case MVM_OP_DIV: case MVM_OP_MOD:
    case MVM_OP_AND: case MVM_OP_OR: case MVM_OP_XOR: case MVM_OP_SHL: case MVM_OP_SHR:
    case MVM_OP_LT: case MVM_OP_LE: return xvm_integer;
    case MVM_OP_EQ: return xvm_eq;
    case MVM_OP_NOT: return xvm_not;
    case MVM_OP_JMP: case MVM_OP_JZ: return xvm_jump;
    case MVM_OP_CALL: return xvm_call;
    case MVM_OP_HOST: return xvm_host;
    case MVM_OP_RET: return xvm_return;
    case MVM_OP_ARRAY: return xvm_array;
    case MVM_OP_GET: return xvm_get;
    case MVM_OP_SET: return xvm_set;
    case MVM_OP_LEN: return xvm_len;
    default: return NULL;
    }
}
static int xvm_terminates(mvm_opcode op) {
    return op == MVM_OP_JMP || op == MVM_OP_JZ || op == MVM_OP_CALL || op == MVM_OP_RET;
}
static void *xvm_allocate(execution *x, size_t count, size_t size, mvm_status *status) {
    size_t *used = &x->vm->xvm_stats.plan_bytes;
    if (count > (x->vm->xvm_options.max_plan_bytes - *used) / size) { *status = MVM_LIMIT; return NULL; }
    *used += count * size;
    {
        void *p = calloc(count, size);
        if (!p) *status = MVM_OOM;
        return p;
    }
}
static void xvm_free_native(void *memory, size_t size) {
    if (!memory) return;
#if !defined(METAVM_NO_JIT) && defined(_WIN32)
    (void)size; (void)VirtualFree(memory, 0, MEM_RELEASE);
#elif !defined(METAVM_NO_JIT) && (defined(__unix__) || defined(__APPLE__))
    (void)munmap(memory, size);
#else
    (void)size;
#endif
}
static void xvm_release(xvm_plan *plan, size_t functions) {
    size_t i;
    xvm_free_native(plan->native, plan->native_size);
    if (plan->functions) {
        for (i = 0; i < functions; ++i) {
            free(plan->functions[i].items); free(plan->functions[i].entries);
            free(plan->functions[i].leaders); free(plan->functions[i].blocks);
        }
        free(plan->functions);
    }
}
typedef struct xvm_bytes { unsigned char data[4096]; size_t size; } xvm_bytes;
static void xvm_emit(xvm_bytes *b, uint64_t value, unsigned count) {
    unsigned i;
    for (i = 0; i < count; ++i) b->data[b->size++] = (unsigned char)(value >> (i * 8));
}
static void xvm_patch(xvm_bytes *b, size_t at, uint64_t value, unsigned count) {
    unsigned i;
    for (i = 0; i < count; ++i) b->data[at + i] = (unsigned char)(value >> (i * 8));
}
static void xvm_arm_address(xvm_bytes *b, unsigned reg, uint64_t address) {
    unsigned i;
    for (i = 0; i < 4; ++i)
        xvm_emit(b, (i ? UINT32_C(0xf2800000) : UINT32_C(0xd2800000)) | (i << 21) |
            (((address >> (16 * i)) & 65535) << 5) | reg, 4);
}
static void xvm_rv_address(xvm_bytes *b, unsigned reg, uint64_t address) {
    if (b->size % 8) xvm_emit(b, 0x00000013, 4);
    xvm_emit(b, (reg << 7) | 0x17, 4);
    xvm_emit(b, 0x01003003 | (reg << 15) | (reg << 7), 4);
    xvm_emit(b, 0x0100006f, 4); xvm_emit(b, 0x00000013, 4); xvm_emit(b, address, 8);
}
static unsigned xvm_arch(void) {
#if defined(METAVM_NO_JIT) || defined(__arm64e__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return 0;
#elif defined(__x86_64__) || defined(_M_X64)
    return 1;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return 2;
#elif defined(__riscv) && __riscv_xlen == 64
    return 3;
#else
    return 0;
#endif
}
static void xvm_emit_block(xvm_bytes *b, const xvm_item *items, size_t count) {
    size_t k, exits[32];
    unsigned arch = xvm_arch();
    b->size = 0;
    if (arch == 1) {
        xvm_emit(b, UINT64_C(0x53fa1e0ff3), 5);
#if defined(_WIN32)
        xvm_emit(b, UINT64_C(0xcb894820ec8348), 7);
#else
        xvm_emit(b, 0xfb8948, 3);
#endif
    } else if (arch == 2) {
        xvm_emit(b, 0xd503245f, 4); xvm_emit(b, 0xa9bf7bf3, 4); xvm_emit(b, 0xaa0003f3, 4);
    } else {
        xvm_emit(b, 0xff010113, 4); xvm_emit(b, 0x00113423, 4);
        xvm_emit(b, 0x00813023, 4); xvm_emit(b, 0x00050413, 4);
    }
    for (k = 0; k < count; ++k) {
        uintptr_t fn = 0;
        memcpy(&fn, &items[k].step, sizeof(fn) < sizeof(items[k].step) ? sizeof(fn) : sizeof(items[k].step));
        if (arch == 1) {
#if defined(_WIN32)
            xvm_emit(b, UINT64_C(0xba48d98948), 5);
#else
            xvm_emit(b, UINT64_C(0xbe48df8948), 5);
#endif
            xvm_emit(b, (uintptr_t)&items[k], 8); xvm_emit(b, 0xb848, 2); xvm_emit(b, fn, 8);
            xvm_emit(b, UINT64_C(0x850fc085d0ff), 6); exits[k] = b->size; xvm_emit(b, 0, 4);
        } else if (arch == 2) {
            xvm_emit(b, 0xaa1303e0, 4); xvm_arm_address(b, 1, (uintptr_t)&items[k]);
            xvm_arm_address(b, 16, fn); xvm_emit(b, 0xd63f0200, 4); exits[k] = b->size; xvm_emit(b, 0, 4);
        } else {
            xvm_emit(b, 0x00040513, 4); xvm_rv_address(b, 11, (uintptr_t)&items[k]);
            xvm_rv_address(b, 5, fn); xvm_emit(b, 0x000280e7, 4); exits[k] = b->size; xvm_emit(b, 0, 4);
        }
    }
    for (k = 0; k < count; ++k) {
        size_t distance = b->size - exits[k];
        if (arch == 1) xvm_patch(b, exits[k], distance - 4, 4);
        else if (arch == 2) xvm_patch(b, exits[k], UINT32_C(0x35000000) | ((distance / 4) << 5), 4);
        else xvm_patch(b, exits[k], 0x00051063u | ((distance >> 12) << 31) | (((distance >> 5) & 63) << 25) |
            (((distance >> 1) & 15) << 8) | (((distance >> 11) & 1) << 7), 4);
    }
    if (arch == 1) {
#if defined(_WIN32)
        xvm_emit(b, 0x20c48348, 4);
#endif
        xvm_emit(b, 0xc35b, 2);
    } else if (arch == 2) {
        xvm_emit(b, 0xa8c17bf3, 4); xvm_emit(b, 0xd65f03c0, 4);
    } else {
        xvm_emit(b, 0x00013403, 4); xvm_emit(b, 0x00813083, 4);
        xvm_emit(b, 0x01010113, 4); xvm_emit(b, 0x00008067, 4);
    }
}
static void *xvm_executable(const unsigned char *bytes, size_t size) {
    void *memory = NULL;
#if !defined(METAVM_NO_JIT) && defined(_WIN32)
    DWORD old;
    memory = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!memory) return NULL;
    memcpy(memory, bytes, size);
    if (!VirtualProtect(memory, size, PAGE_EXECUTE_READ, &old) ||
        !FlushInstructionCache(GetCurrentProcess(), memory, size)) {
        xvm_free_native(memory, size); return NULL;
    }
#elif !defined(METAVM_NO_JIT) && (defined(__unix__) || defined(__APPLE__))
    memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) return NULL;
    memcpy(memory, bytes, size);
    if (mprotect(memory, size, PROT_READ | PROT_EXEC)) { xvm_free_native(memory, size); return NULL; }
    __builtin___clear_cache(memory, (char *)memory + size);
#else
    (void)bytes; (void)size;
#endif
    return memory;
}
static mvm_status xvm_native_plan(execution *x, xvm_plan *plan) {
    unsigned char *bytes = NULL;
    size_t used = 0, capacity = 0, f, b;
    mvm_status status = MVM_OK;
    mvm_xvm_options options = x->vm->xvm_options;
    if (options.native_mode == MVM_XVM_NATIVE_OFF) return MVM_OK;
    if (!xvm_arch() || sizeof(uintptr_t) != sizeof(xvm_stepper) || sizeof(void *) != sizeof(xvm_stepper)) goto unavailable;
    /* Keep sanitizer's pre-entry function metadata probe inside the mapping. */
    if (options.max_native_bytes < 16) return mvm_fail(x->error, MVM_LIMIT, "XVM native code byte limit");
    bytes = calloc(16, 1);
    if (!bytes) return mvm_fail(x->error, MVM_OOM, "XVM native code allocation");
    used = capacity = 16;
    for (f = 0; f < x->vm->module->function_count; ++f) {
        xvm_function *function = &plan->functions[f];
        for (b = 0; b < function->block_count; ++b) {
            xvm_bytes block;
            xvm_block *entry = &function->blocks[b];
            size_t aligned, need;
            xvm_emit_block(&block, function->items + entry->begin, entry->end - entry->begin);
            if (used > SIZE_MAX - 15) { status = MVM_LIMIT; goto done; }
            aligned = (used + 15) & ~(size_t)15;
            if (aligned > options.max_native_bytes || block.size > options.max_native_bytes - aligned) { status = MVM_LIMIT; goto done; }
            need = aligned + block.size;
            if (need > capacity) {
                size_t next = capacity > options.max_native_bytes / 2 ? options.max_native_bytes : capacity * 2;
                unsigned char *grown;
                if (next < need) next = need;
                grown = realloc(bytes, next);
                if (!grown) { status = MVM_OOM; goto done; }
                bytes = grown; capacity = next;
            }
            memset(bytes + used, 0, aligned - used);
            memcpy(bytes + aligned, block.data, block.size);
            entry->native_offset = aligned; used = need;
        }
    }
    plan->native = xvm_executable(bytes, used);
    if (!plan->native) { free(bytes); bytes = NULL; goto unavailable; }
    plan->native_size = used;
    x->vm->xvm_stats.native_bytes = used; x->vm->xvm_stats.native = 1;
done:
    free(bytes);
    if (status != MVM_OK) return mvm_fail(x->error, status, "XVM native code allocation/byte limit");
    return status;
unavailable:
    if (options.native_mode == MVM_XVM_NATIVE_REQUIRED)
        return mvm_fail(x->error, MVM_NOT_FOUND, "native XVM executable memory is unavailable");
    return MVM_OK;
}
static mvm_status xvm_compile(execution *x, xvm_plan *plan) {
    size_t f, i;
    mvm_status status = MVM_OK;
    const mvm_module *module = x->vm->module;
    plan->functions = xvm_allocate(x, module->function_count, sizeof(*plan->functions), &status);
    if (!plan->functions) goto failure;
    for (f = 0; f < module->function_count; ++f) {
        const mvm_function *source = &module->functions[f];
        xvm_function *function = &plan->functions[f];
        function->items = xvm_allocate(x, source->count, sizeof(*function->items), &status);
        function->entries = xvm_allocate(x, source->count, sizeof(*function->entries), &status);
        function->leaders = xvm_allocate(x, source->count, sizeof(*function->leaders), &status);
        function->blocks = xvm_allocate(x, source->count, sizeof(*function->blocks), &status);
        if (!function->items || !function->entries || !function->leaders || !function->blocks) goto failure;
        function->leaders[0] = 1;
        for (i = 0; i < source->count; ++i) {
            const mvm_instruction *instruction = &source->code[i];
            function->items[i].instruction = instruction; function->items[i].step = xvm_select(instruction->op);
            function->items[i].function = (uint32_t)f; function->items[i].pc = i;
            if (!function->items[i].step) { status = MVM_VERIFY; goto failure; }
            if (instruction->op == MVM_OP_JMP || instruction->op == MVM_OP_JZ) function->leaders[(size_t)instruction->immediate] = 1;
            if (xvm_terminates(instruction->op) && i + 1 < source->count) function->leaders[i + 1] = 1;
        }
        for (i = 0; i < source->count;) {
            xvm_block *block = &function->blocks[function->block_count];
            block->begin = i;
            do {
                function->entries[i] = function->block_count;
                ++i;
                if (xvm_terminates(source->code[i - 1].op)) break;
            } while (i < source->count && !function->leaders[i] && i - block->begin < 32);
            block->end = i; ++function->block_count;
        }
        x->vm->xvm_stats.instructions += source->count; x->vm->xvm_stats.blocks += function->block_count;
    }
    return xvm_native_plan(x, plan);
failure:
    return mvm_fail(x->error, status, "XVM plan allocation/byte limit");
}
static mvm_status xvm_execute(execution *x, const xvm_plan *plan) {
    while (x->vm->frame_count) {
        const mvm_frame *frame = &x->vm->frames[x->vm->frame_count - 1];
        const xvm_function *function = &plan->functions[frame->function];
        const xvm_block *block = &function->blocks[function->entries[frame->pc]];
        ++x->vm->xvm_stats.executed_blocks;
        if (plan->native) {
            typedef int (*entry_fn)(void *);
            void *address = (unsigned char *)plan->native + block->native_offset;
            entry_fn entry = NULL;
            memcpy(&entry, &address, sizeof(entry) < sizeof(address) ? sizeof(entry) : sizeof(address));
            (void)entry(x);
        } else {
            size_t pc;
            for (pc = block->begin; pc < block->end; ++pc)
                if (function->items[pc].step(x, &function->items[pc])) break;
        }
        if (x->xvm_status != MVM_OK) return x->xvm_status;
    }
    return MVM_OK;
}
