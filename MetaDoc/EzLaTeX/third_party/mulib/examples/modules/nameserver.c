#define MULIB_IMPL_DNS
#define MULIB_IMPL_DNSCLIENT
#define MULIB_IMPL_NAMESERVER
#include "mulib-nameserver.h"
#include <assert.h>
#include <string.h>
int main(void) {
    unsigned char query[512], reply[512], ip[4] = {192,0,2,1}; size_t qn, rn;
    mulib_dns_rr_t record = {0}; mulib_nameserver_t server; mulib_dns_message_t message;
    strcpy(record.name, "www.example.org"); record.type = MULIB_DNS_A; record.class_ = MULIB_DNS_IN;
    record.ttl = 60; record.data = ip; record.data_length = sizeof(ip);
    assert(mulib_nameserver_init(&server, "example.org", 11, &record, 1) == MULIB_DNS_OK);
    assert(mulib_dnsclient_query(record.name, strlen(record.name), MULIB_DNS_A, 1, 0, query, sizeof(query), &qn) == MULIB_DNS_OK);
    assert(mulib_nameserver_answer(&server, query, qn, reply, sizeof(reply), &rn) == MULIB_DNS_OK);
    assert(mulib_dns_message_parse(reply, rn, 0, &message) == MULIB_DNS_OK);
    assert((message.header.flags & MULIB_DNS_AA) && message.header.counts[1] == 1);
    assert(mulib_dnsclient_query("absent.example.org", 18, MULIB_DNS_A, 2, 0, query, sizeof(query), &qn) == MULIB_DNS_OK);
    assert(mulib_nameserver_answer(&server, query, qn, reply, sizeof(reply), &rn) == MULIB_DNS_OK);
    assert(mulib_dns_message_parse(reply, rn, 0, &message) == MULIB_DNS_OK && (message.header.flags & 15u) == 3);
    return 0;
}
