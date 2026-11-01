/* Source generation requires neither Ruby nor a native demo library. */
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PYBIND
#define MULIB_IMPL_RUBYBIND
#include "mulib-rubybind.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const mulib_bind_type_t args[] = { MULIB_BIND_INT, MULIB_BIND_INT };
    const mulib_bind_function_t functions[] = {
        { "add", "demo_add", MULIB_BIND_INT, args, 2 }
    };
    char *source = NULL;
    size_t length;
    assert(mulib_rubybind_generate("./libdemo.so", functions, 1,
                                  &source, &length, NULL) == MULIB_BIND_OK);
    assert(length == strlen(source));
    assert(strstr(source, "module MuLIBBindings"));
    assert(strstr(source, "def self.add(a0, a1)"));
    assert(fwrite(source, 1, length, stdout) == length);
    free(source);
    return 0;
}
