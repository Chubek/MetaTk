#define MULIB_IMPL_JIT
#define MULIB_IMPL_DYNCALL
#include "mulib-jit.h"
#include <assert.h>

int main(void)
{
    /* AMD64: mov eax,42; ret. The host gate is checked before allocation. */
    static const unsigned char code[] = {0xb8, 42, 0, 0, 0, 0xc3};
    mulib_jit_t *jit = NULL;
    int64_t result;
    if (!mulib_jit_supported()) {
        assert(mulib_jit_create(&jit, sizeof(code)) == MULIB_JIT_ENOTSUP);
        return 0;
    }
    assert(mulib_jit_create(&jit, sizeof(code)) == 0);
    assert(mulib_jit_write(jit, 0, code, sizeof(code)) == 0);
    assert(mulib_jit_seal(jit) == 0);
    assert(mulib_jit_write(jit, 0, code, 1) == MULIB_JIT_ESTATE);
    assert(mulib_jit_run_i64_0(jit, &result) == 0 && result == 42);
    assert(mulib_jit_destroy(jit) == 0);
    return 0;
}
