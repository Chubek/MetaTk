#define MULIB_IMPL_DSYMSPEC
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-dsymspec.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    mulib_dsymspec_error_t error;
    mulib_dsymspec_t *spec = mulib_dsymspec_load_builtin("amd64", &error);
    const mulib_json_t *record;
    assert(spec && !error.message);
    record = mulib_dsymspec_find(spec, "registers", "rdi");
    assert(record && mulib_json_numeric(mulib_json_get(record, "enc")) == 7);
    record = mulib_dsymspec_find(spec, "targets", "amd64");
    assert(!strcmp(mulib_json_text(mulib_json_get(record, "byte_order"), NULL), "little"));
    record = mulib_dsymspec_find(spec, "encoders", "ret");
    assert(!strcmp(mulib_json_text(mulib_json_get(record, "body"), NULL), "EMIT8 0xC3\n"));
    assert(!mulib_dsymspec_find(spec, "registers", "missing"));
    mulib_dsymspec_destroy(spec);
    return 0;
}
