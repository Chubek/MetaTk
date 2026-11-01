#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PEG
#include "mulib-peg.h"
#include <assert.h>
#include <string.h>

int main(void) {
    const char grammar[] =
        "List <- Number (',' Number)*;"
        "Number <- [0-9]+;";
    mulib_peg_error_t error;
    mulib_parsec_result_t result;
    mulib_peg_t *peg = mulib_peg_compile(grammar, sizeof grammar - 1, &error);
    assert(peg && mulib_peg_rule_count(peg) == 2);
    assert(!strcmp(mulib_peg_rule_name(peg, 1), "Number"));
    assert(mulib_peg_run(peg, NULL, "12,34", 5, 1, NULL, &result) == 1);
    assert(result.count == 3 && result.captures[2].tag == 0);
    mulib_parsec_result_free(&result);
    assert(mulib_peg_run(peg, NULL, "12,", 3, 1, NULL, &result) == 0);
    mulib_parsec_result_free(&result);
    mulib_peg_destroy(peg);
    return 0;
}
