#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#include "mulib-json.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char source[] = "{\"greeting\":\"hello\",\"items\":[true,null,2.5]}";
    mulib_json_error_t error;
    mulib_json_t *value = mulib_json_parse(source, sizeof source - 1, &error);
    char *text;
    assert(value && !error.message);
    assert(!strcmp(mulib_json_text(mulib_json_get(value, "greeting"), NULL), "hello"));
    assert(mulib_json_size(mulib_json_get(value, "items")) == 3);
    text = mulib_json_stringify(value, 0);
    assert(text && !strcmp(text, source));
    free(text);
    mulib_json_destroy(value);
    return 0;
}
