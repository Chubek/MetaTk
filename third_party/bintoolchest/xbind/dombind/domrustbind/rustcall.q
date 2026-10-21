# rustcall.q — rust C API primitives.
@module rustcall
@version 1.0
@param PREFIX rustcall
@include <stddef.h>
@include <stdint.h>
@guard

@raw {
/* context is opaque and borrowed; caller coordinates lifetime and threads.
 * Callbacks must translate panics/exceptions to integer status codes. */
typedef int (*${PREFIX}_fn)(void *, const unsigned char *, size_t);
typedef struct { ${PREFIX}_fn fn; void *context; } ${PREFIX}_t;
}

@fn invoke {
static inline int ${PREFIX}_invoke(${PREFIX}_t call, const unsigned char *input, size_t size) {
    if (!call.fn || (size && !input)) return -1;
    return call.fn(call.context, input, size);
}
}

