#define MULIB_IMPL_REGEX
#include "mulib-regex.h"
#include <assert.h>

int main(void) {
    const char pattern[] = "[A-Za-z_][A-Za-z_0-9]*";
    mulib_regex_error_t error;
    mulib_regex_match_t match;
    mulib_regex_t *regex = mulib_regex_compile(pattern, sizeof pattern - 1, &error);
    assert(regex && !error.message);
    assert(mulib_regex_search(regex, "42 answer_1!", 12, 0, &match) == 1);
    assert(match.offset == 3 && match.length == 8);
    assert(mulib_regex_full_match(regex, "answer_1", 8) == 1);
    assert(mulib_regex_full_match(regex, "42", 2) == 0);
    mulib_regex_destroy(regex);
    return 0;
}
