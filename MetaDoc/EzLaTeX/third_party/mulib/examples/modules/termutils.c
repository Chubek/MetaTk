#define MULIB_IMPL_TERMUTILS
#include "mulib-termutils.h"
#include <assert.h>
int main(void) {
    mulib_termutils_raw_t raw = MULIB_TERMUTILS_RAW_INIT;
    /* For a real terminal: raw_begin(&raw, fd), process input, raw_end(&raw).
       Always restore before closing fd. This example does not alter the console. */
    assert(mulib_termutils_raw_end(&raw) == MULIB_TERMUTILS_OK);
    assert(mulib_termutils_write(-1,"",0,NULL) == MULIB_TERMUTILS_EINVAL);
    return 0;
}
