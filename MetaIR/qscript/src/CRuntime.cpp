// The portable C99 run-time for QScript built-ins (not installed API).
//
// The C backend (CEmitter.cpp) places this source in the generated program,
// after tables that describe the unit: qs_strings, qs_types and the QS_B_*
// built-in numbers. It ports the semantics of Runtime.cpp, with the same
// messages and the same heap accounting, so that the generated program and
// the VM agree on output, traps and limits.
//
// Conventions: a handle is a 1-based index into the heap; a failure records a
// status and message (the first one wins) and the built-in returns; the
// dispatcher reports the status as a negative value, which stops the run.
#include "qscript/src/Runtime.h"

namespace qscript::detail {
std::string_view c_runtime_source() {
    return R"QSRT(
/* ---- QScript run-time: strings, lists, integers and printing ---- */
enum { QS_RT_TRAP = 1, QS_RT_LIMIT = 3, QS_RT_OUTPUT = 4 };

typedef struct qs_rt_obj {
    int kind;
    size_t len, cap;
    unsigned char *bytes;   /* strings */
    int64_t *items;         /* lists */
} qs_rt_obj;

typedef struct qs_rt_buf { char *p; size_t n, cap; } qs_rt_buf;

static qs_rt_obj *qs_rt_heap = NULL;
static size_t qs_rt_count = 0, qs_rt_capacity = 0, qs_rt_used = 0;
static int64_t *qs_rt_literals = NULL;
static int qs_rt_status = 0;
static char qs_rt_message[512] = "";
static qs_generated_output_fn qs_rt_output = NULL;
static void *qs_rt_output_user = NULL;
static qs_generated_text_fn qs_rt_text = NULL;
static void *qs_rt_text_user = NULL;
static qs_rt_obj qs_rt_none = { -1, 0, 0, NULL, NULL };

/* Installs the function that receives each printed line of text (one call per
   print of a non-integer, or of every value when the program prints any
   non-integer). The text does not include the newline. */
void qs_generated_set_text(qs_generated_text_fn text, void *user) {
    qs_rt_text = text;
    qs_rt_text_user = user;
}

static void qs_rt_failf(int status, const char *format, ...) {
    va_list args;
    if (qs_rt_status) return;
    qs_rt_status = status;
    va_start(args, format);
    vsnprintf(qs_rt_message, sizeof qs_rt_message, format, args);
    va_end(args);
}

static void qs_rt_end(void) {
    size_t i;
    for (i = 0; i < qs_rt_count; ++i) {
        free(qs_rt_heap[i].bytes);
        free(qs_rt_heap[i].items);
    }
    free(qs_rt_heap);
    qs_rt_heap = NULL;
    qs_rt_count = qs_rt_capacity = qs_rt_used = 0;
    free(qs_rt_literals);
    qs_rt_literals = NULL;
}

static int qs_rt_begin(qs_generated_output_fn output, void *user) {
    qs_rt_end();
    qs_rt_status = 0;
    qs_rt_message[0] = '\0';
    qs_rt_output = output;
    qs_rt_output_user = user;
    qs_rt_literals = calloc(QS_NSTRINGS ? QS_NSTRINGS : 1, sizeof *qs_rt_literals);
    return qs_rt_literals ? 0 : QS_RT_LIMIT;
}

/* Heap accounting matches Session::charge_bytes and Session::allocate. */
static int qs_rt_charge(size_t bytes) {
    if (bytes > QS_MAX_BYTES || qs_rt_used > QS_MAX_BYTES - bytes) {
        qs_rt_failf(QS_RT_LIMIT, "heap size limit exceeded");
        return 1;
    }
    qs_rt_used += bytes;
    return 0;
}

static int64_t qs_rt_alloc(int kind, size_t bytes) {
    qs_rt_obj *slot;
    if (qs_rt_count >= QS_MAX_OBJECTS) {
        qs_rt_failf(QS_RT_LIMIT, "heap object limit exceeded");
        return 0;
    }
    if (qs_rt_charge(bytes + 64)) return 0;
    if (qs_rt_count == qs_rt_capacity) {
        size_t capacity = qs_rt_capacity ? 2 * qs_rt_capacity : 64;
        qs_rt_obj *heap = realloc(qs_rt_heap, capacity * sizeof *heap);
        if (!heap) {
            qs_rt_failf(QS_RT_LIMIT, "out of memory");
            return 0;
        }
        qs_rt_heap = heap;
        qs_rt_capacity = capacity;
    }
    slot = &qs_rt_heap[qs_rt_count++];
    slot->kind = kind;
    slot->len = 0;
    slot->cap = 0;
    slot->bytes = NULL;
    slot->items = NULL;
    return (int64_t)qs_rt_count;
}

static qs_rt_obj *qs_rt_get(int64_t handle, int kind) {
    if (handle <= 0 || (uint64_t)handle > qs_rt_count || qs_rt_heap[handle - 1].kind != kind) {
        qs_rt_failf(QS_RT_TRAP, "internal: invalid QScript value");
        return &qs_rt_none;
    }
    return &qs_rt_heap[handle - 1];
}

/* A string of n bytes; the caller fills *bytes. Returns 0 on failure. */
static int64_t qs_rt_string(size_t n, unsigned char **bytes) {
    int64_t handle = qs_rt_alloc(1, n);
    qs_rt_obj *o;
    if (!handle) return 0;
    o = &qs_rt_heap[handle - 1];
    o->bytes = malloc(n ? n : 1);
    if (!o->bytes) {
        qs_rt_failf(QS_RT_LIMIT, "out of memory");
        return 0;
    }
    o->len = n;
    if (bytes) *bytes = o->bytes;
    return handle;
}

static int64_t qs_rt_string_copy(const unsigned char *p, size_t n) {
    unsigned char *bytes = NULL;
    int64_t handle = qs_rt_string(n, &bytes);
    if (handle && n) memcpy(bytes, p, n);
    return handle;
}

static int qs_rt_reserve(qs_rt_obj *o, size_t n) {
    size_t capacity;
    int64_t *items;
    if (n <= o->cap) return 0;
    capacity = o->cap ? o->cap : 4;
    while (capacity < n) capacity *= 2;
    items = realloc(o->items, capacity * sizeof *items);
    if (!items) {
        qs_rt_failf(QS_RT_LIMIT, "out of memory");
        return 1;
    }
    o->items = items;
    o->cap = capacity;
    return 0;
}

static int qs_rt_index_ok(int64_t index, size_t length) {
    if (index >= 0 && (uint64_t)index < length) return 1;
    qs_rt_failf(QS_RT_TRAP, "index %lld out of range (length %lu)", (long long)index, (unsigned long)length);
    return 0;
}

static int qs_rt_buf_add(qs_rt_buf *b, const void *data, size_t n) {
    if (b->cap - b->n < n) {
        size_t capacity = b->cap ? b->cap : 64;
        char *p;
        while (capacity - b->n < n) capacity *= 2;
        p = realloc(b->p, capacity);
        if (!p) {
            qs_rt_failf(QS_RT_LIMIT, "out of memory");
            return 1;
        }
        b->p = p;
        b->cap = capacity;
    }
    if (n) memcpy(b->p + b->n, data, n);
    b->n += n;
    return 0;
}

static int qs_rt_buf_char(qs_rt_buf *b, char c) { return qs_rt_buf_add(b, &c, 1); }

static int qs_rt_type_ok(int64_t type) {
    if (type >= 0 && type < QS_NTYPES) return 1;
    qs_rt_failf(QS_RT_TRAP, "internal: type descriptor out of range");
    return 0;
}

/* Formats like Session::format_into: strings are quoted only inside lists. */
static int qs_rt_format(qs_rt_buf *b, int64_t value, int64_t type, int quote) {
    char digits[32];
    qs_rt_obj *o;
    size_t i;
    if (!qs_rt_type_ok(type)) return 1;
    switch (qs_types[type].kind) {
    case QS_K_INT:
        snprintf(digits, sizeof digits, "%lld", (long long)value);
        return qs_rt_buf_add(b, digits, strlen(digits));
    case QS_K_STR:
        o = qs_rt_get(value, QS_K_STR);
        if (qs_rt_status) return 1;
        if (quote && qs_rt_buf_char(b, '"')) return 1;
        if (qs_rt_buf_add(b, o->bytes, o->len)) return 1;
        return quote ? qs_rt_buf_char(b, '"') : 0;
    case QS_K_LIST:
        o = qs_rt_get(value, QS_K_LIST);
        if (qs_rt_status) return 1;
        if (qs_rt_buf_char(b, '[')) return 1;
        for (i = 0; i < o->len; ++i) {
            if (i && qs_rt_buf_add(b, ", ", 2)) return 1;
            if (qs_rt_format(b, o->items[i], qs_types[type].elem, 1)) return 1;
        }
        return qs_rt_buf_char(b, ']');
    default:
        qs_rt_failf(QS_RT_TRAP, "internal: value cannot be printed by the C backend");
        return 1;
    }
}

static int qs_rt_emit_line(const char *p, size_t n) {
    if (!qs_rt_text) {
        qs_rt_failf(QS_RT_TRAP, "printing a string, list, grammar or match needs a text output (qs_set_text_output)");
        return 1;
    }
    if (qs_rt_text(p ? p : "", n, qs_rt_text_user)) {
        qs_rt_failf(QS_RT_OUTPUT, "text output failed");
        return 1;
    }
    return 0;
}

/* print of any value. Integers without a text output go to the integer output. */
static int qs_rt_print(int64_t value, int64_t type) {
    qs_rt_buf b = { NULL, 0, 0 };
    int status;
    if (!qs_rt_type_ok(type)) return 1;
    if (qs_types[type].kind == QS_K_INT && !qs_rt_text) {
        if (qs_rt_output && qs_rt_output(value, qs_rt_output_user)) {
            qs_rt_failf(QS_RT_OUTPUT, "output failed");
            return 1;
        }
        return 0;
    }
    status = qs_rt_format(&b, value, type, 0);
    if (!status) status = qs_rt_emit_line(b.p, b.n);
    free(b.p);
    return status;
}

static int qs_rt_compare(const unsigned char *a, size_t an, const unsigned char *b, size_t bn) {
    size_t m = an < bn ? an : bn;
    int c = m ? memcmp(a, b, m) : 0;
    if (c) return c < 0 ? -1 : 1;
    return an < bn ? -1 : (an > bn ? 1 : 0);
}

/* Offset of the first occurrence of p at or after from, or -1. */
static int64_t qs_rt_find(const unsigned char *s, size_t n, const unsigned char *p, size_t m, size_t from) {
    size_t i;
    if (m == 0) return (int64_t)from;
    for (i = from; i + m <= n; ++i)
        if (!memcmp(s + i, p, m)) return (int64_t)i;
    return -1;
}

static int qs_rt_starts(const unsigned char *s, size_t n, const unsigned char *p, size_t m) {
    return n >= m && (m == 0 || !memcmp(s, p, m));
}

static int qs_rt_ends(const unsigned char *s, size_t n, const unsigned char *p, size_t m) {
    return n >= m && (m == 0 || !memcmp(s + (n - m), p, m));
}

static int qs_rt_is_trim(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

/* Same grammar as metachain::text::integer and signed_integer: surrounding
   space/tab/CR/LF, an optional sign, an optional 0x/0b/0o prefix, then digits. */
static int qs_rt_to_int(const unsigned char *s, size_t n, int64_t *out) {
    size_t a = 0, b = n;
    int negative = 0, base = 10;
    uint64_t magnitude = 0, limit;
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    if (a < b && (s[a] == '+' || s[a] == '-')) negative = s[a++] == '-';
    if (b - a >= 2 && s[a] == '0') {
        switch (s[a + 1]) {
        case 'x': case 'X': base = 16; break;
        case 'b': case 'B': base = 2; break;
        case 'o': case 'O': base = 8; break;
        default: break;
        }
        if (base != 10) a += 2;
    }
    if (a >= b) return 0;
    for (; a < b; ++a) {
        unsigned char c = s[a];
        unsigned digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'z') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') digit = c - 'A' + 10;
        else return 0;
        if (digit >= (unsigned)base) return 0;
        if (magnitude > (UINT64_MAX - digit) / (uint64_t)base) return 0;
        magnitude = magnitude * (uint64_t)base + digit;
    }
    limit = (uint64_t)INT64_MAX + (negative ? 1u : 0u);
    if (magnitude > limit) return 0;
    if (negative && magnitude == ((uint64_t)1 << 63)) *out = INT64_MIN;
    else *out = negative ? -(int64_t)magnitude : (int64_t)magnitude;
    return 1;
}

static int qs_rt_cmp_int(const void *a, const void *b) {
    int64_t x = *(const int64_t *)a, y = *(const int64_t *)b;
    return x < y ? -1 : (x > y);
}

static int qs_rt_cmp_str(const void *a, const void *b) {
    const qs_rt_obj *x = qs_rt_get(*(const int64_t *)a, QS_K_STR);
    const qs_rt_obj *y = qs_rt_get(*(const int64_t *)b, QS_K_STR);
    return qs_rt_compare(x->bytes, x->len, y->bytes, y->len);
}

/* The built-in dispatcher. Returns 0 for ids that are not built-ins (the host
   callback handles them), 1 when handled (result in *value), or a negative
   status when the run must stop. */
static int qs_rt_builtin(int64_t id, const int64_t *args, int64_t *value, void *user) {
    int64_t result = 0;
    (void)user;
    if (id < QS_BUILTIN_BASE || id >= QS_BUILTIN_BASE + 256) return 0;
    switch (id - QS_BUILTIN_BASE) {
    case QS_B_Die: {
        qs_rt_obj *o = qs_rt_get(args[0], QS_K_STR);
        if (qs_rt_status) break;
        if (o->len == 0) qs_rt_failf(QS_RT_TRAP, "die");
        else qs_rt_failf(QS_RT_TRAP, "%.*s", (int)(o->len > 400 ? 400 : o->len), (const char *)o->bytes);
        break;
    }
    case QS_B_StrLiteral: {
        if (args[0] < 0 || args[0] >= (int64_t)QS_NSTRINGS) {
            qs_rt_failf(QS_RT_TRAP, "internal: string literal out of range");
            break;
        }
        if (!qs_rt_literals[args[0]])
            qs_rt_literals[args[0]] = qs_rt_string_copy((const unsigned char *)qs_strings[args[0]].p, qs_strings[args[0]].n);
        result = qs_rt_literals[args[0]];
        break;
    }
    case QS_B_ListNew:
        result = qs_rt_alloc(QS_K_LIST, 0);
        break;
    case QS_B_ListPush: {
        qs_rt_obj *o = qs_rt_get(args[0], QS_K_LIST);
        if (qs_rt_status || qs_rt_charge(8)) break;
        if (qs_rt_reserve(o, o->len + 1)) break;
        o->items[o->len++] = args[1];
        break;
    }
    case QS_B_ListGet: {
        qs_rt_obj *o = qs_rt_get(args[0], QS_K_LIST);
        if (qs_rt_status || !qs_rt_index_ok(args[1], o->len)) break;
        result = o->items[args[1]];
        break;
    }
    case QS_B_ListSet: {
        qs_rt_obj *o = qs_rt_get(args[0], QS_K_LIST);
        if (qs_rt_status || !qs_rt_index_ok(args[1], o->len)) break;
        o->items[args[1]] = args[2];
        break;
    }
    case QS_B_ListLen: {
        qs_rt_obj *o = qs_rt_get(args[0], QS_K_LIST);
        if (!qs_rt_status) result = (int64_t)o->len;
        break;
    }
    case QS_B_ListPop: {
        qs_rt_obj *o = qs_rt_get(args[0], QS_K_LIST);
        if (qs_rt_status) break;
        if (o->len == 0) { qs_rt_failf(QS_RT_TRAP, "pop from an empty list"); break; }
        result = o->items[--o->len];
        break;
    }
    case QS_B_ListLast: {
        qs_rt_obj *o = qs_rt_get(args[0], QS_K_LIST);
        if (qs_rt_status) break;
        if (o->len == 0) { qs_rt_failf(QS_RT_TRAP, "last of an empty list"); break; }
        result = o->items[o->len - 1];
        break;
    }
    case QS_B_ListContains: {
        qs_rt_obj *o = qs_rt_get(args[0], QS_K_LIST);
        size_t i;
        int is_int;
        if (qs_rt_status || !qs_rt_type_ok(args[2])) break;
        is_int = qs_types[args[2]].kind == QS_K_INT;
        for (i = 0; i < o->len; ++i) {
            if (is_int) {
                if (o->items[i] == args[1]) { result = 1; break; }
            } else {
                const qs_rt_obj *x = qs_rt_get(o->items[i], QS_K_STR);
                const qs_rt_obj *y = qs_rt_get(args[1], QS_K_STR);
                if (qs_rt_status) break;
                if (!qs_rt_compare(x->bytes, x->len, y->bytes, y->len)) { result = 1; break; }
            }
        }
        break;
    }
    case QS_B_ListSort: {
        qs_rt_obj *o = qs_rt_get(args[0], QS_K_LIST);
        if (qs_rt_status || !qs_rt_type_ok(args[1])) break;
        if (o->len > 1) {
            if (qs_types[args[1]].kind == QS_K_INT) qsort(o->items, o->len, sizeof *o->items, qs_rt_cmp_int);
            else qsort(o->items, o->len, sizeof *o->items, qs_rt_cmp_str);
        }
        break;
    }
    case QS_B_ListReverse: {
        qs_rt_obj *o = qs_rt_get(args[0], QS_K_LIST);
        size_t i;
        if (qs_rt_status) break;
        for (i = 0; i < o->len / 2; ++i) {
            int64_t t = o->items[i];
            o->items[i] = o->items[o->len - 1 - i];
            o->items[o->len - 1 - i] = t;
        }
        break;
    }
    case QS_B_ListClear: {
        qs_rt_obj *o = qs_rt_get(args[0], QS_K_LIST);
        if (!qs_rt_status) o->len = 0;
        break;
    }
    case QS_B_ListJoin: {
        qs_rt_obj *o = qs_rt_get(args[0], QS_K_LIST);
        qs_rt_obj *sep = qs_rt_get(args[1], QS_K_STR);
        qs_rt_buf b = { NULL, 0, 0 };
        size_t i;
        if (qs_rt_status) break;
        for (i = 0; i < o->len; ++i) {
            qs_rt_obj *item;
            if (i && qs_rt_buf_add(&b, sep->bytes, sep->len)) break;
            item = qs_rt_get(o->items[i], QS_K_STR);
            if (qs_rt_status || qs_rt_buf_add(&b, item->bytes, item->len)) break;
        }
        if (!qs_rt_status) result = qs_rt_string_copy((const unsigned char *)b.p, b.n);
        free(b.p);
        break;
    }
    case QS_B_ListSlice: {
        qs_rt_obj *o = qs_rt_get(args[0], QS_K_LIST);
        int64_t from = args[1], count = args[2];
        size_t len, n;
        int64_t *source;
        if (qs_rt_status) break;
        len = o->len;
        if (from < 0 || count < 0 || (uint64_t)from > len || (uint64_t)count > len - (size_t)from) {
            qs_rt_failf(QS_RT_TRAP, "slice out of range");
            break;
        }
        n = (size_t)count;
        source = o->items;              /* the items block does not move with the heap */
        result = qs_rt_alloc(QS_K_LIST, n * 8);
        if (!result) break;
        if (qs_rt_reserve(&qs_rt_heap[result - 1], n)) break;
        if (n) memcpy(qs_rt_heap[result - 1].items, source + from, n * sizeof *source);
        qs_rt_heap[result - 1].len = n;
        break;
    }
    case QS_B_StrCompare: {
        qs_rt_obj *a = qs_rt_get(args[0], QS_K_STR);
        qs_rt_obj *b = qs_rt_get(args[1], QS_K_STR);
        if (!qs_rt_status) result = qs_rt_compare(a->bytes, a->len, b->bytes, b->len);
        break;
    }
    case QS_B_StrConcat: {
        qs_rt_obj *a = qs_rt_get(args[0], QS_K_STR);
        qs_rt_obj *b = qs_rt_get(args[1], QS_K_STR);
        const unsigned char *ap, *bp;
        size_t an, bn;
        unsigned char *bytes = NULL;
        int64_t handle;
        if (qs_rt_status) break;
        ap = a->bytes; an = a->len; bp = b->bytes; bn = b->len;
        handle = qs_rt_string(an + bn, &bytes);
        if (!handle) break;
        if (an) memcpy(bytes, ap, an);
        if (bn) memcpy(bytes + an, bp, bn);
        result = handle;
        break;
    }
    case QS_B_StrLen: {
        qs_rt_obj *s = qs_rt_get(args[0], QS_K_STR);
        if (!qs_rt_status) result = (int64_t)s->len;
        break;
    }
    case QS_B_StrAt: {
        qs_rt_obj *s = qs_rt_get(args[0], QS_K_STR);
        if (qs_rt_status || !qs_rt_index_ok(args[1], s->len)) break;
        result = s->bytes[args[1]];
        break;
    }
    case QS_B_StrSub: {
        qs_rt_obj *s = qs_rt_get(args[0], QS_K_STR);
        int64_t from = args[1], count = args[2];
        const unsigned char *p;
        size_t len;
        if (qs_rt_status) break;
        len = s->len;
        if (from < 0 || count < 0 || (uint64_t)from > len || (uint64_t)count > len - (size_t)from) {
            qs_rt_failf(QS_RT_TRAP, "substring out of range");
            break;
        }
        p = s->bytes + from;
        result = qs_rt_string_copy(p, (size_t)count);
        break;
    }
    case QS_B_StrFind: {
        qs_rt_obj *s = qs_rt_get(args[0], QS_K_STR);
        qs_rt_obj *p = qs_rt_get(args[1], QS_K_STR);
        if (!qs_rt_status) result = qs_rt_find(s->bytes, s->len, p->bytes, p->len, 0);
        break;
    }
    case QS_B_StrFindFrom: {
        qs_rt_obj *s = qs_rt_get(args[0], QS_K_STR);
        qs_rt_obj *p = qs_rt_get(args[1], QS_K_STR);
        if (qs_rt_status) break;
        if (args[2] < 0 || (uint64_t)args[2] > s->len) {
            qs_rt_failf(QS_RT_TRAP, "search position out of range");
            break;
        }
        result = qs_rt_find(s->bytes, s->len, p->bytes, p->len, (size_t)args[2]);
        break;
    }
    case QS_B_StrStarts: {
        qs_rt_obj *s = qs_rt_get(args[0], QS_K_STR);
        qs_rt_obj *p = qs_rt_get(args[1], QS_K_STR);
        if (!qs_rt_status) result = qs_rt_starts(s->bytes, s->len, p->bytes, p->len);
        break;
    }
    case QS_B_StrEnds: {
        qs_rt_obj *s = qs_rt_get(args[0], QS_K_STR);
        qs_rt_obj *p = qs_rt_get(args[1], QS_K_STR);
        if (!qs_rt_status) result = qs_rt_ends(s->bytes, s->len, p->bytes, p->len);
        break;
    }
    case QS_B_StrContains: {
        qs_rt_obj *s = qs_rt_get(args[0], QS_K_STR);
        qs_rt_obj *p = qs_rt_get(args[1], QS_K_STR);
        if (!qs_rt_status) result = qs_rt_find(s->bytes, s->len, p->bytes, p->len, 0) >= 0;
        break;
    }
    case QS_B_StrSplit: {
        qs_rt_obj *s = qs_rt_get(args[0], QS_K_STR);
        qs_rt_obj *sep = qs_rt_get(args[1], QS_K_STR);
        const unsigned char *sp, *pp;
        size_t sn, pn, start = 0, count = 0, capacity = 0;
        int64_t *parts = NULL;
        int failed = 0;
        if (qs_rt_status) break;
        sp = s->bytes; sn = s->len; pp = sep->bytes; pn = sep->len;
        if (pn == 0) { qs_rt_failf(QS_RT_TRAP, "split separator must not be empty"); break; }
        for (;;) {
            int64_t found = qs_rt_find(sp, sn, pp, pn, start);
            size_t end = found < 0 ? sn : (size_t)found;
            int64_t piece;
            if (count == capacity) {
                size_t grown = capacity ? 2 * capacity : 8;
                int64_t *items = realloc(parts, grown * sizeof *items);
                if (!items) { qs_rt_failf(QS_RT_LIMIT, "out of memory"); failed = 1; break; }
                parts = items;
                capacity = grown;
            }
            piece = qs_rt_string_copy(sp + start, end - start);
            if (!piece) { failed = 1; break; }
            parts[count++] = piece;
            if (found < 0) break;
            start = (size_t)found + pn;
        }
        if (!failed) {
            result = qs_rt_alloc(QS_K_LIST, count * 8);
            if (result && !qs_rt_reserve(&qs_rt_heap[result - 1], count)) {
                if (count) memcpy(qs_rt_heap[result - 1].items, parts, count * sizeof *parts);
                qs_rt_heap[result - 1].len = count;
            }
        }
        free(parts);
        break;
    }
    case QS_B_StrRepeat: {
        qs_rt_obj *s = qs_rt_get(args[0], QS_K_STR);
        int64_t n = args[1];
        const unsigned char *p;
        size_t len, i, total;
        unsigned char *bytes = NULL;
        int64_t handle;
        if (qs_rt_status) break;
        if (n < 0) { qs_rt_failf(QS_RT_TRAP, "repeat count must not be negative"); break; }
        len = s->len;
        if (n > 0 && len > QS_MAX_BYTES / (size_t)n) { qs_rt_failf(QS_RT_LIMIT, "heap size limit exceeded"); break; }
        p = s->bytes;
        total = len * (size_t)n;
        handle = qs_rt_string(total, &bytes);
        if (!handle) break;
        for (i = 0; i < (size_t)n; ++i)
            if (len) memcpy(bytes + i * len, p, len);
        result = handle;
        break;
    }
    case QS_B_StrToInt: {
        qs_rt_obj *s = qs_rt_get(args[0], QS_K_STR);
        int64_t v = 0;
        if (qs_rt_status) break;
        if (!qs_rt_to_int(s->bytes, s->len, &v)) {
            qs_rt_failf(QS_RT_TRAP, "not a signed 64-bit integer: \"%.*s\"",
                        (int)(s->len > 300 ? 300 : s->len), (const char *)s->bytes);
            break;
        }
        result = v;
        break;
    }
    case QS_B_StrUpper:
    case QS_B_StrLower: {
        qs_rt_obj *s = qs_rt_get(args[0], QS_K_STR);
        const unsigned char *p;
        size_t len, i;
        unsigned char *bytes = NULL;
        int64_t handle;
        int upper = (id - QS_BUILTIN_BASE) == QS_B_StrUpper;
        if (qs_rt_status) break;
        p = s->bytes; len = s->len;
        handle = qs_rt_string(len, &bytes);
        if (!handle) break;
        for (i = 0; i < len; ++i) {
            unsigned char c = p[i];
            if (upper && c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 'A');
            else if (!upper && c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
            bytes[i] = c;
        }
        result = handle;
        break;
    }
    case QS_B_StrTrim: {
        qs_rt_obj *s = qs_rt_get(args[0], QS_K_STR);
        size_t first = 0, last, len;
        const unsigned char *p;
        if (qs_rt_status) break;
        p = s->bytes; len = s->len;
        while (first < len && qs_rt_is_trim(p[first])) ++first;
        if (first == len) { result = qs_rt_string_copy(NULL, 0); break; }
        last = len;
        while (last > first && qs_rt_is_trim(p[last - 1])) --last;
        result = qs_rt_string_copy(p + first, last - first);
        break;
    }
    case QS_B_StrReplace: {
        qs_rt_obj *s = qs_rt_get(args[0], QS_K_STR);
        qs_rt_obj *from = qs_rt_get(args[1], QS_K_STR);
        qs_rt_obj *to = qs_rt_get(args[2], QS_K_STR);
        qs_rt_buf b = { NULL, 0, 0 };
        size_t start = 0;
        if (qs_rt_status) break;
        if (from->len == 0) { qs_rt_failf(QS_RT_TRAP, "replace pattern must not be empty"); break; }
        for (;;) {
            int64_t found = qs_rt_find(s->bytes, s->len, from->bytes, from->len, start);
            if (found < 0) {
                if (qs_rt_buf_add(&b, s->bytes + start, s->len - start)) break;
                break;
            }
            if (qs_rt_buf_add(&b, s->bytes + start, (size_t)found - start)) break;
            if (qs_rt_buf_add(&b, to->bytes, to->len)) break;
            start = (size_t)found + from->len;
        }
        if (!qs_rt_status) result = qs_rt_string_copy((const unsigned char *)b.p, b.n);
        free(b.p);
        break;
    }
    case QS_B_IntToStr: {
        char digits[32];
        snprintf(digits, sizeof digits, "%lld", (long long)args[0]);
        result = qs_rt_string_copy((const unsigned char *)digits, strlen(digits));
        break;
    }
    case QS_B_IntToChar: {
        unsigned char c;
        if (args[0] < 0 || args[0] > 255) { qs_rt_failf(QS_RT_TRAP, "character code out of range 0..255"); break; }
        c = (unsigned char)args[0];
        result = qs_rt_string_copy(&c, 1);
        break;
    }
    case QS_B_PrintValue:
        qs_rt_print(args[0], args[1]);
        break;
    default:
        qs_rt_failf(QS_RT_TRAP, "internal: unknown built-in");
        break;
    }
    if (qs_rt_status) return -qs_rt_status;
    *value = result;
    return 1;
}
)QSRT";
}
} // namespace qscript::detail
