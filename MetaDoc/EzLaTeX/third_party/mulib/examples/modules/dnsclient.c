#define MULIB_IMPL_DNS
#define MULIB_IMPL_DNSCLIENT
#include "mulib-dnsclient.h"
#include <assert.h>
#include <string.h>
/* A local transport for the example; production transports also check the peer. */
static int exchange(void *user, const unsigned char *q, size_t n,
                    unsigned char *r, size_t cap, size_t *written) {
    (void)user;
    if (n > cap) return MULIB_DNS_LIMIT;
    memcpy(r, q, n); r[2] |= 0x80; /* Valid empty NOERROR response, echoing question. */
    *written = n; return MULIB_DNS_OK;
}
int main(void) {
    unsigned char query[512], reply[512]; size_t qn, rn; mulib_dns_message_t message;
    assert(mulib_dnsclient_query("example.org", 11, MULIB_DNS_A, 123, 1, query, sizeof(query), &qn) == MULIB_DNS_OK);
    assert(mulib_dnsclient_exchange(exchange, NULL, query, qn, reply, sizeof(reply), &rn, &message) == MULIB_DNS_OK);
    assert(rn == qn && message.header.id == 123 && !message.header.counts[1]);
    reply[0] ^= 1;
    { mulib_dns_message_t q, r;
      assert(mulib_dns_message_parse(query, qn, 0, &q) == MULIB_DNS_OK);
      assert(mulib_dns_message_parse(reply, rn, 0, &r) == MULIB_DNS_OK);
      assert(mulib_dnsclient_validate_reply(&q, &r) == MULIB_DNS_MISMATCH); }
    return 0;
}
