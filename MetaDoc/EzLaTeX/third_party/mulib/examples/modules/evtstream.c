#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_EVTSTREAM
#include "mulib-evtstream.h"
#include <assert.h>
int main(void) {
    mulib_ast_t *root = mulib_ast_create("root", "", 0);
    mulib_evtstream_t stream = mulib_evtstream_from_ast(root);
    mulib_event_t event;
    assert(root && mulib_evtstream_next(&stream, &event) == 1 && event.kind == MULIB_EVENT_ENTER);
    assert(mulib_evtstream_next(&stream, &event) == 1 && event.kind == MULIB_EVENT_LEAVE);
    assert(mulib_evtstream_next(&stream, &event) == 0);
    mulib_ast_destroy(root); return 0;
}
