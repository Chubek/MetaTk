#define MULIB_IMPL_PARSEC
#include "mulib-parsec.h"
#include <assert.h>

int main(void) {
    mulib_parsec_parser_t digit = mulib_parsec_range('0', '9');
    mulib_parsec_parser_t digits = mulib_parsec_repeat(&digit, 1, SIZE_MAX);
    mulib_parsec_parser_t parts[2], number;
    mulib_parsec_result_t result;
    parts[0] = mulib_parsec_capture(&digits, 42);
    parts[1] = mulib_parsec_end();
    number = mulib_parsec_sequence(parts, 2);
    assert(mulib_parsec_run(&number, "123", 3, NULL, &result) == 1);
    assert(result.consumed == 3 && result.count == 1);
    assert(result.captures[0].tag == 42 && result.captures[0].length == 3);
    mulib_parsec_result_free(&result);
    assert(mulib_parsec_run(&number, "12x", 3, NULL, &result) == 0);
    assert(result.error.offset == 2 && result.error.column == 3);
    mulib_parsec_result_free(&result);
    return 0;
}
