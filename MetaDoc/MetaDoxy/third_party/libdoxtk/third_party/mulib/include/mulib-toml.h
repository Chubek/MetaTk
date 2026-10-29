/**
 * @file mulib-toml.h
 * @brief TOML configuration tables and values backed by the owned JSON DOM.
 * This is not a complete TOML implementation: dates/times and non-finite or
 * inexact integers are rejected. See manual/mulib-toml.md. Define
 * MULIB_IMPL_TOML once, with JSON/STRBUF/UTF8 and JSON's PARSEC dependency.
 */
#ifndef MULIB_TOML_H
#define MULIB_TOML_H
#include <stddef.h>
#include "mulib-json.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Static message, byte offset, and one-based line/column. */
typedef struct mulib_toml_error {
    size_t offset, line, column;
    const char *message;
} mulib_toml_error_t;
/**
 * @brief Parse borrowed UTF-8 bytes to a caller-owned JSON object, or NULL.
 * Empty input produces an empty object. Duplicate/conflicting definitions and
 * unsupported syntax fail without returning a partial tree. error may be NULL.
 * Source NULs are rejected; escaped NULs in keys/strings are preserved.
 */
mulib_json_t *mulib_toml_parse(const char *source, size_t length, mulib_toml_error_t *error);
/**
 * @brief Serialize a borrowed JSON object to malloc-owned TOML, or NULL.
 * Nested objects use inline tables. Null values, non-object roots and nesting
 * beyond 128 fail with a diagnostic; input is never modified or consumed.
 */
char *mulib_toml_stringify(const mulib_json_t *object, mulib_toml_error_t *error);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TOML_H */

