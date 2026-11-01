#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_GETADDR
#include "mulib-getaddr.h"
#include <assert.h>
#include <string.h>
int main(void) {
    mulib_getaddr_address_t addresses[16]; size_t count; char text[40];
    assert(mulib_getaddr_resolve("2001:db8::1", 443, 6, 0, addresses, 16, &count) == MULIB_GETADDR_OK);
    assert(count == 1 && addresses[0].family == 6 && addresses[0].port == 443);
    assert(mulib_getaddr_format(addresses, text, sizeof(text)) == MULIB_GETADDR_OK && !strcmp(text, "2001:db8::1"));
    assert(mulib_getaddr_resolve("192.0.2.1", 80, 4, 0, NULL, 0, &count) == MULIB_GETADDR_LIMIT && count == 1);
#if MULIB_GETADDR_HAS_SYSTEM
    assert(mulib_getaddr_resolve("localhost", 80, 0, MULIB_GETADDR_SYSTEM, addresses, 16, &count) == MULIB_GETADDR_OK && count);
#else
    assert(mulib_getaddr_resolve("localhost", 80, 0, MULIB_GETADDR_SYSTEM, addresses, 16, &count) == MULIB_GETADDR_UNSUPPORTED);
#endif
    return 0;
}
