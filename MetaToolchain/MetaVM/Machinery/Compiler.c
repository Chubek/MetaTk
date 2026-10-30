#include "Internal.h"

typedef struct symbol { char name[256]; uint32_t function; size_t pc; } symbol;
typedef struct fixup { symbol target; size_t line; mvm_opcode op; } fixup;
typedef struct location { uint32_t function; size_t pc, line; } location;
typedef struct parser {
    mvm_module *module;
    symbol *labels;
    fixup *fixups;
    location *locations;
    size_t label_count, label_capacity, fixup_count, fixup_capacity;
    size_t location_count, location_capacity;
    uint32_t function;
    int in_function;
    size_t line;
    mvm_error *error;
} parser;
static int space(unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f' || c == ','; }
static mvm_status unsigned_number(const char *text, uint64_t maximum, uint64_t *out) {
    uint64_t n = 0;
    if (!text || !*text) return MVM_FORMAT;
    while (*text) {
        unsigned c = (unsigned char)*text++;
        if (c < '0' || c > '9') return MVM_FORMAT;
        c -= '0';
        if (n > maximum / 10 || (n == maximum / 10 && c > maximum % 10)) return MVM_FORMAT;
        n = n * 10 + c;
    }
    *out = n;
    return MVM_OK;
}
static mvm_status u32(const char *text, uint32_t *out) {
    uint64_t n;
    mvm_status s = unsigned_number(text, UINT32_MAX, &n);
    if (s == MVM_OK) *out = (uint32_t)n;
    return s;
}
static mvm_status reg(const char *text, uint32_t *out) {
    return text && text[0] == 'r' ? u32(text + 1, out) : MVM_FORMAT;
}
static mvm_status immediate(const char *text, int64_t *out) {
    uint64_t n;
    int negative = text[0] == '-';
    mvm_status s = unsigned_number(text + negative, (uint64_t)INT64_MAX + (unsigned)negative, &n);
    if (s == MVM_OK) *out = negative ? mvm_from_bits(UINT64_C(0) - n) : (int64_t)n;
    return s;
}
static mvm_status label_add(parser *p, const char *name) {
    size_t i;
    void *data;
    mvm_status s;
    symbol *sym;
    if (!p->in_function || !mvm_name_valid(name)) return mvm_fail(p->error, MVM_FORMAT, "invalid label");
    for (i = 0; i < p->label_count; ++i) {
        if (p->labels[i].function == p->function && !strcmp(name, p->labels[i].name))
            return mvm_fail(p->error, MVM_FORMAT, "duplicate label: %s", name);
    }
    data = p->labels;
    s = mvm_grow(&data, &p->label_capacity, p->label_count + 1, sizeof(symbol));
    if (s != MVM_OK) return s;
    p->labels = data;
    sym = &p->labels[p->label_count++];
    memcpy(sym->name, name, strlen(name) + 1);
    sym->function = p->function;
    sym->pc = p->module->functions[p->function].count;
    return MVM_OK;
}
static mvm_status fixup_add(parser *p, const char *name, mvm_opcode op) {
    fixup *fix;
    void *data = p->fixups;
    mvm_status s;
    if (!mvm_name_valid(name)) return mvm_fail(p->error, MVM_FORMAT, "invalid reference: %s", name);
    s = mvm_grow(&data, &p->fixup_capacity, p->fixup_count + 1, sizeof(fixup));
    if (s != MVM_OK) return s;
    p->fixups = data;
    fix = &p->fixups[p->fixup_count++];
    memcpy(fix->target.name, name, strlen(name) + 1);
    fix->target.function = p->function;
    fix->target.pc = p->module->functions[p->function].count;
    fix->op = op; fix->line = p->line;
    return MVM_OK;
}
static mvm_status emit(parser *p, mvm_instruction in) {
    void *data = p->locations;
    location *loc;
    mvm_status s = mvm_grow(&data, &p->location_capacity, p->location_count + 1, sizeof(location));
    if (s != MVM_OK) return s;
    p->locations = data;
    loc = &p->locations[p->location_count++];
    loc->function = p->function; loc->line = p->line;
    loc->pc = p->module->functions[p->function].count;
    return mvm_module_emit(p->module, p->function, in, p->error);
}
static mvm_status parse_line(parser *p, char *text) {
    char *tokens[5];
    size_t count = 0, len;
    unsigned op;
    mvm_instruction in = {MVM_OP_NOP, 0, 0, 0, 0};
    mvm_status s = MVM_OK;
    char *cursor = text;
    while (*cursor) {
        while (space((unsigned char)*cursor)) ++cursor;
        if (!*cursor || *cursor == '#' || *cursor == ';') break;
        if (count == 5) return mvm_fail(p->error, MVM_FORMAT, "too many tokens");
        tokens[count++] = cursor;
        while (*cursor && !space((unsigned char)*cursor) && *cursor != '#' && *cursor != ';') ++cursor;
        if (*cursor == '#' || *cursor == ';') { *cursor = 0; break; }
        if (*cursor) *cursor++ = 0;
    }
    if (!count) return MVM_OK;
    if (!strcmp(tokens[0], ".function")) {
        uint32_t arity, registers;
        if (p->in_function || count != 4 || u32(tokens[2], &arity) != MVM_OK || u32(tokens[3], &registers) != MVM_OK)
            return mvm_fail(p->error, MVM_FORMAT, "expected .function name arity registers outside a function");
        s = mvm_module_add_function(p->module, tokens[1], arity, registers, &p->function, p->error);
        if (s == MVM_OK) p->in_function = 1;
        return s;
    }
    if (!strcmp(tokens[0], ".import")) {
        uint32_t arity;
        if (p->in_function || count != 3 || u32(tokens[2], &arity) != MVM_OK)
            return mvm_fail(p->error, MVM_FORMAT, "expected .import name arity outside a function");
        return mvm_module_add_import(p->module, tokens[1], arity, NULL, p->error);
    }
    if (!strcmp(tokens[0], ".end")) {
        if (!p->in_function || count != 1) return mvm_fail(p->error, MVM_FORMAT, "unexpected .end");
        p->in_function = 0;
        return MVM_OK;
    }
    len = strlen(tokens[0]);
    if (count == 1 && len && tokens[0][len - 1] == ':') {
        tokens[0][len - 1] = 0;
        return label_add(p, tokens[0]);
    }
    if (!p->in_function) return mvm_fail(p->error, MVM_FORMAT, "instruction outside a function");
    for (op = 0; op < MVM_OP_COUNT; ++op) if (!strcmp(tokens[0], mvm_opcode_name((mvm_opcode)op))) break;
    if (op == MVM_OP_COUNT) return mvm_fail(p->error, MVM_FORMAT, "unknown opcode: %s", tokens[0]);
    in.op = (mvm_opcode)op;
    switch (in.op) {
    case MVM_OP_NOP:
        if (count != 1) s = MVM_FORMAT;
        break;
    case MVM_OP_CONST: case MVM_OP_BOOL:
        if (count != 3 || reg(tokens[1], &in.a) != MVM_OK || immediate(tokens[2], &in.immediate) != MVM_OK) s = MVM_FORMAT;
        break;
    case MVM_OP_NIL: case MVM_OP_RET:
        if (count != 2 || reg(tokens[1], &in.a) != MVM_OK) s = MVM_FORMAT;
        break;
    case MVM_OP_MOV: case MVM_OP_NOT: case MVM_OP_ARRAY: case MVM_OP_LEN:
        if (count != 3 || reg(tokens[1], &in.a) != MVM_OK || reg(tokens[2], &in.b) != MVM_OK) s = MVM_FORMAT;
        break;
    case MVM_OP_JMP:
        s = count == 2 ? fixup_add(p, tokens[1], in.op) : MVM_FORMAT;
        break;
    case MVM_OP_JZ:
        if (count != 3 || reg(tokens[1], &in.a) != MVM_OK) s = MVM_FORMAT;
        else s = fixup_add(p, tokens[2], in.op);
        break;
    case MVM_OP_CALL: case MVM_OP_HOST:
        if (count != 4 || reg(tokens[1], &in.a) != MVM_OK || reg(tokens[3], &in.c) != MVM_OK) s = MVM_FORMAT;
        else s = fixup_add(p, tokens[2], in.op);
        break;
    default:
        if (count != 4 || reg(tokens[1], &in.a) != MVM_OK || reg(tokens[2], &in.b) != MVM_OK || reg(tokens[3], &in.c) != MVM_OK) s = MVM_FORMAT;
        break;
    }
    if (s != MVM_OK) {
        if (p->error && p->error->message[0]) return s;
        return mvm_fail(p->error, s, "invalid operands for %s", tokens[0]);
    }
    return emit(p, in);
}
static mvm_status resolve(parser *p) {
    size_t i, j;
    for (i = 0; i < p->fixup_count; ++i) {
        const fixup *f = &p->fixups[i];
        const char *name = f->target.name;
        mvm_instruction *in = &p->module->functions[f->target.function].code[f->target.pc];
        p->line = f->line;
        if (f->op == MVM_OP_CALL) {
            if (mvm_module_find(p->module, name, &in->b) == MVM_OK) continue;
        } else if (f->op == MVM_OP_HOST) {
            for (j = 0; j < p->module->import_count; ++j) if (!strcmp(name, p->module->imports[j].name)) break;
            if (j < p->module->import_count) { in->b = (uint32_t)j; continue; }
        } else {
            for (j = 0; j < p->label_count; ++j) {
                if (p->labels[j].function == f->target.function && !strcmp(name, p->labels[j].name)) break;
            }
            if (j < p->label_count) { in->immediate = (int64_t)p->labels[j].pc; continue; }
        }
        return mvm_fail(p->error, MVM_FORMAT, "undefined reference: %s", name);
    }
    return MVM_OK;
}
mvm_status mvm_compile(const char *source, size_t n, size_t limit, mvm_module **out, mvm_error *e) {
    parser p;
    char *text = NULL, *line;
    size_t i;
    mvm_status s = MVM_OK;
    mvm_error_clear(e);
    if (!out) return mvm_fail(e, MVM_INVALID, "NULL module output");
    *out = NULL;
    if (!source && n) return mvm_fail(e, MVM_INVALID, "NULL source");
    if (!limit) limit = mvm_default_limits().max_module_bytes;
    if (n > limit || n == SIZE_MAX) return mvm_fail(e, MVM_LIMIT, "source exceeds byte limit");
    if (n && memchr(source, 0, n)) return mvm_fail(e, MVM_FORMAT, "embedded NUL in source");
    memset(&p, 0, sizeof(p));
    p.error = e; p.line = 1;
    p.module = mvm_module_create();
    text = malloc(n + 1);
    if (!p.module || !text) { s = MVM_OOM; goto done; }
    if (n) memcpy(text, source, n);
    text[n] = 0; line = text;
    for (i = 0; i <= n; ++i) {
        if (text[i] == '\n' || i == n) {
            text[i] = 0;
            s = parse_line(&p, line);
            if (s != MVM_OK) goto done;
            if (i < n) line = text + i + 1;
            ++p.line;
        }
    }
    if (p.in_function) { s = mvm_fail(e, MVM_FORMAT, "missing .end"); goto done; }
    s = resolve(&p);
    if (s != MVM_OK) goto done;
    s = mvm_module_verify(p.module, e);
    if (s != MVM_OK && e) {
        for (i = 0; i < p.location_count; ++i) {
            const location *loc = &p.locations[i];
            if (loc->function == e->function && loc->pc == e->instruction) { p.line = loc->line; break; }
        }
    }
done:
    free(text); free(p.labels); free(p.fixups); free(p.locations);
    if (s == MVM_OK) *out = p.module;
    else {
        mvm_module_destroy(p.module);
        if (!e || !e->message[0]) (void)mvm_fail(e, s, "%s", mvm_status_string(s));
        if (e) { e->line = p.line; e->status = s; }
    }
    return s;
}
