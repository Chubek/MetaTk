/**
 * @file mulib-json.h
 * @brief Owned JSON values, RFC 8259 parsing, and serialization.
 * Define MULIB_IMPL_JSON in one C11 translation unit. Dependencies are listed
 * in manual/mulib-json.md and must have their own implementation switches.
 */
#ifndef MULIB_JSON_H
#define MULIB_JSON_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_json_type {
    MULIB_JSON_NULL, MULIB_JSON_BOOL, MULIB_JSON_NUMBER,
    MULIB_JSON_STRING, MULIB_JSON_ARRAY, MULIB_JSON_OBJECT
} mulib_json_type;
typedef struct mulib_json mulib_json_t;
typedef struct mulib_json_error {
    size_t offset, line, column;
    const char *message;
} mulib_json_error_t;
/** @brief Allocate a zero/empty value of the given type; caller owns it. */
mulib_json_t *mulib_json_new(mulib_json_type type);
/** @brief Allocate a boolean value. */
mulib_json_t *mulib_json_bool(int value);
/** @brief Allocate a finite numeric value, or return NULL. */
mulib_json_t *mulib_json_number(double value);
/** @brief Copy a NUL-terminated string into a new value. */
mulib_json_t *mulib_json_string(const char *text);
/** @brief Copy a length-delimited UTF-8 string, including embedded NULs. */
mulib_json_t *mulib_json_string_n(const char *text, size_t length);
/** @brief Recursively release a value; NULL is harmless. */
void mulib_json_destroy(mulib_json_t *value);
/** @brief NULL pointers have type MULIB_JSON_NULL. */
mulib_json_type mulib_json_typeof(const mulib_json_t *value);
/** @brief Return an array/object's number of members, otherwise zero. */
size_t mulib_json_size(const mulib_json_t *value);
/** @brief Borrow a string's bytes; return NULL for other types. */
const char *mulib_json_text(const mulib_json_t *value, size_t *length);
/** @brief Return a number, or zero for other types. */
double mulib_json_numeric(const mulib_json_t *value);
/** @brief Return a boolean, or zero for other types. */
int mulib_json_boolean(const mulib_json_t *value);
/** @brief Borrow an indexed array element or object value. */
mulib_json_t *mulib_json_at(const mulib_json_t *value, size_t index);
/** @brief Borrow an indexed object's key; keys can contain NUL bytes. */
const char *mulib_json_key(const mulib_json_t *object, size_t index, size_t *length);
/** @brief Borrow an object member by NUL-terminated key. */
mulib_json_t *mulib_json_get(const mulib_json_t *object, const char *key);
/** @brief Borrow an object member by length-delimited key. */
mulib_json_t *mulib_json_get_n(const mulib_json_t *object, const char *key, size_t length);
/** @brief Append; takes ownership of value only on success. */
int mulib_json_append(mulib_json_t *array, mulib_json_t *value);
/** @brief Insert/replace; copies key and takes value only on success. */
int mulib_json_set(mulib_json_t *object, const char *key, mulib_json_t *value);
/** @brief Length-delimited version of mulib_json_set. */
int mulib_json_set_n(mulib_json_t *object, const char *key, size_t length, mulib_json_t *value);
/** @brief Parse exactly one UTF-8 JSON value; return NULL with a diagnostic. */
mulib_json_t *mulib_json_parse(const char *source, size_t length, mulib_json_error_t *error);
/** @brief Return malloc-owned JSON text; pretty enables indentation. */
char *mulib_json_stringify(const mulib_json_t *value, int pretty);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_JSON_H */

