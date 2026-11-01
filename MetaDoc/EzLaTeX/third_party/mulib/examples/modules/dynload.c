#define MULIB_IMPL_DYNLOAD
#include "mulib-dynload.h"
#include <assert.h>

int main(void) {
    mulib_dynload_t library = MULIB_DYNLOAD_INIT;
    char error[256];
    void *symbol = NULL;
#if defined(_WIN32)
    assert(mulib_dynload_open(&library, "kernel32.dll", error, sizeof(error)) == 0);
    assert(mulib_dynload_symbol(&library, "GetCurrentProcessId", &symbol, error, sizeof(error)) == 0);
#elif defined(__unix__) || defined(__APPLE__)
    assert(mulib_dynload_open(&library, NULL, error, sizeof(error)) == 0);
    assert(mulib_dynload_symbol(&library, "malloc", &symbol, error, sizeof(error)) == 0);
#else
    assert(mulib_dynload_open(&library, NULL, error, sizeof(error)) == MULIB_DYNLOAD_ENOTSUP);
    (void)symbol;
    return 0;
#endif
    assert(symbol && error[0] == '\0');
    assert(mulib_dynload_close(&library, error, sizeof(error)) == 0);
    return 0;
}
