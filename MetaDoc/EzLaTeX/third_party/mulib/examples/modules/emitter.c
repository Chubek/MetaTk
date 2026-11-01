#define MULIB_IMPL_EMITTER
#include "mulib-emitter.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    unsigned char bytes[8];
    mulib_emitter_t emitter;
    size_t offset;
    assert(mulib_emitter_init(&emitter, bytes, sizeof(bytes), MULIB_EMITTER_BIG) == 0);
    assert(mulib_emitter_uint(&emitter, 0x1234, 2) == 0);
    assert(mulib_emitter_reserve(&emitter, 4, &offset) == 0 && offset == 2);
    assert(mulib_emitter_patch_uint(&emitter, offset, 0x89abcdef, 4) == 0);
    assert(!memcmp(bytes, "\x12\x34\x89\xab\xcd\xef", 6));
    assert(mulib_emitter_uint(&emitter, 256, 1) == MULIB_EMITTER_ERANGE);
    assert(emitter.size == 6);
    return 0;
}
