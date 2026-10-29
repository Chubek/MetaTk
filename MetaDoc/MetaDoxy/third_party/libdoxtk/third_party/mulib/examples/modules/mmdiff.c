#include <assert.h>
#include <string.h>
#define MULIB_IMPL_VEC
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_MMDIFF
#include "mulib-mmdiff.h"
int main(void) {
    const unsigned char old_bytes[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    const unsigned char new_bytes[] = {8,9,10,11,12,13,14,15,255,0,1,2,3,4,5,6,7};
    mulib_mmdiff_patch_t patch; mulib_buffer_t result;
    mulib_mmdiff_init(&patch); mulib_buffer_init(&result);
    assert(mulib_mmdiff_make(old_bytes, sizeof(old_bytes), new_bytes,
        sizeof(new_bytes), NULL, &patch) == MULIB_MMDIFF_OK);
    assert(patch.commands.length == 3); /* COPY, INSERT, COPY */
    assert(mulib_mmdiff_apply(&patch, old_bytes, sizeof(old_bytes),
        1024, &result) == MULIB_MMDIFF_OK);
    assert(result.length == sizeof(new_bytes) && !memcmp(result.data, new_bytes, result.length));
    mulib_mmdiff_free(&patch); mulib_buffer_free(&result); return 0;
}
