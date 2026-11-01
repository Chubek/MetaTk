#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#define MULIB_IMPL_RPC
#include "mulib-rpc.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int add(const mulib_json_t *params, mulib_json_t **result, void *user) {
    (void)user;
    if (mulib_json_typeof(params) != MULIB_JSON_ARRAY || mulib_json_size(params) != 2 ||
        mulib_json_typeof(mulib_json_at(params, 0)) != MULIB_JSON_NUMBER ||
        mulib_json_typeof(mulib_json_at(params, 1)) != MULIB_JSON_NUMBER) return -32602;
    *result = mulib_json_number(mulib_json_numeric(mulib_json_at(params, 0)) + mulib_json_numeric(mulib_json_at(params, 1)));
    return *result ? 0 : -32603;
}
int main(void) {
    const mulib_rpc_method_t methods[] = { {"add", add, NULL} };
    const char request[] = "{\"jsonrpc\":\"2.0\",\"method\":\"add\",\"params\":[2,3],\"id\":7}";
    char *text = NULL;
    mulib_json_t *response;
    assert(mulib_rpc_process(methods, 1, request, strlen(request), 0, &text));
    assert(text);
    response = mulib_json_parse(text, strlen(text), NULL);
    assert(response && mulib_json_numeric(mulib_json_get(response, "result")) == 5);
    mulib_json_destroy(response); free(text);
    return 0;
}
