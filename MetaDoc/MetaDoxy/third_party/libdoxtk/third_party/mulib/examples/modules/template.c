#define MULIB_IMPL_TEMPLATE
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-template.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const char json[] = "{\"users\":[{\"name\":\"Ada & Lin\",\"active\":true},"
                        "{\"name\":\"Grace\",\"active\":false}]}";
    const char source[] = "{{#each users}}{{#if active}}<p>{{name}}</p>{{/if}}"
                          "{{else}}<p>No users</p>{{/each}}";
    mulib_json_t *context = mulib_json_parse(json, sizeof(json) - 1, NULL);
    mulib_template_error_t error;
    size_t length;
    char *output;
    assert(context);
    output = mulib_template_render(source, sizeof(source) - 1, context, &length, &error);
    assert(output && !error.message);
    assert(length == strlen("<p>Ada &amp; Lin</p>"));
    assert(strcmp(output, "<p>Ada &amp; Lin</p>") == 0);
    puts(output);
    free(output);
    mulib_json_destroy(context);
    return 0;
}
