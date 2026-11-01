#define MULIB_IMPL_MICROJS
#include "mulib-microjs.h"
#include <stdio.h>
#include <string.h>
int main(void) {
    const char *source = "let price=12.5; let quantity=4; price*quantity >= 50 && 'ok' === 'ok'";
    mulib_microjs_t *js = mulib_microjs_create(NULL);
    mulib_microjs_value_t result = {0}; mulib_microjs_error_t error;
    if (!js) return 1;
    if (mulib_microjs_eval(js, source, strlen(source), &result, &error) != MULIB_MICROJS_OK) {
        fprintf(stderr, "%zu:%zu: %s\n", error.line, error.column, error.message);
        mulib_microjs_destroy(js); return 1;
    }
    printf("checked expression: %s\n", result.boolean ? "true" : "false");
    mulib_microjs_value_free(&result); mulib_microjs_destroy(js); return 0;
}
