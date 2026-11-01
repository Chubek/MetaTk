#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_JSON
#define MULIB_IMPL_BROWSERTOOLS
#include "mulib-browsertools.h"
#include <stdio.h>
#include <string.h>
/* Replace with a WebSocket text-frame sender. This example only prints. */
static int send_json(void *user, const char *json, size_t length) {
    (void)user;
    return fwrite(json,1,length,stdout) != length || putchar('\n') == EOF;
}
int main(void) {
    mulib_browsertools_t *client = mulib_browsertools_create(send_json,NULL,NULL);
    mulib_browsertools_message_t message = {0}; uint32_t id; int failed;
    const char reply[] = "{\"id\":1,\"sessionId\":\"page-session\",\"result\":{\"result\":{\"type\":\"string\",\"value\":\"Example\"}}}";
    if (!client) return 1;
    failed = mulib_browsertools_evaluate(client,"document.title","page-session",&id) != MULIB_BROWSERTOOLS_OK;
    if (!failed) failed = mulib_browsertools_receive(client,reply,strlen(reply),&message) != MULIB_BROWSERTOOLS_RESPONSE;
    if (!failed) printf("matched request %u; pending=%zu\n",(unsigned)id,mulib_browsertools_pending(client));
    mulib_browsertools_message_free(&message); mulib_browsertools_destroy(client); return failed ? 1 : 0;
}
