/* Source generation requires neither Python nor a native demo library. */
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PYBIND
#include "mulib-pybind.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const mulib_bind_type_t args[] = { MULIB_BIND_INT, MULIB_BIND_INT };
    const mulib_bind_function_t functions[] = {
        { "add", "demo_add", MULIB_BIND_INT, args, 2 }
    };
    mulib_bind_error_t error;
    char *source = NULL;
    size_t length = 0;
    assert(mulib_pybind_generate("./libdemo.so", functions, 1,
                                &source, &length, &error) == MULIB_BIND_OK);
    assert(length == strlen(source));
    assert(strstr(source, "add.restype = _mulib_ctypes.c_int"));
    assert(strstr(source, "add.argtypes = [_mulib_ctypes.c_int, _mulib_ctypes.c_int]"));
    assert(fwrite(source, 1, length, stdout) == length);
    free(source);
    return 0;
}
