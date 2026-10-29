#include <assert.h>
#include <string.h>
#define MULIB_IMPL_VEC
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_TXTDIFF
#include "mulib-txtdiff.h"
int main(void) {
    const char old_text[] = "alpha\nbeta\nlast";
    const char new_text[] = "alpha\nnew\nbeta\nlast\n";
    mulib_txtdiff_patch_t patch; mulib_buffer_t result;
    mulib_txtdiff_init(&patch); mulib_buffer_init(&result);
    assert(mulib_txtdiff_make(old_text, sizeof(old_text) - 1, new_text,
        sizeof(new_text) - 1, NULL, &patch) == MULIB_TXTDIFF_OK);
    assert(mulib_txtdiff_apply(&patch, old_text, sizeof(old_text) - 1,
        1024, &result) == MULIB_TXTDIFF_OK);
    assert(result.length == sizeof(new_text) - 1 && !memcmp(result.data, new_text, result.length));
    mulib_txtdiff_free(&patch); mulib_buffer_free(&result); return 0;
}
