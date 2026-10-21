# dbuf.q — d C API primitives.
@module dbuf
@version 1.0
@param PREFIX dbuf
@include <stddef.h>
@include <stdint.h>
@include <stdlib.h>
@include <string.h>
@guard

@raw {
/* copy requires an empty output; failure leaves it unchanged.
 * Buffers are allocated/freed by this C allocator, never a language GC. */
typedef struct { unsigned char *data; size_t size; } ${PREFIX}_t;
typedef struct { const unsigned char *data; size_t size; } ${PREFIX}_view;
}

@fn copy {
static inline int ${PREFIX}_copy(${PREFIX}_t *out, const void *data, size_t size) {
    if (!out || out->data || out->size || (size && !data)) return -1;
    unsigned char *copy = size ? (unsigned char *)malloc(size) : NULL;
    if (size && !copy) return -2;
    if (size) memcpy(copy, data, size);
    out->data = copy; out->size = size; return 0;
}
}

@fn free {
static inline void ${PREFIX}_free(${PREFIX}_t *buf) {
    if (buf) { free(buf->data); buf->data = NULL; buf->size = 0; }
}
}

@fn slice {
static inline int ${PREFIX}_slice(const ${PREFIX}_t *buf, size_t offset, size_t size, ${PREFIX}_view *out) {
    if (!buf || !out || offset > buf->size || size > buf->size - offset || (buf->size && !buf->data)) return -1;
    out->data = buf->data ? buf->data + offset : NULL; out->size = size; return 0;
}
}

