#define MULIB_IMPL_DNS
#define MULIB_IMPL_DNSCLIENT
#define MULIB_IMPL_DNSCACHE
#define MULIB_IMPL_DNSRESOLVER
#define MULIB_IMPL_NAMESERVER
#include "mulib-dnsresolver.h"
#include "mulib-nameserver.h"
#include <assert.h>
#include <string.h>
typedef struct local_transport { mulib_nameserver_t *server; unsigned calls; } local_transport_t;
static int exchange(void *user, const unsigned char *q, size_t n,
                    unsigned char *r, size_t cap, size_t *written) {
    local_transport_t *local = (local_transport_t *)user; ++local->calls;
    return mulib_nameserver_answer(local->server, q, n, r, cap, written);
}
int main(void) {
    unsigned char query[512], reply[512], ip[4] = {192,0,2,8};
    mulib_dns_rr_t record = {0}; mulib_nameserver_t server; mulib_dnsresolver_t resolver;
    mulib_dnscache_t cache; mulib_dnscache_entry_t entries[2]; mulib_dnscache_result_t result;
    local_transport_t local = {&server, 0};
    strcpy(record.name, "www.example.org"); record.class_ = MULIB_DNS_IN; record.type = MULIB_DNS_A;
    record.ttl = 30; record.data = ip; record.data_length = sizeof(ip);
    assert(mulib_nameserver_init(&server, "example.org", 11, &record, 1) == MULIB_DNS_OK);
    assert(mulib_dnscache_init(&cache, entries, 2) == MULIB_DNS_OK);
    assert(mulib_dnsresolver_init(&resolver, exchange, &local, &cache, 321) == MULIB_DNS_OK);
    assert(mulib_dnsresolver_resolve(&resolver, record.name, 15, MULIB_DNS_A, 100, query, sizeof(query), reply, sizeof(reply), &result) == MULIB_DNS_OK);
    assert(result.count == 1 && result.addresses[0][3] == 8 && local.calls == 1);
    assert(mulib_dnsresolver_resolve(&resolver, record.name, 15, MULIB_DNS_A, 110, NULL, 0, NULL, 0, &result) == MULIB_DNS_OK);
    assert(result.ttl == 20 && local.calls == 1);
    return 0;
}
