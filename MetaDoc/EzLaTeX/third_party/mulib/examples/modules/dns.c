#define MULIB_IMPL_DNS
#include "mulib-dns.h"
#include <assert.h>
#include <string.h>
int main(void) {
    unsigned char packet[512], address[4] = {192,0,2,1};
    mulib_dns_writer_t writer; mulib_dns_message_t message; mulib_dns_iterator_t it;
    mulib_dns_question_t question = {"www.example.org", MULIB_DNS_A, MULIB_DNS_IN};
    mulib_dns_rr_t record = {0}, decoded; unsigned section;
    strcpy(record.name, question.name); record.type = MULIB_DNS_A; record.class_ = MULIB_DNS_IN;
    record.ttl = 60; record.data = address; record.data_length = sizeof(address);
    assert(mulib_dns_writer_init(&writer, packet, sizeof(packet), 42, MULIB_DNS_QR|MULIB_DNS_AA) == MULIB_DNS_OK);
    assert(mulib_dns_writer_question(&writer, &question) == MULIB_DNS_OK);
    assert(mulib_dns_writer_rr(&writer, 1, &record) == MULIB_DNS_OK);
    assert(mulib_dns_message_parse(packet, writer.length, 0, &message) == MULIB_DNS_OK);
    mulib_dns_iterator_init(&it, &message);
    assert(mulib_dns_next_question(&it, &question) == MULIB_DNS_OK);
    assert(mulib_dns_next_rr(&it, &decoded, &section) == MULIB_DNS_OK);
    assert(section == 1 && decoded.ttl == 60 && !memcmp(decoded.data, address, 4));
    assert(mulib_dns_next_rr(&it, &decoded, &section) == MULIB_DNS_END);
    return 0;
}
