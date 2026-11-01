#define MULIB_IMPL_TOML
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-toml.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const char source[] =
        "title = 'MuLIB'\n"
        "[service]\n"
        "listen.host = 'localhost'\n"
        "listen.ports = [8080, 8081]\n"
        "[[workers]]\n"
        "name = 'alpha'\n"
        "limits = { connections = 100, enabled = true }\n";
    mulib_toml_error_t error;
    mulib_json_t *config = mulib_toml_parse(source, sizeof(source) - 1, &error);
    mulib_json_t *ports, *copy;
    char *text;
    assert(config && !error.message);
    ports = mulib_json_get(mulib_json_get(mulib_json_get(config, "service"), "listen"), "ports");
    assert(mulib_json_numeric(mulib_json_at(ports, 0)) == 8080);
    assert(mulib_json_size(mulib_json_get(config, "workers")) == 1);
    text = mulib_toml_stringify(config, &error);
    assert(text);
    copy = mulib_toml_parse(text, strlen(text), &error);
    assert(copy && mulib_json_size(copy) == 3);
    fputs(text, stdout);
    free(text);
    mulib_json_destroy(copy);
    mulib_json_destroy(config);
    return 0;
}
