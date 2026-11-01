/* cc -std=c11 -Iinclude examples/calculator.c -lm -o calculator
 * ./calculator 'let subtotal = 7 * 6; subtotal / 2'
 * This demonstrates the documented MicroJS subset, not a JavaScript runtime.
 */
#define MULIB_IMPL_MICROJS
#include "mulib-microjs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *source = argc == 2 ? argv[1] : "let subtotal = 6 * 7; subtotal";
    mulib_microjs_value_t result = {0};
    mulib_microjs_error_t error = {0};
    mulib_microjs_t *interpreter;
    int status = EXIT_FAILURE;
    if (argc > 2) {
        fprintf(stderr, "usage: %s ['numeric expression or statements']\n", argv[0]);
        return status;
    }
    interpreter = mulib_microjs_create(NULL);
    if (!interpreter) {
        fputs("calculator: allocation failed\n", stderr);
        return status;
    }
    if (mulib_microjs_eval(interpreter, source, strlen(source), &result, &error) != MULIB_MICROJS_OK) {
        fprintf(stderr, "calculator:%zu:%zu: %s\n", error.line, error.column,
                error.message ? error.message : "evaluation failed");
    } else if (result.type != MULIB_MICROJS_NUMBER) {
        fputs("calculator: expression must produce a number\n", stderr);
    } else {
        if (argc == 1) assert(result.number == 42.0);
        printf("%.17g\n", result.number);
        status = EXIT_SUCCESS;
    }
    mulib_microjs_value_free(&result);
    mulib_microjs_destroy(interpreter);
    return status;
}