#if defined(MULIB_IMPL_JSON) && !defined(MULIB_JSON_IMPLEMENTED)
#define MULIB_JSON_IMPLEMENTED
#include "mulib-parsec.h"
#include "mulib-strbuf.h"
#include "mulib-utf8.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <locale.h>
#ifndef MULIB_JSON_MAX_DEPTH
#define MULIB_JSON_MAX_DEPTH 128
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_json_entry_ {
    char *key;
    size_t key_length;
    mulib_json_t *value;
} mulib_json_entry_;
struct mulib_json {
    mulib_json_type type;
    mulib_json_t *parent;
    union {
        int boolean;
        double number;
        struct { char *bytes; size_t length; } string;
        struct { mulib_json_entry_ *items; size_t length, capacity; } list;
    } as;
};
static char *mulib_json_copy_(const char *s, size_t n) {
    char *p;
    if ((!s && n) || n == SIZE_MAX) return NULL;
    p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
mulib_json_t *mulib_json_new(mulib_json_type type) {
    mulib_json_t *v;
    if (type < MULIB_JSON_NULL || type > MULIB_JSON_OBJECT) return NULL;
    v = (mulib_json_t *)calloc(1, sizeof *v);
    if (!v) return NULL;
    v->type = type;
    if (type == MULIB_JSON_NUMBER) v->as.number = 0.0;
    if (type == MULIB_JSON_STRING) {
        v->as.string.bytes = mulib_json_copy_(NULL, 0);
        if (!v->as.string.bytes) { free(v); return NULL; }
    }
    return v;
}
mulib_json_t *mulib_json_bool(int b) {
    mulib_json_t *v = mulib_json_new(MULIB_JSON_BOOL);
    if (v) v->as.boolean = !!b;
    return v;
}
mulib_json_t *mulib_json_number(double d) {
    mulib_json_t *v;
    if (!isfinite(d)) return NULL;
    v = mulib_json_new(MULIB_JSON_NUMBER);
    if (v) v->as.number = d;
    return v;
}
mulib_json_t *mulib_json_string_n(const char *s, size_t n) {
    mulib_json_t *v;
    char *copy;
    if (n == SIZE_MAX || !mulib_utf8_validate((const unsigned char *)s, n)) return NULL;
    copy = mulib_json_copy_(s, n);
    if (!copy) return NULL;
    v = (mulib_json_t *)calloc(1, sizeof *v);
    if (!v) { free(copy); return NULL; }
    v->type = MULIB_JSON_STRING; v->as.string.bytes = copy; v->as.string.length = n;
    return v;
}
mulib_json_t *mulib_json_string(const char *s) { return s ? mulib_json_string_n(s, strlen(s)) : NULL; }
void mulib_json_destroy(mulib_json_t *v) {
    mulib_json_t *stop = v ? v->parent : NULL;
    while (v && v != stop) {
        mulib_json_t *parent;
        if ((v->type == MULIB_JSON_ARRAY || v->type == MULIB_JSON_OBJECT) && v->as.list.length) {
            mulib_json_entry_ *e = &v->as.list.items[--v->as.list.length];
            free(e->key); v = e->value; continue;
        }
        parent = v->parent;
        if (v->type == MULIB_JSON_STRING) free(v->as.string.bytes);
        else if (v->type == MULIB_JSON_ARRAY || v->type == MULIB_JSON_OBJECT) free(v->as.list.items);
        free(v); v = parent;
    }
}
mulib_json_type mulib_json_typeof(const mulib_json_t *v) { return v ? v->type : MULIB_JSON_NULL; }
size_t mulib_json_size(const mulib_json_t *v) {
    return v && (v->type == MULIB_JSON_ARRAY || v->type == MULIB_JSON_OBJECT) ? v->as.list.length : 0;
}
const char *mulib_json_text(const mulib_json_t *v, size_t *n) {
    if (n) *n = v && v->type == MULIB_JSON_STRING ? v->as.string.length : 0;
    return v && v->type == MULIB_JSON_STRING ? v->as.string.bytes : NULL;
}
double mulib_json_numeric(const mulib_json_t *v) { return v && v->type == MULIB_JSON_NUMBER ? v->as.number : 0.0; }
int mulib_json_boolean(const mulib_json_t *v) { return v && v->type == MULIB_JSON_BOOL ? v->as.boolean : 0; }
mulib_json_t *mulib_json_at(const mulib_json_t *v, size_t i) {
    return i < mulib_json_size(v) ? v->as.list.items[i].value : NULL;
}
const char *mulib_json_key(const mulib_json_t *v, size_t i, size_t *n) {
    if (!v || v->type != MULIB_JSON_OBJECT || i >= v->as.list.length) { if (n) *n = 0; return NULL; }
    if (n) *n = v->as.list.items[i].key_length;
    return v->as.list.items[i].key;
}
static size_t mulib_json_find_(const mulib_json_t *v, const char *key, size_t n) {
    size_t i;
    if (!v || v->type != MULIB_JSON_OBJECT || (!key && n)) return SIZE_MAX;
    for (i = 0; i < v->as.list.length; ++i)
        if (v->as.list.items[i].key_length == n && (!n || !memcmp(v->as.list.items[i].key, key, n))) return i;
    return SIZE_MAX;
}
mulib_json_t *mulib_json_get_n(const mulib_json_t *v, const char *key, size_t n) {
    size_t i = mulib_json_find_(v, key, n);
    return i == SIZE_MAX ? NULL : v->as.list.items[i].value;
}
mulib_json_t *mulib_json_get(const mulib_json_t *v, const char *key) {
    return key ? mulib_json_get_n(v, key, strlen(key)) : NULL;
}
static int mulib_json_attachable_(mulib_json_t *parent, mulib_json_t *v) {
    mulib_json_t *p;
    if (!v || v->parent) return 0;
    for (p = parent; p; p = p->parent) if (p == v) return 0;
    return 1;
}
static int mulib_json_reserve_(mulib_json_t *v) {
    size_t cap;
    mulib_json_entry_ *p;
    if (v->as.list.length < v->as.list.capacity) return 1;
    cap = v->as.list.capacity;
    if (cap > SIZE_MAX / 2) return 0;
    cap = cap ? cap * 2 : 8;
    if (cap > SIZE_MAX / sizeof *p) return 0;
    p = (mulib_json_entry_ *)realloc(v->as.list.items, cap * sizeof *p);
    if (!p) return 0;
    v->as.list.items = p; v->as.list.capacity = cap; return 1;
}
int mulib_json_append(mulib_json_t *a, mulib_json_t *v) {
    mulib_json_entry_ *e;
    if (!a || a->type != MULIB_JSON_ARRAY || !mulib_json_attachable_(a, v) || !mulib_json_reserve_(a)) return 0;
    e = &a->as.list.items[a->as.list.length++]; e->key = NULL; e->key_length = 0; e->value = v;
    v->parent = a; return 1;
}
int mulib_json_set_n(mulib_json_t *o, const char *key, size_t n, mulib_json_t *v) {
    size_t i;
    char *copy;
    if (!o || o->type != MULIB_JSON_OBJECT || n == SIZE_MAX ||
        !mulib_utf8_validate((const unsigned char *)key, n)) return 0;
    i = mulib_json_find_(o, key, n);
    if (i != SIZE_MAX && o->as.list.items[i].value == v) return 1;
    if (!mulib_json_attachable_(o, v)) return 0;
    if (i != SIZE_MAX) {
        mulib_json_t *old = o->as.list.items[i].value;
        old->parent = NULL; mulib_json_destroy(old);
        o->as.list.items[i].value = v; v->parent = o; return 1;
    }
    copy = mulib_json_copy_(key, n);
    if (!copy) return 0;
    if (!mulib_json_reserve_(o)) { free(copy); return 0; }
    i = o->as.list.length++; o->as.list.items[i].key = copy;
    o->as.list.items[i].key_length = n; o->as.list.items[i].value = v; v->parent = o;
    return 1;
}
int mulib_json_set(mulib_json_t *o, const char *key, mulib_json_t *v) {
    return key ? mulib_json_set_n(o, key, strlen(key), v) : 0;
}
static void mulib_json_error_(mulib_json_error_t *e, const char *s, size_t at, const char *message) {
    size_t i;
    if (!e) return;
    e->offset = at; e->line = e->column = 1; e->message = message;
    for (i = 0; i < at; ++i) {
        if (s[i] == '\n') { ++e->line; e->column = 1; } else ++e->column;
    }
}
static int mulib_json_space_(unsigned char c, void *u) { (void)u; return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static int mulib_json_raw_(unsigned char c, void *u) { (void)u; return c >= 32 && c != '"' && c != '\\'; }
static int mulib_json_hex_(unsigned char c, void *u) {
    (void)u; return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
static unsigned mulib_json_hex_value_(unsigned char c) {
    return c <= '9' ? (unsigned)(c - '0') : c <= 'F' ? (unsigned)(c - 'A' + 10) : (unsigned)(c - 'a' + 10);
}
static uint32_t mulib_json_u4_(const char *s) {
    uint32_t c = 0; size_t i;
    for (i = 0; i < 4; ++i) c = (c << 4) | mulib_json_hex_value_((unsigned char)s[i]);
    return c;
}
/* Syntax is exclusively recognized by the combinator grammar below. This
 * routine only decodes a successfully captured JSON string's escape values. */
static mulib_json_t *mulib_json_decode_(const char *s, const mulib_parsec_capture_t *c,
                                       mulib_json_error_t *error) {
    mulib_strbuf_t b;
    size_t i = c->offset + 1, end = c->offset + c->length - 1;
    mulib_json_t *v;
    mulib_strbuf_init(&b);
    while (i < end) {
        unsigned char ch = (unsigned char)s[i++];
        if (ch == '\\') {
            size_t escape = i - 1;
            ch = (unsigned char)s[i++];
            switch (ch) {
            case 'b': ch = '\b'; break;
            case 'f': ch = '\f'; break;
            case 'n': ch = '\n'; break;
            case 'r': ch = '\r'; break;
            case 't': ch = '\t'; break;
            case 'u': {
                uint32_t cp = mulib_json_u4_(s + i);
                unsigned char bytes[4]; size_t n;
                i += 4;
                if (cp >= 0xd800 && cp <= 0xdbff) {
                    uint32_t lo;
                    if (end - i < 6 || s[i] != '\\' || s[i + 1] != 'u') goto surrogate;
                    lo = mulib_json_u4_(s + i + 2);
                    if (lo < 0xdc00 || lo > 0xdfff) goto surrogate;
                    cp = 0x10000 + ((cp - 0xd800) << 10) + lo - 0xdc00; i += 6;
                } else if (cp >= 0xdc00 && cp <= 0xdfff) goto surrogate;
                n = mulib_utf8_encode(cp, bytes, sizeof bytes);
                if (!n || !mulib_strbuf_append_n(&b, (const char *)bytes, n)) goto memory;
                continue;
surrogate:
                mulib_json_error_(error, s, escape, "unpaired Unicode surrogate");
                mulib_strbuf_free(&b); return NULL;
            }
            default: break;
            }
        }
        if (!mulib_strbuf_putc(&b, (char)ch)) goto memory;
    }
    v = mulib_json_string_n(b.data, b.length);
    if (!v) goto memory;
    mulib_strbuf_free(&b); return v;
memory:
    mulib_json_error_(error, s, c->offset, "out of memory"); mulib_strbuf_free(&b); return NULL;
}
/* Use the C library's correctly rounded conversion without assuming LC_NUMERIC
 * is C. No process-global locale is changed. */
static mulib_json_t *mulib_json_decode_number_(const char *s, size_t n) {
    struct lconv *lc = localeconv();
    const char *decimal = lc && lc->decimal_point && *lc->decimal_point ? lc->decimal_point : ".";
    mulib_strbuf_t b;
    size_t i; char *end; double d; mulib_json_t *v = NULL;
    mulib_strbuf_init(&b);
    for (i = 0; i < n; ++i) {
        if (s[i] == '.') { if (!mulib_strbuf_append(&b, decimal)) goto done; }
        else if (!mulib_strbuf_putc(&b, s[i])) goto done;
    }
    d = strtod(b.data, &end);
    if (end == b.data + b.length && isfinite(d)) v = mulib_json_number(d);
done:
    mulib_strbuf_free(&b); return v;
}
typedef struct mulib_json_build_ { mulib_json_t *value; size_t index, depth; } mulib_json_build_;
static mulib_json_t *mulib_json_build_values_(const char *source, const mulib_parsec_result_t *r,
                                             mulib_json_error_t *error) {
    mulib_json_build_ *stack;
    size_t used = 0, i, j;
    mulib_json_t *v = NULL;
    if (!r->count || r->count > SIZE_MAX / sizeof *stack) {
        mulib_json_error_(error, source, 0, "capture allocation overflow"); return NULL;
    }
    stack = (mulib_json_build_ *)calloc(r->count, sizeof *stack);
    if (!stack) { mulib_json_error_(error, source, 0, "out of memory"); return NULL; }
    for (i = 0; i < r->count; ++i) {
        const mulib_parsec_capture_t *c = &r->captures[i];
        size_t base = used, depth = 0;
        v = NULL;
        if (c->tag == MULIB_JSON_STRING) {
            v = mulib_json_decode_(source, c, error); if (!v) goto fail;
        } else if (c->tag == MULIB_JSON_NUMBER) {
            v = mulib_json_decode_number_(source + c->offset, c->length);
            if (!v) { mulib_json_error_(error, source, c->offset, "number out of finite double range or out of memory"); goto fail; }
        } else if (c->tag == MULIB_JSON_BOOL) v = mulib_json_bool(source[c->offset] == 't');
        else v = mulib_json_new((mulib_json_type)c->tag);
        if (!v) { mulib_json_error_(error, source, c->offset, "out of memory"); goto fail; }
        if (c->tag == MULIB_JSON_ARRAY || c->tag == MULIB_JSON_OBJECT) {
            while (base && stack[base - 1].index >= c->first) --base;
            for (j = base; j < used; ++j) if (stack[j].depth > depth) depth = stack[j].depth;
            if (depth >= (size_t)MULIB_JSON_MAX_DEPTH) {
                mulib_json_error_(error, source, c->offset, "JSON nesting limit exceeded"); goto fail;
            }
            ++depth;
            for (j = base; j < used; ++j) {
                int ok;
                if (c->tag == MULIB_JSON_ARRAY) {
                    ok = mulib_json_append(v, stack[j].value);
                    if (ok) stack[j].value = NULL;
                } else {
                    mulib_json_t *key = stack[j].value;
                    ok = mulib_json_set_n(v, key->as.string.bytes, key->as.string.length, stack[j + 1].value);
                    if (ok) {
                        stack[j + 1].value = NULL; mulib_json_destroy(key); stack[j].value = NULL;
                    }
                    ++j;
                }
                if (!ok) { mulib_json_error_(error, source, c->offset, "out of memory"); goto fail; }
            }
            used = base;
        }
        stack[used].value = v; stack[used].index = i; stack[used].depth = depth; ++used; v = NULL;
    }
    v = stack[0].value; free(stack); return v;
fail:
    mulib_json_destroy(v);
    for (i = 0; i < used; ++i) mulib_json_destroy(stack[i].value);
    free(stack); return NULL;
}
mulib_json_t *mulib_json_parse(const char *source, size_t length, mulib_json_error_t *error) {
    /* Each lexical and structural production is composed from public parsec
     * primitives; captures commit only after the entire grammar succeeds. */
    const mulib_parsec_parser_t *value_target = NULL;
    mulib_parsec_parser_t ws_byte = mulib_parsec_satisfy(mulib_json_space_, NULL, "JSON whitespace");
    mulib_parsec_parser_t ws = mulib_parsec_repeat(&ws_byte, 0, SIZE_MAX);
    mulib_parsec_parser_t hex = mulib_parsec_satisfy(mulib_json_hex_, NULL, "hexadecimal digit");
    mulib_parsec_parser_t unicode_parts[2], escape_alts[2], escape_parts[2], string_alts[2], string_parts[3];
    mulib_parsec_parser_t unicode, escaped, string_unit, string_body, string;
    mulib_parsec_parser_t simple_alts[9];
    mulib_parsec_parser_t digit = mulib_parsec_range('0', '9');
    mulib_parsec_parser_t digits = mulib_parsec_repeat(&digit, 0, SIZE_MAX);
    mulib_parsec_parser_t nonzero_parts[2], integer_alts[2], fraction_parts[2], exponent_parts[3];
    mulib_parsec_parser_t nonzero, integer, fraction, exponent, minus, sign, e;
    mulib_parsec_parser_t sign_alts[2], e_alts[2], number_parts[4], number_body, number;
    mulib_parsec_parser_t true_lit = mulib_parsec_literal("true", 4);
    mulib_parsec_parser_t false_lit = mulib_parsec_literal("false", 5);
    mulib_parsec_parser_t null_lit = mulib_parsec_literal("null", 4);
    mulib_parsec_parser_t value_ws_parts[2], value_ws, array_tail_parts[3], array_tail;
    mulib_parsec_parser_t array_items_parts[2], array_items, array_parts[4], array_body, array;
    mulib_parsec_parser_t member_parts[5], member, object_tail_parts[3], object_tail;
    mulib_parsec_parser_t object_items_parts[2], object_items, object_parts[4], object_body, object;
    mulib_parsec_parser_t value_alts[7], value, root_parts[4], root;
    mulib_parsec_options_t options;
    mulib_parsec_result_t result;
    mulib_json_t *answer;
    size_t i, depth = (size_t)MULIB_JSON_MAX_DEPTH;
    static const char simple[] = "\"\\/bfnrt";
    if (error) memset(error, 0, sizeof *error);
    if ((!source && length) || length == SIZE_MAX) {
        mulib_json_error_(error, source, 0, "invalid input"); return NULL;
    }
    for (i = 0; i < length;) {
        uint32_t cp; size_t n;
        if (!mulib_utf8_decode((const unsigned char *)source + i, length - i, &cp, &n)) {
            mulib_json_error_(error, source, i, "invalid UTF-8"); return NULL;
        }
        i += n;
    }
    unicode_parts[0] = mulib_parsec_byte('u'); unicode_parts[1] = mulib_parsec_repeat(&hex, 4, 4);
    unicode = mulib_parsec_sequence(unicode_parts, 2);
    for (i = 0; i < sizeof simple - 1; ++i) simple_alts[i] = mulib_parsec_byte((unsigned char)simple[i]);
    escape_alts[0] = mulib_parsec_choice(simple_alts, sizeof simple - 1); escape_alts[1] = unicode;
    escape_parts[0] = mulib_parsec_byte('\\'); escape_parts[1] = mulib_parsec_choice(escape_alts, 2);
    escaped = mulib_parsec_sequence(escape_parts, 2);
    string_alts[0] = mulib_parsec_satisfy(mulib_json_raw_, NULL, "unescaped string byte"); string_alts[1] = escaped;
    string_unit = mulib_parsec_choice(string_alts, 2);
    string_parts[0] = mulib_parsec_byte('"'); string_parts[1] = mulib_parsec_repeat(&string_unit, 0, SIZE_MAX);
    string_parts[2] = mulib_parsec_named(mulib_parsec_byte('"'), "closing quote");
    string_body = mulib_parsec_sequence(string_parts, 3); string = mulib_parsec_capture(&string_body, MULIB_JSON_STRING);
    nonzero_parts[0] = mulib_parsec_range('1', '9'); nonzero_parts[1] = digits;
    nonzero = mulib_parsec_sequence(nonzero_parts, 2);
    integer_alts[0] = mulib_parsec_byte('0'); integer_alts[1] = nonzero;
    integer = mulib_parsec_choice(integer_alts, 2);
    fraction_parts[0] = mulib_parsec_byte('.'); fraction_parts[1] = mulib_parsec_repeat(&digit, 1, SIZE_MAX);
    fraction = mulib_parsec_sequence(fraction_parts, 2);
    sign_alts[0] = mulib_parsec_byte('+'); sign_alts[1] = mulib_parsec_byte('-'); sign = mulib_parsec_choice(sign_alts, 2);
    e_alts[0] = mulib_parsec_byte('e'); e_alts[1] = mulib_parsec_byte('E'); e = mulib_parsec_choice(e_alts, 2);
    exponent_parts[0] = e; exponent_parts[1] = mulib_parsec_repeat(&sign, 0, 1);
    exponent_parts[2] = mulib_parsec_repeat(&digit, 1, SIZE_MAX); exponent = mulib_parsec_sequence(exponent_parts, 3);
    minus = mulib_parsec_byte('-'); number_parts[0] = mulib_parsec_repeat(&minus, 0, 1); number_parts[1] = integer;
    number_parts[2] = mulib_parsec_repeat(&fraction, 0, 1); number_parts[3] = mulib_parsec_repeat(&exponent, 0, 1);
    number_body = mulib_parsec_sequence(number_parts, 4); number = mulib_parsec_capture(&number_body, MULIB_JSON_NUMBER);
    value_ws_parts[0] = mulib_parsec_ref(&value_target); value_ws_parts[1] = ws;
    value_ws = mulib_parsec_sequence(value_ws_parts, 2);
    array_tail_parts[0] = mulib_parsec_byte(','); array_tail_parts[1] = ws; array_tail_parts[2] = value_ws;
    array_tail = mulib_parsec_sequence(array_tail_parts, 3);
    array_items_parts[0] = value_ws; array_items_parts[1] = mulib_parsec_repeat(&array_tail, 0, SIZE_MAX);
    array_items = mulib_parsec_sequence(array_items_parts, 2);
    array_parts[0] = mulib_parsec_byte('['); array_parts[1] = ws;
    array_parts[2] = mulib_parsec_repeat(&array_items, 0, 1);
    array_parts[3] = mulib_parsec_named(mulib_parsec_byte(']'), "closing array bracket");
    array_body = mulib_parsec_sequence(array_parts, 4); array = mulib_parsec_capture(&array_body, MULIB_JSON_ARRAY);
    member_parts[0] = string; member_parts[1] = ws; member_parts[2] = mulib_parsec_byte(':');
    member_parts[3] = ws; member_parts[4] = value_ws; member = mulib_parsec_sequence(member_parts, 5);
    object_tail_parts[0] = mulib_parsec_byte(','); object_tail_parts[1] = ws; object_tail_parts[2] = member;
    object_tail = mulib_parsec_sequence(object_tail_parts, 3);
    object_items_parts[0] = member; object_items_parts[1] = mulib_parsec_repeat(&object_tail, 0, SIZE_MAX);
    object_items = mulib_parsec_sequence(object_items_parts, 2);
    object_parts[0] = mulib_parsec_byte('{'); object_parts[1] = ws;
    object_parts[2] = mulib_parsec_repeat(&object_items, 0, 1);
    object_parts[3] = mulib_parsec_named(mulib_parsec_byte('}'), "closing object brace");
    object_body = mulib_parsec_sequence(object_parts, 4); object = mulib_parsec_capture(&object_body, MULIB_JSON_OBJECT);
    value_alts[0] = object; value_alts[1] = array; value_alts[2] = string; value_alts[3] = number;
    value_alts[4] = mulib_parsec_capture(&true_lit, MULIB_JSON_BOOL);
    value_alts[5] = mulib_parsec_capture(&false_lit, MULIB_JSON_BOOL);
    value_alts[6] = mulib_parsec_capture(&null_lit, MULIB_JSON_NULL);
    value = mulib_parsec_choice(value_alts, 7); value_target = &value;
    root_parts[0] = ws; root_parts[1] = mulib_parsec_ref(&value_target); root_parts[2] = ws; root_parts[3] = mulib_parsec_end();
    root = mulib_parsec_sequence(root_parts, 4);
    /* A nested value adds at most ten evaluator frames (an object's later
     * member is the longest path); lexical productions fit in the spare 32.
     * Do not grant malformed input an unnecessarily large C-stack budget. */
    options.max_depth = depth > (SIZE_MAX - 32) / 10 ? SIZE_MAX : depth * 10 + 32;
    options.max_steps = length > (SIZE_MAX - 2048) / 128 ? SIZE_MAX : length * 128 + 2048;
    options.max_captures = length + 1;
    if (mulib_parsec_run(&root, source, length, &options, &result) != 1) {
        mulib_json_error_(error, source, result.error.offset, result.error.expected);
        mulib_parsec_result_free(&result); return NULL;
    }
    answer = mulib_json_build_values_(source, &result, error);
    mulib_parsec_result_free(&result); return answer;
}
static int mulib_json_quote_(mulib_strbuf_t *b, const char *s, size_t n) {
    static const char hex[] = "0123456789abcdef";
    size_t i;
    if (!mulib_strbuf_putc(b, '"')) return 0;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i]; const char *escape = NULL;
        switch (c) {
        case '"': escape = "\\\""; break;
        case '\\': escape = "\\\\"; break;
        case '\b': escape = "\\b"; break;
        case '\f': escape = "\\f"; break;
        case '\n': escape = "\\n"; break;
        case '\r': escape = "\\r"; break;
        case '\t': escape = "\\t"; break;
        default: break;
        }
        if (escape) { if (!mulib_strbuf_append(b, escape)) return 0; }
        else if (c < 32) {
            char code[6] = { '\\', 'u', '0', '0', hex[c >> 4], hex[c & 15] };
            if (!mulib_strbuf_append_n(b, code, sizeof code)) return 0;
        } else if (!mulib_strbuf_putc(b, (char)c)) return 0;
    }
    return mulib_strbuf_putc(b, '"');
}
static int mulib_json_indent_(mulib_strbuf_t *b, size_t depth) {
    size_t i;
    if (!mulib_strbuf_putc(b, '\n')) return 0;
    for (i = 0; i < depth; ++i) if (!mulib_strbuf_append(b, "  ")) return 0;
    return 1;
}
static int mulib_json_write_number_(mulib_strbuf_t *b, double d) {
    char *text, *point; int n; size_t decimal_length;
    struct lconv *lc;
    const char *decimal;
    n = snprintf(NULL, 0, "%.17g", d);
    if (n < 0 || (size_t)n == SIZE_MAX) return 0;
    text = (char *)malloc((size_t)n + 1);
    if (!text) return 0;
    if (snprintf(text, (size_t)n + 1, "%.17g", d) != n) { free(text); return 0; }
    lc = localeconv(); decimal = lc && lc->decimal_point && *lc->decimal_point ? lc->decimal_point : ".";
    point = strstr(text, decimal); decimal_length = strlen(decimal);
    if (point && strcmp(decimal, ".")) {
        size_t prefix = (size_t)(point - text);
        int ok = mulib_strbuf_append_n(b, text, prefix) && mulib_strbuf_putc(b, '.') &&
                 mulib_strbuf_append(b, point + decimal_length);
        free(text); return ok;
    }
    n = mulib_strbuf_append(b, text); free(text); return n;
}
static int mulib_json_write_(mulib_strbuf_t *b, const mulib_json_t *v, int pretty, size_t depth) {
    size_t i; int object;
    if (!v || v->type == MULIB_JSON_NULL) return mulib_strbuf_append(b, "null");
    if (v->type == MULIB_JSON_BOOL) return mulib_strbuf_append(b, v->as.boolean ? "true" : "false");
    if (v->type == MULIB_JSON_NUMBER) return mulib_json_write_number_(b, v->as.number);
    if (v->type == MULIB_JSON_STRING) return mulib_json_quote_(b, v->as.string.bytes, v->as.string.length);
    if (depth >= (size_t)MULIB_JSON_MAX_DEPTH) return 0;
    object = v->type == MULIB_JSON_OBJECT;
    if (!mulib_strbuf_putc(b, object ? '{' : '[')) return 0;
    for (i = 0; i < v->as.list.length; ++i) {
        const mulib_json_entry_ *e = &v->as.list.items[i];
        if (i && !mulib_strbuf_putc(b, ',')) return 0;
        if (pretty && !mulib_json_indent_(b, depth + 1)) return 0;
        if (object && (!mulib_json_quote_(b, e->key, e->key_length) || !mulib_strbuf_append(b, pretty ? ": " : ":"))) return 0;
        if (!mulib_json_write_(b, e->value, pretty, depth + 1)) return 0;
    }
    if (pretty && v->as.list.length && !mulib_json_indent_(b, depth)) return 0;
    return mulib_strbuf_putc(b, object ? '}' : ']');
}
char *mulib_json_stringify(const mulib_json_t *v, int pretty) {
    mulib_strbuf_t b;
    mulib_strbuf_init(&b);
    if (!mulib_json_write_(&b, v, pretty, 0)) { mulib_strbuf_free(&b); return NULL; }
    return b.data;
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
