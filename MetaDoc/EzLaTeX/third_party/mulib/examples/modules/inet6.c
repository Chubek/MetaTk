#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#include "mulib-inet6.h"
#include <assert.h>
#include <string.h>
int main(void) {
    mulib_inet6_t address, network; char text[40];
    assert(mulib_inet6_parse("2001:DB8::1", 11, &address));
    assert(mulib_inet6_format(&address, text, sizeof(text)) && !strcmp(text, "2001:db8::1"));
    assert(mulib_inet6_network(&address, 64, &network));
    assert(mulib_inet6_contains(&network, 64, &address));
    assert(mulib_inet6_format(&network, text, sizeof(text)) && !strcmp(text, "2001:db8::"));
    assert(!mulib_inet6_parse("[::1]", 5, &address));
    return 0;
}
