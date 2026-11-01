#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_URI
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#include "mulib-uri.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    mulib_uri_t base, ref, parsed;
    char *text;
    assert(mulib_uri_parse("https://example.org/a/b?old", 27, &base));
    assert(mulib_uri_parse("../c?new#part", 13, &ref));
    text = mulib_uri_resolve(&base, &ref);
    assert(text && !strcmp(text, "https://example.org/c?new#part"));
    assert(mulib_uri_parse(text, strlen(text), &parsed));
    assert(parsed.host.length == 11 && parsed.has_query && parsed.has_fragment);
    free(text);
    assert(!mulib_uri_parse("http://x/%zz", 12, &parsed));
    return 0;
}
