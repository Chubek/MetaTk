/* Parse with parsec-backed JSON; transform with vec-backed vector algebra. */
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#define MULIB_IMPL_VEC
#define MULIB_IMPL_VECTOR
#include "mulib-json.h"
#include "mulib-vector.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    const char *source = "[3,4,0]";
    mulib_json_error_t error;
    mulib_json_t *input = mulib_json_parse(source, strlen(source), &error);
    mulib_json_t *output;
    mulib_vector_t vector;
    char *text;
    assert(input && mulib_vector_init(&vector, mulib_json_size(input)));
    for (size_t i = 0; i < mulib_json_size(input); ++i)
        assert(mulib_vector_set(&vector, i, mulib_json_numeric(mulib_json_at(input, i))));
    output = mulib_json_number(mulib_vector_norm(&vector));
    assert(output);
    text = mulib_json_stringify(output, 0);
    assert(text);
    puts(text); /* 5 */
    free(text);
    mulib_vector_free(&vector);
    mulib_json_destroy(input);
    mulib_json_destroy(output);
    return 0;
}
