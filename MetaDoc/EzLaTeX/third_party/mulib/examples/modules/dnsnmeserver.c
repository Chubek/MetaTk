#define MULIB_IMPL_DNS
#define MULIB_IMPL_DNSCLIENT
#define MULIB_IMPL_NAMESERVER
#define MULIB_IMPL_DNSNMESERVER
#include "mulib-dnsnmeserver.h"
#include <assert.h>
int main(void) {
    mulib_dnsnmeserver_t server; mulib_dns_message_t message;
    unsigned char query[512], reply[512]; size_t qn, rn;
    assert(mulib_dnsnmeserver_init(&server, "example.org", 11, NULL, 0) == MULIB_DNS_OK);
    assert(mulib_dnsclient_query("missing.example.org", 19, MULIB_DNS_A, 1, 0, query, sizeof(query), &qn) == MULIB_DNS_OK);
    assert(mulib_dnsnmeserver_answer(&server, query, qn, reply, sizeof(reply), &rn) == MULIB_DNS_OK);
    assert(mulib_dns_message_parse(reply, rn, 0, &message) == MULIB_DNS_OK);
    assert((message.header.flags & MULIB_DNS_AA) && (message.header.flags & 15u) == 3);
    return 0;
}
