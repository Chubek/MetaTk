#define MULIB_IMPL_MININCAT
#include "mulib-minincat.h"
#include <assert.h>
#include <string.h>
typedef struct input { const unsigned char *bytes; size_t length, offset; } input;
typedef struct output { unsigned char bytes[8]; size_t length; } output;
static int read_bytes(void *user, void *bytes, size_t capacity, size_t *count) {
    input *in = (input *)user; *count = in->length - in->offset;
    if (*count > capacity) *count = capacity;
    memcpy(bytes, in->bytes + in->offset, *count); in->offset += *count; return 1;
}
static int write_bytes(void *user, const void *bytes, size_t length, size_t *count) {
    output *out = (output *)user; assert(length && out->length < sizeof(out->bytes));
    /* Demonstrate a short write. */
    out->bytes[out->length++] = *(const unsigned char *)bytes; *count = 1; return 1;
}
int main(void) {
    input in = {(const unsigned char *)"a\0b", 3, 0}; output out = {{0}, 0};
    unsigned char scratch[2]; size_t transferred = 0;
    assert(mulib_minincat_relay(read_bytes, &in, write_bytes, &out, scratch,
        sizeof(scratch), 8, &transferred) == MULIB_MININCAT_OK);
    assert(transferred == 3 && !memcmp(out.bytes, in.bytes, 3)); return 0;
}
