#include <assert.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_TERMINFO
#include "mulib-terminfo.h"

int main(void) {
    mulib_terminfo_t info;
    mulib_strbuf_t output;
    int32_t parameters[9] = {2, 4, 0, 0, 0, 0, 0, 0, 0};
    mulib_terminfo_init(&info);
    mulib_terminfo_ansi(&info); /* Explicit fallback, not a database query. */
    mulib_strbuf_init(&output);
    assert(mulib_terminfo_num(&info, "colors") == 256);
    assert(mulib_terminfo_emit(&info, "cup", parameters, &output));
    assert(!strcmp(output.data, "\033[3;5H"));
    assert(!mulib_terminfo_expand("%p1%s", parameters, &output));
    assert(!strcmp(output.data, "\033[3;5H"));
    mulib_strbuf_free(&output);
    mulib_terminfo_free(&info);
    return 0;
}
