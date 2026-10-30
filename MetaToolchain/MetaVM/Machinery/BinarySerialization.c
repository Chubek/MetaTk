#include "Internal.h"

void mvm_buffer_dispose(mvm_buffer *b) {
    if (!b) return;
    free(b->data); b->data = NULL; b->size = b->capacity = 0;
}
mvm_status mvm_buffer_append(mvm_buffer *b, const void *data, size_t n) {
    mvm_status s;
    void *p;
    if (!b || (!data && n) || b->size > b->capacity || (b->capacity && !b->data)) return MVM_INVALID;
    if (n > SIZE_MAX - b->size) return MVM_LIMIT;
    p = b->data;
    s = mvm_grow(&p, &b->capacity, b->size + n, 1);
    if (s != MVM_OK) return s;
    b->data = p;
    if (n) memcpy(b->data + b->size, data, n);
    b->size += n;
    return MVM_OK;
}
mvm_status mvm_reader_init(mvm_reader *r, const void *data, size_t n) {
    if (!r || (!data && n)) return MVM_INVALID;
    r->data = data; r->size = n; r->offset = 0;
    return MVM_OK;
}
mvm_status mvm_reader_bytes(mvm_reader *r, void *out, size_t n) {
    if (!r || (!out && n) || (!r->data && r->size)) return MVM_INVALID;
    if (r->offset > r->size || n > r->size - r->offset) return MVM_FORMAT;
    if (n) memcpy(out, r->data + r->offset, n);
    r->offset += n;
    return MVM_OK;
}
mvm_status mvm_reader_u32(mvm_reader *r, uint32_t *out) {
    unsigned char b[4];
    mvm_status s;
    if (!out) return MVM_INVALID;
    s = mvm_reader_bytes(r, b, sizeof(b));
    if (s != MVM_OK) return s;
    *out = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return MVM_OK;
}
mvm_status mvm_reader_u64(mvm_reader *r, uint64_t *out) {
    unsigned char b[8];
    unsigned i;
    uint64_t v = 0;
    mvm_status s;
    if (!out) return MVM_INVALID;
    s = mvm_reader_bytes(r, b, sizeof(b));
    if (s != MVM_OK) return s;
    for (i = 0; i < 8; ++i) v |= (uint64_t)b[i] << (8 * i);
    *out = v;
    return MVM_OK;
}
static mvm_status put_u32(mvm_buffer *b, uint32_t v) {
    unsigned char bytes[4];
    unsigned i;
    for (i = 0; i < 4; ++i) bytes[i] = (unsigned char)(v >> (8 * i));
    return mvm_buffer_append(b, bytes, sizeof(bytes));
}
static mvm_status put_u64(mvm_buffer *b, uint64_t v) {
    unsigned char bytes[8];
    unsigned i;
    for (i = 0; i < 8; ++i) bytes[i] = (unsigned char)(v >> (8 * i));
    return mvm_buffer_append(b, bytes, sizeof(bytes));
}
static mvm_status put_name(mvm_buffer *b, const char *name) {
    size_t n = strlen(name);
    mvm_status s = put_u32(b, (uint32_t)n);
    return s == MVM_OK ? mvm_buffer_append(b, name, n) : s;
}
/* Keep all partial writes in a private buffer. */
#define WRITE(expr) do { s = (expr); if (s != MVM_OK) goto done; } while (0)
mvm_status mvm_serialize(const mvm_module *m, mvm_buffer *out, mvm_error *e) {
    mvm_buffer b = {0};
    mvm_status s;
    size_t i, j;
    mvm_error_clear(e);
    if (!out) return mvm_fail(e, MVM_INVALID, "NULL output buffer");
    s = mvm_module_verify(m, e);
    if (s != MVM_OK) return s;
    WRITE(mvm_buffer_append(&b, "MVM\0", 4));
    WRITE(put_u32(&b, 1));
    WRITE(put_u32(&b, (uint32_t)m->import_count));
    WRITE(put_u32(&b, (uint32_t)m->function_count));
    for (i = 0; i < m->import_count; ++i) {
        WRITE(put_name(&b, m->imports[i].name));
        WRITE(put_u32(&b, m->imports[i].arity));
    }
    for (i = 0; i < m->function_count; ++i) {
        const mvm_function *f = &m->functions[i];
        WRITE(put_name(&b, f->name));
        WRITE(put_u32(&b, f->arity));
        WRITE(put_u32(&b, f->registers));
        WRITE(put_u32(&b, (uint32_t)f->count));
        for (j = 0; j < f->count; ++j) {
            const mvm_instruction *in = &f->code[j];
            WRITE(put_u32(&b, (uint32_t)in->op));
            WRITE(put_u32(&b, in->a));
            WRITE(put_u32(&b, in->b));
            WRITE(put_u32(&b, in->c));
            WRITE(put_u64(&b, (uint64_t)in->immediate));
        }
    }
    WRITE(mvm_buffer_append(out, b.data, b.size));
done:
    mvm_buffer_dispose(&b);
    if (s != MVM_OK) return mvm_fail(e, s, "serialization failed: %s", mvm_status_string(s));
    return MVM_OK;
}
#undef WRITE
static mvm_status read_name(mvm_reader *r, char name[256]) {
    uint32_t n;
    mvm_status s = mvm_reader_u32(r, &n);
    if (s != MVM_OK || !n || n > 255) return MVM_FORMAT;
    s = mvm_reader_bytes(r, name, n);
    if (s != MVM_OK) return s;
    if (memchr(name, 0, n)) return MVM_FORMAT;
    name[n] = 0;
    return mvm_name_valid(name) ? MVM_OK : MVM_FORMAT;
}
#define READ(expr) do { s = (expr); if (s != MVM_OK) goto fail; } while (0)
mvm_status mvm_deserialize(const void *data, size_t n, size_t limit, mvm_module **out, mvm_error *e) {
    mvm_reader r;
    mvm_module *m = NULL;
    mvm_status s;
    unsigned char magic[4];
    uint32_t version, imports, functions, i, j;
    mvm_error_clear(e);
    if (!out) return mvm_fail(e, MVM_INVALID, "NULL module output");
    *out = NULL;
    if (!data && n) return mvm_fail(e, MVM_INVALID, "NULL binary input");
    if (!limit) limit = mvm_default_limits().max_module_bytes;
    if (n > limit) return mvm_fail(e, MVM_LIMIT, "binary input exceeds byte limit");
    READ(mvm_reader_init(&r, data, n));
    READ(mvm_reader_bytes(&r, magic, 4));
    if (memcmp(magic, "MVM\0", 4)) { s = MVM_FORMAT; goto fail; }
    READ(mvm_reader_u32(&r, &version));
    if (version != 1) return mvm_fail(e, MVM_FORMAT, "unsupported binary version %lu", (unsigned long)version);
    READ(mvm_reader_u32(&r, &imports));
    READ(mvm_reader_u32(&r, &functions));
    if (!functions || imports > (r.size - r.offset) / 9 || functions > (r.size - r.offset) / 41) {
        s = MVM_FORMAT; goto fail;
    }
    m = mvm_module_create();
    if (!m) { s = MVM_OOM; goto fail; }
    for (i = 0; i < imports; ++i) {
        char name[256];
        uint32_t arity;
        READ(read_name(&r, name));
        READ(mvm_reader_u32(&r, &arity));
        READ(mvm_module_add_import(m, name, arity, NULL, e));
    }
    for (i = 0; i < functions; ++i) {
        char name[256];
        uint32_t arity, registers, count;
        READ(read_name(&r, name));
        READ(mvm_reader_u32(&r, &arity));
        READ(mvm_reader_u32(&r, &registers));
        READ(mvm_reader_u32(&r, &count));
        if (!count || count > (r.size - r.offset) / 24) { s = MVM_FORMAT; goto fail; }
        READ(mvm_module_add_function(m, name, arity, registers, NULL, e));
        for (j = 0; j < count; ++j) {
            mvm_instruction in;
            uint32_t op;
            uint64_t immediate;
            READ(mvm_reader_u32(&r, &op));
            if (op >= MVM_OP_COUNT) { s = MVM_VERIFY; goto fail; }
            in.op = (mvm_opcode)op;
            READ(mvm_reader_u32(&r, &in.a));
            READ(mvm_reader_u32(&r, &in.b));
            READ(mvm_reader_u32(&r, &in.c));
            READ(mvm_reader_u64(&r, &immediate));
            in.immediate = mvm_from_bits(immediate);
            READ(mvm_module_emit(m, i, in, e));
        }
    }
    if (r.offset != r.size) { s = MVM_FORMAT; goto fail; }
    READ(mvm_module_verify(m, e));
    *out = m;
    return MVM_OK;
fail:
    mvm_module_destroy(m);
    if (s == MVM_INVALID) s = MVM_FORMAT;
    return mvm_fail(e, s, "invalid module at byte %zu: %s", r.offset, mvm_status_string(s));
}
#undef READ
