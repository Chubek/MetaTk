#define MULIB_IMPL_YAML
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-yaml.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const char source[] =
        "service:\n"
        "  name: MuLIB\n"
        "  ports: [8080, 8081]\n"
        "workers:\n"
        "  - name: alpha\n"
        "    enabled: true\n";
    mulib_yaml_error_t error;
    mulib_json_t *config = mulib_yaml_parse(source, sizeof(source) - 1, &error);
    mulib_json_t *service, *copy;
    char *text;
    assert(config && !error.message);
    service = mulib_json_get(config, "service");
    assert(mulib_json_size(mulib_json_get(service, "ports")) == 2);
    assert(strcmp(mulib_json_text(mulib_json_get(service, "name"), NULL), "MuLIB") == 0);
    text = mulib_yaml_stringify(config, &error);
    assert(text);
    copy = mulib_yaml_parse(text, strlen(text), &error);
    assert(copy && mulib_json_size(copy) == 2);
    fputs(text, stdout);
    free(text);
    mulib_json_destroy(copy);
    mulib_json_destroy(config);
    return 0;
}
