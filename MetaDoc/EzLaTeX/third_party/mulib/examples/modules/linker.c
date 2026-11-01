#define MULIB_IMPL_LINKER
#define MULIB_IMPL_EMITTER
#include "mulib-linker.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    unsigned char bytes[6];
    mulib_emitter_t emitter;
    mulib_linker_t *linker = mulib_linker_create(2, 2);
    assert(linker);
    assert(mulib_emitter_init(&emitter, bytes, sizeof(bytes), MULIB_EMITTER_LITTLE) == 0);
    assert(mulib_emitter_bytes(&emitter, "\xe9\0\0\0\0\xc3", 6) == 0);
    assert(mulib_linker_relocate(linker, "return", 1, MULIB_LINKER_PC32, -4) == 0);
    assert(mulib_linker_apply(linker, &emitter, 4096, NULL) == MULIB_LINKER_EUNDEFINED);
    assert(mulib_linker_define(linker, "return", 5, 0) == 0);
    assert(mulib_linker_apply(linker, &emitter, 4096, NULL) == 0);
    assert(!memcmp(bytes, "\xe9\0\0\0\0\xc3", 6));
    mulib_linker_destroy(linker);
    return 0;
}
