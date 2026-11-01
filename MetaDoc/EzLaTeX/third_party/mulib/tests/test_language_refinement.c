/* Standalone numeric regressions; no transitive implementation dependencies. */
#define MULIB_IMPL_MICROJS
#include "mulib-microjs.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void number(mulib_microjs_t *js, const char *source, double expected)
{
    mulib_microjs_value_t value = {0};
    mulib_microjs_error_t error;
    assert(mulib_microjs_eval(js, source, strlen(source), &value, &error) == MULIB_MICROJS_OK);
    assert(!error.code && !error.message && value.type == MULIB_MICROJS_NUMBER);
    if (expected == 0) assert(value.number == 0);
    else assert(fabs(value.number - expected) <= fabs(expected) * (4 * DBL_EPSILON));
    mulib_microjs_value_free(&value);
}

static void compensated(mulib_microjs_t *js, size_t zeros)
{
    char *source = (char *)malloc(zeros + 64);
    assert(source);
    source[0] = '1'; memset(source + 1, '0', zeros);
    (void)snprintf(source + zeros + 1, 63, "e-%zu", zeros);
    number(js, source, 1);
    source[0] = '0'; source[1] = '.'; memset(source + 2, '0', zeros);
    (void)snprintf(source + zeros + 2, 62, "125e%zu", zeros + 1);
    number(js, source, 1.25);
    free(source);
}

int main(void)
{
    mulib_microjs_t *js = mulib_microjs_create(NULL);
    mulib_microjs_value_t value = {0};
    mulib_microjs_error_t error;
    const char *failure = "let stable=7; stable=1e999999999999999999999999999999999999999";
    assert(js);
    number(js, "12.5e-1", 1.25);
    number(js, "00000125e-2", 1.25);
    number(js, "1e-999999999999999999999999999999999999999", 0);
    number(js, "0e999999999999999999999999999999999999999", 0);
    number(js, "0.000e-999999999999999999999999999999999999999", 0);
    compensated(js, 400);
    compensated(js, 120000);
    assert(mulib_microjs_eval(js, failure, strlen(failure), &value, &error) == MULIB_MICROJS_ARITHMETIC);
    assert(value.type == MULIB_MICROJS_NULL && !value.string && error.message);
    number(js, "stable", 7);
    if (DBL_MAX_10_EXP == 308 && DBL_MANT_DIG == 53) {
        number(js, "1.7976931348623157e308", DBL_MAX);
        number(js, "2.2250738585072014e-308", DBL_MIN);
        /* This nonzero subnormal used to become zero before scaling. */
        number(js, "5e-324", nextafter(0.0, 1.0));
    }
    mulib_microjs_destroy(js);
    puts("language refinement tests passed");
    return 0;
}
