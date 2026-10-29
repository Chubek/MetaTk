/**
 * @file mulib-pybind.h
 * @brief Generate Python ctypes source for explicitly described C functions.
 *
 * This is a source generator, not a Python embedding API. No Python headers,
 * interpreter, or runtime library are needed to build or run the generator.
 * Define MULIB_IMPL_PYBIND and MULIB_IMPL_STRBUF in one translation unit.
 */
#ifndef MULIB_PYBIND_H
#define MULIB_PYBIND_H
#include <stddef.h>
#include "mulib-strbuf.h"
#ifdef __cplusplus
extern "C" {
#endif

/** @brief Native C ABI scalar types shared with mulib-rubybind.h. */
typedef enum mulib_bind_type {
    MULIB_BIND_VOID, MULIB_BIND_SCHAR, MULIB_BIND_UCHAR,
    MULIB_BIND_SHORT, MULIB_BIND_USHORT, MULIB_BIND_INT, MULIB_BIND_UINT,
    MULIB_BIND_LONG, MULIB_BIND_ULONG, MULIB_BIND_LLONG, MULIB_BIND_ULLONG,
    MULIB_BIND_FLOAT, MULIB_BIND_DOUBLE, MULIB_BIND_POINTER, MULIB_BIND_CSTRING
} mulib_bind_type_t;

/** @brief A borrowed signature; symbol is native bytes, or NULL to use name. */
typedef struct mulib_bind_function {
    const char *name;
    const char *symbol;
    mulib_bind_type_t result;
    const mulib_bind_type_t *arguments;
    size_t argument_count;
} mulib_bind_function_t;

/** @brief Generator errors; no partial source is returned on failure. */
typedef enum mulib_bind_status {
    MULIB_BIND_OK, MULIB_BIND_INVALID_ARGUMENT, MULIB_BIND_INVALID_NAME,
    MULIB_BIND_INVALID_TYPE, MULIB_BIND_DUPLICATE_NAME, MULIB_BIND_LIMIT,
    MULIB_BIND_NO_MEMORY
} mulib_bind_status_t;

/** @brief Error location; SIZE_MAX means no particular function/argument. */
typedef struct mulib_bind_error {
    mulib_bind_status_t status;
    size_t function_index, argument_index;
} mulib_bind_error_t;

/** @brief Hard bounds on descriptor counts and NUL-terminated input strings. */
#define MULIB_BIND_MAX_FUNCTIONS 1024u
#define MULIB_BIND_MAX_ARGUMENTS 64u
#define MULIB_BIND_MAX_NAME 128u
#define MULIB_BIND_MAX_TEXT 4096u

/** @brief Validate common descriptors, including duplicate names and ABI types. */
mulib_bind_status_t mulib_bind_validate(const mulib_bind_function_t *functions,
                                      size_t count, mulib_bind_error_t *error);
/** @brief Return a static description for a generator status. */
const char *mulib_bind_status_string(mulib_bind_status_t status);
/**
 * @brief Emit an ASCII-only Python module with callable, typed ctypes functions.
 * @param library Nonempty native-encoding library path/name; copied and escaped.
 * @param source Receives malloc-owned source (free it); reset to NULL on failure.
 * @param length Optional source byte length, excluding NUL; reset on failure.
 * @details Only native, nonvariadic C calling convention functions are supported.
 * Native symbols are emitted as exact byte strings, without text decoding.
 * CSTRING is borrowed NUL-terminated char*: Python uses bytes/None, and ctypes
 * copies string results. POINTER is opaque; no pointer ownership is inferred.
 */
mulib_bind_status_t mulib_pybind_generate(const char *library,
    const mulib_bind_function_t *functions, size_t count,
    char **source, size_t *length, mulib_bind_error_t *error);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PYBIND_H */

#if defined(MULIB_IMPL_PYBIND) && !defined(MULIB_PYBIND_IMPLEMENTED)
#define MULIB_PYBIND_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

static mulib_bind_status_t mulib_pybind__error(mulib_bind_error_t *e,
    mulib_bind_status_t s, size_t f, size_t a) {
    if (e) { e->status = s; e->function_index = f; e->argument_index = a; }
    return s;
}

static int mulib_pybind__letter(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int mulib_pybind__text(const char *s, size_t limit) {
    size_t i;
    if (!s || !s[0]) return 0;
    for (i = 0; i <= limit; ++i) if (!s[i]) return 1;
    return 0;
}

static int mulib_pybind__name(const char *s) {
    size_t i;
    if (!mulib_pybind__text(s, MULIB_BIND_MAX_NAME) ||
        !mulib_pybind__letter((unsigned char)s[0]) ||
        !strncmp(s, "_mulib_", 7)) return 0;
    for (i = 1; s[i]; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (!mulib_pybind__letter(c) && !(c >= '0' && c <= '9')) return 0;
    }
    return 1;
}

mulib_bind_status_t mulib_bind_validate(const mulib_bind_function_t *f,
    size_t count, mulib_bind_error_t *e) {
    size_t i, j;
    mulib_pybind__error(e, MULIB_BIND_OK, SIZE_MAX, SIZE_MAX);
    if ((!f && count))
        return mulib_pybind__error(e, MULIB_BIND_INVALID_ARGUMENT, SIZE_MAX, SIZE_MAX);
    if (count > MULIB_BIND_MAX_FUNCTIONS)
        return mulib_pybind__error(e, MULIB_BIND_LIMIT, SIZE_MAX, SIZE_MAX);
    for (i = 0; i < count; ++i) {
        if (!mulib_pybind__name(f[i].name))
            return mulib_pybind__error(e, MULIB_BIND_INVALID_NAME, i, SIZE_MAX);
        if (f[i].symbol && !mulib_pybind__text(f[i].symbol, MULIB_BIND_MAX_TEXT))
            return mulib_pybind__error(e, MULIB_BIND_INVALID_ARGUMENT, i, SIZE_MAX);
        if ((int)f[i].result < (int)MULIB_BIND_VOID || f[i].result > MULIB_BIND_CSTRING)
            return mulib_pybind__error(e, MULIB_BIND_INVALID_TYPE, i, SIZE_MAX);
        if (f[i].argument_count > MULIB_BIND_MAX_ARGUMENTS)
            return mulib_pybind__error(e, MULIB_BIND_LIMIT, i, SIZE_MAX);
        if (!f[i].arguments && f[i].argument_count)
            return mulib_pybind__error(e, MULIB_BIND_INVALID_ARGUMENT, i, SIZE_MAX);
        for (j = 0; j < f[i].argument_count; ++j)
            if (f[i].arguments[j] <= MULIB_BIND_VOID || f[i].arguments[j] > MULIB_BIND_CSTRING)
                return mulib_pybind__error(e, MULIB_BIND_INVALID_TYPE, i, j);
        for (j = 0; j < i; ++j)
            if (!strcmp(f[i].name, f[j].name))
                return mulib_pybind__error(e, MULIB_BIND_DUPLICATE_NAME, i, SIZE_MAX);
    }
    return MULIB_BIND_OK;
}

const char *mulib_bind_status_string(mulib_bind_status_t s) {
    switch (s) {
    case MULIB_BIND_OK: return "success";
    case MULIB_BIND_INVALID_ARGUMENT: return "invalid argument or text";
    case MULIB_BIND_INVALID_NAME: return "invalid or reserved identifier";
    case MULIB_BIND_INVALID_TYPE: return "invalid C ABI type";
    case MULIB_BIND_DUPLICATE_NAME: return "duplicate exported name";
    case MULIB_BIND_LIMIT: return "descriptor limit exceeded";
    case MULIB_BIND_NO_MEMORY: return "allocation failed";
    default: return "unknown binding status";
    }
}

static int mulib_pybind__keyword(const char *name) {
    static const char *const words[] = {
        "False", "None", "True", "and", "as", "assert", "async", "await", "break",
        "class", "continue", "def", "del", "elif", "else", "except", "finally",
        "for", "from", "global", "if", "import", "in", "is", "lambda", "nonlocal",
        "not", "or", "pass", "raise", "return", "try", "while", "with", "yield"
    };
    size_t i;
    for (i = 0; i < sizeof(words) / sizeof(words[0]); ++i)
        if (!strcmp(name, words[i])) return 1;
    /* Do not let assignments replace Python's module bookkeeping. */
    return name[0] == '_' && name[1] == '_';
}

static int mulib_pybind__bytes(mulib_strbuf_t *b, const char *s) {
    static const char hex[] = "0123456789abcdef";
    const unsigned char *p = (const unsigned char *)s;
    if (!mulib_strbuf_append(b, "b'")) return 0;
    while (*p) {
        char escape[4] = { '\\', 'x', hex[*p >> 4], hex[*p & 15] };
        if (!mulib_strbuf_append_n(b, escape, sizeof(escape))) return 0;
        ++p;
    }
    return mulib_strbuf_putc(b, '\'');
}

mulib_bind_status_t mulib_pybind_generate(const char *library,
    const mulib_bind_function_t *f, size_t count, char **source,
    size_t *length, mulib_bind_error_t *e) {
    static const char *const types[] = {
        "None", "_mulib_ctypes.c_byte", "_mulib_ctypes.c_ubyte",
        "_mulib_ctypes.c_short", "_mulib_ctypes.c_ushort", "_mulib_ctypes.c_int",
        "_mulib_ctypes.c_uint", "_mulib_ctypes.c_long", "_mulib_ctypes.c_ulong",
        "_mulib_ctypes.c_longlong", "_mulib_ctypes.c_ulonglong",
        "_mulib_ctypes.c_float", "_mulib_ctypes.c_double", "_mulib_ctypes.c_void_p",
        "_mulib_ctypes.c_char_p"
    };
    mulib_strbuf_t b;
    mulib_bind_status_t status;
    size_t i, j;
    if (source) *source = NULL;
    if (length) *length = 0;
    if (!source || !mulib_pybind__text(library, MULIB_BIND_MAX_TEXT))
        return mulib_pybind__error(e, MULIB_BIND_INVALID_ARGUMENT, SIZE_MAX, SIZE_MAX);
    status = mulib_bind_validate(f, count, e);
    if (status != MULIB_BIND_OK) return status;
    for (i = 0; i < count; ++i)
        if (mulib_pybind__keyword(f[i].name))
            return mulib_pybind__error(e, MULIB_BIND_INVALID_NAME, i, SIZE_MAX);
    mulib_strbuf_init(&b);
    if (!mulib_strbuf_append(&b, "# Generated by MuLIB; native C ABI, no ownership transfer.\n"
        "import ctypes as _mulib_ctypes\nimport os as _mulib_os\n"
        "_mulib_library = _mulib_ctypes.CDLL(_mulib_os.fsdecode(") ||
        !mulib_pybind__bytes(&b, library) || !mulib_strbuf_append(&b, "))\n")) goto nomem;
    for (i = 0; i < count; ++i) {
        if (!mulib_strbuf_append(&b, f[i].name) ||
            !mulib_strbuf_append(&b, " = _mulib_library[") ||
            !mulib_pybind__bytes(&b, f[i].symbol ? f[i].symbol : f[i].name) ||
            !mulib_strbuf_append(&b, "]\n") ||
            !mulib_strbuf_append(&b, f[i].name) || !mulib_strbuf_append(&b, ".restype = ") ||
            !mulib_strbuf_append(&b, types[f[i].result]) || !mulib_strbuf_putc(&b, '\n') ||
            !mulib_strbuf_append(&b, f[i].name) || !mulib_strbuf_append(&b, ".argtypes = [")) goto nomem;
        for (j = 0; j < f[i].argument_count; ++j) {
            if ((j && !mulib_strbuf_append(&b, ", ")) ||
                !mulib_strbuf_append(&b, types[f[i].arguments[j]])) goto nomem;
        }
        if (!mulib_strbuf_append(&b, "]\n")) goto nomem;
    }
    *source = b.data;
    if (length) *length = b.length;
    return MULIB_BIND_OK;
nomem:
    mulib_strbuf_free(&b);
    return mulib_pybind__error(e, MULIB_BIND_NO_MEMORY, SIZE_MAX, SIZE_MAX);
}
#ifdef __cplusplus
}
#endif
#endif /* implementation */
