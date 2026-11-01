#define MULIB_IMPL_DNS
#define MULIB_IMPL_DNSCACHE
#include "mulib-dnscache.h"
#include <assert.h>
int main(void) {
    mulib_dnscache_t cache; mulib_dnscache_entry_t slots[4];
    mulib_dnscache_result_t value = {0}, hit;
    value.type = MULIB_DNS_A; value.ttl = 30; value.count = 1;
    value.addresses[0][0] = 192; value.addresses[0][2] = 2; value.addresses[0][3] = 1;
    assert(mulib_dnscache_init(&cache, slots, 4) == MULIB_DNS_OK);
    assert(mulib_dnscache_put(&cache, "EXAMPLE.org.", 12, &value, 100) == MULIB_DNS_OK);
    value.addresses[0][3] = 99; /* The cache owns its copy. */
    assert(mulib_dnscache_get(&cache, "example.org", 11, MULIB_DNS_A, 105, &hit) == MULIB_DNS_OK);
    assert(hit.ttl == 25 && hit.addresses[0][3] == 1);
    assert(mulib_dnscache_get(&cache, "example.org", 11, MULIB_DNS_A, 130, &hit) == MULIB_DNS_NOTFOUND);
    return 0;
}