#if defined(MULIB_IMPL_TOML) && !defined(MULIB_TOML_IMPLEMENTED)
#define MULIB_TOML_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mulib-strbuf.h"
#include "mulib-utf8.h"
#ifdef __cplusplus
extern "C" {
#endif
enum mulib_toml_flag_ {
    MULIB_TOML_IMPLICIT_ = 1, MULIB_TOML_EXPLICIT_ = 2,
    MULIB_TOML_DOTTED_ = 3, MULIB_TOML_SEALED_ = 4, MULIB_TOML_AOT_ = 5
};
typedef struct mulib_toml_meta_ { mulib_json_t *node; int flag; } mulib_toml_meta_;
typedef struct mulib_toml_state_ {
    const char *source;
    size_t length, pos, count, capacity;
    mulib_toml_meta_ *meta;
    mulib_toml_error_t *error;
    int failed;
} mulib_toml_state_;
static int mulib_toml_fail_(mulib_toml_state_ *s, size_t at, const char *message)
{
    if (!s->failed && s->error) {
        size_t i;
        s->error->offset = at; s->error->line = s->error->column = 1;
        for (i = 0; i < at && i < s->length; ++i) {
            if (s->source[i] == '\n') { ++s->error->line; s->error->column = 1; }
            else ++s->error->column;
        }
        s->error->message = message;
    }
    s->failed = 1; return 0;
}
static int mulib_toml_hspace_(char c) { return c == ' ' || c == '\t'; }
static int mulib_toml_space_(char c) { return mulib_toml_hspace_(c) || c == '\n' || c == '\r'; }
static void mulib_toml_horizontal_(mulib_toml_state_ *s)
{ while (s->pos < s->length && mulib_toml_hspace_(s->source[s->pos])) ++s->pos; }
static void mulib_toml_whitespace_(mulib_toml_state_ *s)
{
    while (s->pos < s->length) {
        if (mulib_toml_space_(s->source[s->pos])) ++s->pos;
        else if (s->source[s->pos] == '#') {
            while (s->pos < s->length && s->source[s->pos] != '\n') ++s->pos;
        } else break;
    }
}
static int mulib_toml_line_end_(mulib_toml_state_ *s)
{
    mulib_toml_horizontal_(s);
    if (s->pos < s->length && s->source[s->pos] == '#')
        while (s->pos < s->length && s->source[s->pos] != '\n') ++s->pos;
    if (s->pos == s->length) return 1;
    if (s->source[s->pos] == '\r') ++s->pos;
    if (s->pos < s->length && s->source[s->pos] == '\n') { ++s->pos; return 1; }
    return mulib_toml_fail_(s, s->pos, "unexpected text after a TOML statement");
}
static int mulib_toml_flag_(mulib_toml_state_ *s, mulib_json_t *node)
{
    size_t i;
    for (i = 0; i < s->count; ++i) if (s->meta[i].node == node) return s->meta[i].flag;
    return 0;
}
static int mulib_toml_mark_(mulib_toml_state_ *s, mulib_json_t *node, int flag)
{
    size_t i;
    for (i = 0; i < s->count; ++i) if (s->meta[i].node == node) { s->meta[i].flag = flag; return 1; }
    if (s->count == s->capacity) {
        size_t cap = s->capacity ? s->capacity * 2 : 32;
        mulib_toml_meta_ *p;
        if (cap < s->capacity || cap > SIZE_MAX / sizeof(*p))
            return mulib_toml_fail_(s, s->pos, "document is too large");
        p = (mulib_toml_meta_ *)realloc(s->meta, cap * sizeof(*p));
        if (!p) return mulib_toml_fail_(s, s->pos, "out of memory");
        s->meta = p; s->capacity = cap;
    }
    s->meta[s->count].node = node; s->meta[s->count++].flag = flag;
    return 1;
}
static int mulib_toml_hex_(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static mulib_json_t *mulib_toml_string_(mulib_toml_state_ *s, int key)
{
    size_t at = s->pos;
    char quote = s->source[s->pos++];
    int multiline = s->length - s->pos >= 2 && s->source[s->pos] == quote && s->source[s->pos + 1] == quote;
    int closed = 0;
    mulib_strbuf_t b;
    mulib_json_t *result = NULL;
    mulib_strbuf_init(&b);
    if (multiline) {
        if (key) { mulib_toml_fail_(s, at, "multiline strings are not valid keys"); goto done; }
        s->pos += 2;
        if (s->pos < s->length && s->source[s->pos] == '\r') ++s->pos;
        if (s->pos < s->length && s->source[s->pos] == '\n') ++s->pos;
    }
    while (s->pos < s->length) {
        unsigned char c = (unsigned char)s->source[s->pos++];
        if (c == (unsigned char)quote) {
            if (!multiline) { closed = 1; break; }
            else {
                size_t count = 1;
                while (s->pos < s->length && s->source[s->pos] == quote) { ++count; ++s->pos; }
                if (count > 5) { mulib_toml_fail_(s, s->pos - count, "too many closing string quotes"); goto done; }
                if (count >= 3) {
                    count -= 3;
                    while (count--) if (!mulib_strbuf_putc(&b, quote)) goto memory;
                    closed = 1; break;
                }
                while (count--) if (!mulib_strbuf_putc(&b, quote)) goto memory;
                continue;
            }
        }
        if ((c == '\r' || c == '\n') && !multiline) {
            mulib_toml_fail_(s, s->pos - 1, "newline in a single-line string"); goto done;
        }
        if (c == '\r') { ++s->pos; c = '\n'; }
        if (quote == '"' && c == '\\') {
            uint32_t code = 0;
            unsigned digits = 0, i;
            if (s->pos == s->length) { mulib_toml_fail_(s, s->pos, "truncated string escape"); goto done; }
            c = (unsigned char)s->source[s->pos++];
            if (multiline && mulib_toml_space_((char)c)) {
                int newline = c == '\n' || c == '\r';
                while (s->pos < s->length && mulib_toml_space_(s->source[s->pos])) {
                    if (s->source[s->pos] == '\n') newline = 1;
                    ++s->pos;
                }
                if (!newline) { mulib_toml_fail_(s, s->pos, "line continuation needs a newline"); goto done; }
                continue;
            }
            switch (c) {
            case 'b': c = 8; break;
            case 't': c = 9; break;
            case 'n': c = 10; break;
            case 'f': c = 12; break;
            case 'r': c = 13; break;
            case '"': case '\\': break;
            case 'u': digits = 4; break;
            case 'U': digits = 8; break;
            default: mulib_toml_fail_(s, s->pos - 1, "unknown TOML string escape"); goto done;
            }
            if (digits) {
                unsigned char encoded[4];
                size_t n;
                if (s->length - s->pos < digits) { mulib_toml_fail_(s, s->pos, "truncated Unicode escape"); goto done; }
                for (i = 0; i < digits; ++i) {
                    int h = mulib_toml_hex_(s->source[s->pos++]);
                    if (h < 0) { mulib_toml_fail_(s, s->pos - 1, "invalid Unicode escape"); goto done; }
                    code = (code << 4) | (uint32_t)h;
                }
                n = mulib_utf8_encode(code, encoded, sizeof(encoded));
                if (!n) { mulib_toml_fail_(s, s->pos, "invalid Unicode scalar"); goto done; }
                if (!mulib_strbuf_append_n(&b, (const char *)encoded, n)) goto memory;
                continue;
            }
        }
        if (!mulib_strbuf_putc(&b, (char)c)) goto memory;
    }
    if (!closed) { mulib_toml_fail_(s, at, "unclosed TOML string"); goto done; }
    result = mulib_json_string_n(b.data ? b.data : "", b.length);
    if (!result) goto memory;
    goto done;
memory:
    mulib_toml_fail_(s, at, "out of memory");
done:
    mulib_strbuf_free(&b); return result;
}
static int mulib_toml_bare_(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-';
}
static mulib_json_t *mulib_toml_path_(mulib_toml_state_ *s)
{
    mulib_json_t *path = mulib_json_new(MULIB_JSON_ARRAY);
    if (!path) { mulib_toml_fail_(s, s->pos, "out of memory"); return NULL; }
    for (;;) {
        mulib_json_t *key;
        size_t start;
        mulib_toml_horizontal_(s); start = s->pos;
        if (s->pos < s->length && (s->source[s->pos] == '\'' || s->source[s->pos] == '"')) key = mulib_toml_string_(s, 1);
        else {
            while (s->pos < s->length && mulib_toml_bare_(s->source[s->pos])) ++s->pos;
            if (s->pos == start) { mulib_toml_fail_(s, start, "expected a quoted or bare key"); goto fail; }
            key = mulib_json_string_n(s->source + start, s->pos - start);
        }
        if (!key || !mulib_json_append(path, key)) {
            mulib_json_destroy(key);
            if (!s->failed) mulib_toml_fail_(s, start, "out of memory");
            goto fail;
        }
        if (mulib_json_size(path) > 128) { mulib_toml_fail_(s, start, "key path exceeds 128 components"); goto fail; }
        mulib_toml_horizontal_(s);
        if (s->pos == s->length || s->source[s->pos] != '.') return path;
        ++s->pos;
    }
fail:
    mulib_json_destroy(path); return NULL;
}
static mulib_json_t *mulib_toml_child_(mulib_toml_state_ *s, mulib_json_t *parent,
                                     const char *key, size_t n, int flag, int arrays)
{
    mulib_json_t *child = mulib_json_get_n(parent, key, n);
    if (!child) {
        child = mulib_json_new(MULIB_JSON_OBJECT);
        if (!child || !mulib_toml_mark_(s, child, flag) || !mulib_json_set_n(parent, key, n, child)) {
            mulib_json_destroy(child);
            if (!s->failed) mulib_toml_fail_(s, s->pos, "out of memory");
            return NULL;
        }
    }
    if (arrays && mulib_toml_flag_(s, child) == MULIB_TOML_AOT_ && mulib_json_size(child))
        child = mulib_json_at(child, mulib_json_size(child) - 1);
    if (mulib_json_typeof(child) != MULIB_JSON_OBJECT || mulib_toml_flag_(s, child) == MULIB_TOML_SEALED_) {
        mulib_toml_fail_(s, s->pos, "key/table conflicts with a value or closed inline table"); return NULL;
    }
    if (flag == MULIB_TOML_DOTTED_) {
        int existing = mulib_toml_flag_(s, child);
        if (existing == MULIB_TOML_EXPLICIT_) {
            mulib_toml_fail_(s, s->pos, "dotted key cannot redefine a header-defined table"); return NULL;
        }
        if (existing == MULIB_TOML_IMPLICIT_ && !mulib_toml_mark_(s, child, MULIB_TOML_DOTTED_)) return NULL;
    }
    return child;
}
static int mulib_toml_set_path_(mulib_toml_state_ *s, mulib_json_t *base,
                                const mulib_json_t *path, mulib_json_t *value)
{
    size_t i, count = mulib_json_size(path), n;
    const char *key;
    for (i = 0; i + 1 < count; ++i) {
        key = mulib_json_text(mulib_json_at(path, i), &n);
        base = mulib_toml_child_(s, base, key, n, MULIB_TOML_DOTTED_, 0);
        if (!base) return 0;
    }
    key = mulib_json_text(mulib_json_at(path, count - 1), &n);
    if (mulib_json_get_n(base, key, n)) return mulib_toml_fail_(s, s->pos, "duplicate TOML key or table");
    if (!mulib_json_set_n(base, key, n, value)) return mulib_toml_fail_(s, s->pos, "out of memory");
    return 1;
}
static int mulib_toml_seal_(mulib_toml_state_ *s, mulib_json_t *value, unsigned depth)
{
    size_t i;
    mulib_json_type type = mulib_json_typeof(value);
    if (type != MULIB_JSON_OBJECT && type != MULIB_JSON_ARRAY) return 1;
    if (depth >= 128) return mulib_toml_fail_(s, s->pos, "TOML nesting exceeds 128");
    if (type == MULIB_JSON_OBJECT && !mulib_toml_mark_(s, value, MULIB_TOML_SEALED_)) return 0;
    for (i = 0; i < mulib_json_size(value); ++i)
        if (!mulib_toml_seal_(s, mulib_json_at(value, i), depth + 1)) return 0;
    return 1;
}
static int mulib_toml_digits_(const char *p, size_t n, size_t *pos, unsigned base)
{
    size_t start = *pos;
    int last_digit = 0;
    while (*pos < n) {
        int digit = mulib_toml_hex_(p[*pos]);
        if (digit >= 0 && (unsigned)digit < base) { ++*pos; last_digit = 1; }
        else if (p[*pos] == '_') {
            if (!last_digit || *pos + 1 == n) return 0;
            digit = mulib_toml_hex_(p[*pos + 1]);
            if (digit < 0 || (unsigned)digit >= base) return 0;
            ++*pos; last_digit = 0;
        } else break;
    }
    return *pos > start && last_digit;
}
static mulib_json_t *mulib_toml_number_(mulib_toml_state_ *s, size_t at, size_t n)
{
    const char *p = s->source + at;
    size_t i = 0, first = 0, integer_end, j;
    unsigned base = 10;
    int negative = 0, floating = 0;
    mulib_json_t *value;
    mulib_strbuf_t b;
    if (n && (p[i] == '+' || p[i] == '-')) { negative = p[i] == '-'; ++i; first = i; }
    if (n - i >= 2 && p[i] == '0' && (p[i + 1] == 'x' || p[i + 1] == 'o' || p[i + 1] == 'b')) {
        if (first) goto invalid;
        base = p[i + 1] == 'x' ? 16U : p[i + 1] == 'o' ? 8U : 2U;
        i += 2; first = i;
        if (!mulib_toml_digits_(p, n, &i, base) || i != n) goto invalid;
        integer_end = i;
    } else {
        if (!mulib_toml_digits_(p, n, &i, 10)) goto invalid;
        integer_end = i;
        if (i - first > 1 && p[first] == '0') goto invalid;
        if (i < n && p[i] == '.') {
            floating = 1; ++i;
            if (!mulib_toml_digits_(p, n, &i, 10)) goto invalid;
        }
        if (i < n && (p[i] == 'e' || p[i] == 'E')) {
            floating = 1; ++i;
            if (i < n && (p[i] == '+' || p[i] == '-')) ++i;
            if (!mulib_toml_digits_(p, n, &i, 10)) goto invalid;
        }
        if (i != n) goto invalid;
    }
    if (!floating) {
        uint64_t integer = 0;
        for (j = first; j < integer_end; ++j) {
            unsigned digit;
            if (p[j] == '_') continue;
            digit = (unsigned)mulib_toml_hex_(p[j]);
            if (integer > (UINT64_C(9007199254740991) - digit) / base) {
                mulib_toml_fail_(s, at, "integer exceeds exact JSON numeric range"); return NULL;
            }
            integer = integer * base + digit;
        }
        value = mulib_json_number(negative ? -(double)integer : (double)integer);
        if (!value) mulib_toml_fail_(s, at, "out of memory");
        return value;
    }
    mulib_strbuf_init(&b);
    for (j = 0; j < n; ++j) if (p[j] != '_' && !(j == 0 && p[j] == '+'))
        if (!mulib_strbuf_putc(&b, p[j])) {
            mulib_strbuf_free(&b); mulib_toml_fail_(s, at, "out of memory"); return NULL;
        }
    value = mulib_json_parse(b.data, b.length, NULL);
    mulib_strbuf_free(&b);
    if (!value) mulib_toml_fail_(s, at, "floating value is out of finite JSON range");
    return value;
invalid:
    mulib_toml_fail_(s, at, "invalid or unsupported TOML value (no dates/times, inf or nan)"); return NULL;
}
static mulib_json_t *mulib_toml_value_(mulib_toml_state_ *s, unsigned depth)
{
    size_t at;
    char c;
    mulib_json_t *container;
    mulib_toml_horizontal_(s); at = s->pos;
    if (at == s->length) { mulib_toml_fail_(s, at, "expected a TOML value"); return NULL; }
    c = s->source[s->pos];
    if (c == '\'' || c == '"') return mulib_toml_string_(s, 0);
    if (c == '[' || c == '{') {
        if (depth >= 128) { mulib_toml_fail_(s, at, "TOML nesting exceeds 128"); return NULL; }
        container = mulib_json_new(c == '[' ? MULIB_JSON_ARRAY : MULIB_JSON_OBJECT);
        if (!container) { mulib_toml_fail_(s, at, "out of memory"); return NULL; }
        ++s->pos;
        if (c == '[') mulib_toml_whitespace_(s); else mulib_toml_horizontal_(s);
        if (s->pos < s->length && s->source[s->pos] == (c == '[' ? ']' : '}')) {
            ++s->pos;
            if (c == '{' && !mulib_toml_seal_(s, container, depth)) goto fail;
            return container;
        }
        for (;;) {
            mulib_json_t *path = NULL, *value;
            if (c == '{') {
                path = mulib_toml_path_(s);
                if (!path) goto fail;
                if (s->pos == s->length || s->source[s->pos] != '=') {
                    mulib_json_destroy(path); mulib_toml_fail_(s, s->pos, "expected '=' in inline table"); goto fail;
                }
                ++s->pos;
            }
            value = mulib_toml_value_(s, depth + 1);
            if (!value) { mulib_json_destroy(path); goto fail; }
            if (!(c == '[' ? mulib_json_append(container, value) : mulib_toml_set_path_(s, container, path, value))) {
                mulib_json_destroy(path); mulib_json_destroy(value);
                if (!s->failed) mulib_toml_fail_(s, s->pos, "out of memory");
                goto fail;
            }
            mulib_json_destroy(path);
            if (c == '[') mulib_toml_whitespace_(s); else mulib_toml_horizontal_(s);
            if (s->pos < s->length && s->source[s->pos] == (c == '[' ? ']' : '}')) {
                ++s->pos;
                if (c == '{' && !mulib_toml_seal_(s, container, depth)) goto fail;
                return container;
            }
            if (s->pos == s->length || s->source[s->pos] != ',') {
                mulib_toml_fail_(s, s->pos, "unclosed collection or missing comma"); goto fail;
            }
            ++s->pos;
            if (c == '[') mulib_toml_whitespace_(s); else mulib_toml_horizontal_(s);
            if (c == '[' && s->pos < s->length && s->source[s->pos] == ']') { ++s->pos; return container; }
        }
fail:
        mulib_json_destroy(container); return NULL;
    }
    while (s->pos < s->length && !mulib_toml_space_(s->source[s->pos]) &&
           !strchr(",]}#", s->source[s->pos])) ++s->pos;
    if (s->pos - at == 4 && memcmp(s->source + at, "true", 4) == 0) container = mulib_json_bool(1);
    else if (s->pos - at == 5 && memcmp(s->source + at, "false", 5) == 0) container = mulib_json_bool(0);
    else return mulib_toml_number_(s, at, s->pos - at);
    if (!container) mulib_toml_fail_(s, at, "out of memory");
    return container;
}
static mulib_json_t *mulib_toml_header_(mulib_toml_state_ *s, mulib_json_t *root)
{
    int array;
    mulib_json_t *path, *table = root, *child;
    size_t i, count, n;
    const char *key;
    ++s->pos; array = s->pos < s->length && s->source[s->pos] == '[';
    if (array) ++s->pos;
    path = mulib_toml_path_(s);
    if (!path) return NULL;
    if (s->pos == s->length || s->source[s->pos++] != ']' ||
        (array && (s->pos == s->length || s->source[s->pos++] != ']'))) {
        mulib_toml_fail_(s, s->pos, "unclosed table header"); goto fail;
    }
    if (!mulib_toml_line_end_(s)) goto fail;
    count = mulib_json_size(path);
    for (i = 0; i + 1 < count; ++i) {
        key = mulib_json_text(mulib_json_at(path, i), &n);
        table = mulib_toml_child_(s, table, key, n, MULIB_TOML_IMPLICIT_, 1);
        if (!table) goto fail;
    }
    key = mulib_json_text(mulib_json_at(path, count - 1), &n);
    child = mulib_json_get_n(table, key, n);
    if (array) {
        mulib_json_t *element;
        if (!child) {
            child = mulib_json_new(MULIB_JSON_ARRAY);
            if (!child || !mulib_toml_mark_(s, child, MULIB_TOML_AOT_) || !mulib_json_set_n(table, key, n, child)) {
                mulib_json_destroy(child);
                if (!s->failed) mulib_toml_fail_(s, s->pos, "out of memory");
                goto fail;
            }
        } else if (mulib_toml_flag_(s, child) != MULIB_TOML_AOT_) {
            mulib_toml_fail_(s, s->pos, "array-of-tables conflicts with an existing value/table"); goto fail;
        }
        element = mulib_json_new(MULIB_JSON_OBJECT);
        if (!element || !mulib_toml_mark_(s, element, MULIB_TOML_EXPLICIT_) || !mulib_json_append(child, element)) {
            mulib_json_destroy(element);
            if (!s->failed) mulib_toml_fail_(s, s->pos, "out of memory");
            goto fail;
        }
        table = element;
    } else {
        if (child) {
            if (mulib_json_typeof(child) != MULIB_JSON_OBJECT || mulib_toml_flag_(s, child) != MULIB_TOML_IMPLICIT_) {
                mulib_toml_fail_(s, s->pos, "duplicate/conflicting table definition"); goto fail;
            }
            if (!mulib_toml_mark_(s, child, MULIB_TOML_EXPLICIT_)) goto fail;
            table = child;
        } else {
            table = mulib_toml_child_(s, table, key, n, MULIB_TOML_EXPLICIT_, 0);
            if (!table) goto fail;
        }
    }
    mulib_json_destroy(path); return table;
fail:
    mulib_json_destroy(path); return NULL;
}
static int mulib_toml_depth_(mulib_toml_state_ *s, const mulib_json_t *value, unsigned depth)
{
    size_t i;
    mulib_json_type type = mulib_json_typeof(value);
    if (type != MULIB_JSON_ARRAY && type != MULIB_JSON_OBJECT) return 1;
    if (depth >= 128) return mulib_toml_fail_(s, s->pos, "TOML nesting exceeds 128");
    for (i = 0; i < mulib_json_size(value); ++i)
        if (!mulib_toml_depth_(s, mulib_json_at(value, i), depth + 1)) return 0;
    return 1;
}
mulib_json_t *mulib_toml_parse(const char *source, size_t length, mulib_toml_error_t *error)
{
    mulib_toml_state_ s;
    mulib_json_t *root, *current;
    size_t i;
    memset(&s, 0, sizeof(s));
    if (error) memset(error, 0, sizeof(*error));
    s.source = source ? source : ""; s.length = length; s.error = error;
    if ((!source && length) || !mulib_utf8_validate((const unsigned char *)s.source, length)) {
        mulib_toml_fail_(&s, 0, "invalid source or UTF-8"); return NULL;
    }
    for (i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)s.source[i];
        if ((c < 32 && c != '\t' && c != '\r' && c != '\n') || c == 127 ||
            (c == '\r' && (i + 1 == length || s.source[i + 1] != '\n'))) {
            mulib_toml_fail_(&s, i, "invalid control byte in TOML source"); return NULL;
        }
    }
    if (length >= 3 && memcmp(s.source, "\xef\xbb\xbf", 3) == 0) s.pos = 3;
    root = current = mulib_json_new(MULIB_JSON_OBJECT);
    if (!root || !mulib_toml_mark_(&s, root, MULIB_TOML_EXPLICIT_)) {
        if (!s.failed) mulib_toml_fail_(&s, 0, "out of memory");
        mulib_json_destroy(root); free(s.meta); return NULL;
    }
    for (;;) {
        mulib_json_t *path, *value;
        mulib_toml_whitespace_(&s);
        if (s.pos == s.length) break;
        if (s.source[s.pos] == '[') {
            current = mulib_toml_header_(&s, root);
            if (!current) goto fail;
            continue;
        }
        path = mulib_toml_path_(&s);
        if (!path) goto fail;
        if (s.pos == s.length || s.source[s.pos] != '=') {
            mulib_json_destroy(path); mulib_toml_fail_(&s, s.pos, "expected '=' after key"); goto fail;
        }
        ++s.pos;
        value = mulib_toml_value_(&s, 1);
        if (!value || !mulib_toml_set_path_(&s, current, path, value)) {
            mulib_json_destroy(path); mulib_json_destroy(value); goto fail;
        }
        mulib_json_destroy(path);
        if (!mulib_toml_line_end_(&s)) goto fail;
    }
    if (!mulib_toml_depth_(&s, root, 0)) goto fail;
    free(s.meta); return root;
fail:
    free(s.meta); mulib_json_destroy(root); return NULL;
}
static int mulib_toml_string_out_(mulib_strbuf_t *b, const char *text, size_t n)
{
    static const char hex[] = "0123456789abcdef";
    size_t i;
    if (!mulib_strbuf_putc(b, '"')) return 0;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)text[i];
        const char *escape = NULL;
        switch (c) {
        case '"': escape = "\\\""; break;
        case '\\': escape = "\\\\"; break;
        case '\b': escape = "\\b"; break;
        case '\t': escape = "\\t"; break;
        case '\n': escape = "\\n"; break;
        case '\f': escape = "\\f"; break;
        case '\r': escape = "\\r"; break;
        default: break;
        }
        if (escape) { if (!mulib_strbuf_append(b, escape)) return 0; }
        else if (c < 32 || c == 127) {
            char encoded[6] = { '\\', 'u', '0', '0', hex[c >> 4], hex[c & 15] };
            if (!mulib_strbuf_append_n(b, encoded, sizeof(encoded))) return 0;
        } else if (!mulib_strbuf_putc(b, (char)c)) return 0;
    }
    return mulib_strbuf_putc(b, '"');
}
static int mulib_toml_out_(mulib_toml_state_ *s, mulib_strbuf_t *b, const mulib_json_t *value, unsigned depth)
{
    mulib_json_type type = mulib_json_typeof(value);
    size_t i, n;
    const char *text;
    if (type == MULIB_JSON_NULL) return mulib_toml_fail_(s, 0, "TOML has no null value");
    if (type == MULIB_JSON_STRING) {
        text = mulib_json_text(value, &n); return mulib_toml_string_out_(b, text, n);
    }
    if (type == MULIB_JSON_BOOL) return mulib_strbuf_append(b, mulib_json_boolean(value) ? "true" : "false");
    if (type == MULIB_JSON_NUMBER) {
        char *number = mulib_json_stringify(value, 0);
        double d = mulib_json_numeric(value);
        int ok;
        if (!number) return 0;
        ok = mulib_strbuf_append(b, number);
        if (ok && !strchr(number, '.') && !strchr(number, 'e') && !strchr(number, 'E') &&
            (d > 9007199254740991.0 || d < -9007199254740991.0)) ok = mulib_strbuf_append(b, ".0");
        free(number); return ok;
    }
    if (depth >= 128) return mulib_toml_fail_(s, 0, "TOML nesting exceeds 128");
    if (!mulib_strbuf_putc(b, type == MULIB_JSON_ARRAY ? '[' : '{')) return 0;
    for (i = 0; i < mulib_json_size(value); ++i) {
        if (i && !mulib_strbuf_append(b, ", ")) return 0;
        if (type == MULIB_JSON_OBJECT) {
            text = mulib_json_key(value, i, &n);
            if (!mulib_toml_string_out_(b, text, n) || !mulib_strbuf_append(b, " = ")) return 0;
        }
        if (!mulib_toml_out_(s, b, mulib_json_at(value, i), depth + 1)) return 0;
    }
    return mulib_strbuf_putc(b, type == MULIB_JSON_ARRAY ? ']' : '}');
}
char *mulib_toml_stringify(const mulib_json_t *object, mulib_toml_error_t *error)
{
    mulib_toml_state_ s;
    mulib_strbuf_t b;
    size_t i;
    memset(&s, 0, sizeof(s)); s.source = ""; s.error = error;
    if (error) memset(error, 0, sizeof(*error));
    if (mulib_json_typeof(object) != MULIB_JSON_OBJECT) {
        mulib_toml_fail_(&s, 0, "TOML document root must be an object"); return NULL;
    }
    mulib_strbuf_init(&b);
    for (i = 0; i < mulib_json_size(object); ++i) {
        size_t n;
        const char *key = mulib_json_key(object, i, &n);
        if (!mulib_toml_string_out_(&b, key, n) || !mulib_strbuf_append(&b, " = ") ||
            !mulib_toml_out_(&s, &b, mulib_json_at(object, i), 1) || !mulib_strbuf_putc(&b, '\n')) goto fail;
    }
    if (!mulib_strbuf_append_n(&b, "", 0)) goto fail;
    return b.data;
fail:
    if (!s.failed) mulib_toml_fail_(&s, 0, "out of memory");
    mulib_strbuf_free(&b); return NULL;
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
