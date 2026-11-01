#define MULIB_IMPL_DNS
#define MULIB_IMPL_DNSCLIENT
#define MULIB_IMPL_DNSCACHE
#define MULIB_IMPL_DNSRESOLVER
#define MULIB_IMPL_NAMESERVER
#define MULIB_IMPL_DNSNMESERVER
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INTE6
#define MULIB_IMPL_GETADDR
#include "mulib-dnsresolver.h"
#include "mulib-dnsnmeserver.h"
#include "mulib-getaddr.h"
#include "mulib-inte6.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_names(void) {
    unsigned char packet[512], sentinel[256]; char text[256], before[256]; size_t n, used;
    static const char *const bad[] = {"", "..", ".a", "a..b", "a..", "a b", "a\\b", "a\x7f", "a\xc3\xa9"};
    assert(mulib_dns_name_normalize("ExAmPlE.Org.", 12, text) == MULIB_DNS_OK && !strcmp(text, "example.org"));
    assert(mulib_dns_name_equal("EXAMPLE.org.", "example.ORG"));
    assert(!mulib_dns_name_equal("bad..", "bad"));
    for (size_t i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i)
        assert(mulib_dns_name_normalize(bad[i], strlen(bad[i]), text) == MULIB_DNS_INVALID);
    assert(mulib_dns_name_normalize("a\0b", 3, text) == MULIB_DNS_INVALID);
    memset(before, 'x', sizeof(before)); memcpy(text, before, sizeof(text));
    used = 91; packet[0] = 0xc0; packet[1] = 0;
    assert(mulib_dns_name_decode(packet, 2, 0, text, &used) == MULIB_DNS_INVALID);
    assert(used == 91 && !memcmp(text, before, sizeof(text)));
    packet[0] = 1; packet[1] = 'a'; packet[2] = 0xc0; packet[3] = 0;
    assert(mulib_dns_name_decode(packet, 4, 0, text, &used) < 0);
    packet[0] = 0x40;
    assert(mulib_dns_name_decode(packet, 1, 0, text, &used) == MULIB_DNS_UNSUPPORTED);
    packet[0] = 1;
    assert(mulib_dns_name_decode(packet, 1, 0, text, &used) == MULIB_DNS_INVALID);
    assert(mulib_dns_name_encode("a.example", 9, packet, sizeof(packet), &n) == MULIB_DNS_OK);
    packet[n] = 1; packet[n+1] = 'b'; packet[n+2] = 0xc0; packet[n+3] = 2;
    assert(mulib_dns_name_decode(packet, n+4, n, text, &used) == MULIB_DNS_OK);
    assert(!strcmp(text, "b.example") && used == 4);
    packet[n+3] = 255;
    assert(mulib_dns_name_decode(packet, n+4, n, text, &used) == MULIB_DNS_INVALID);
    assert(mulib_dns_name_encode(".", 1, packet, sizeof(packet), &n) == MULIB_DNS_OK && n == 1 && !packet[0]);
    assert(mulib_dns_name_decode(packet, n, 0, text, &used) == MULIB_DNS_OK && !strcmp(text, "."));
    memset(text, 'a', 253); text[63] = text[127] = text[191] = '.'; text[253] = 0;
    assert(mulib_dns_name_encode(text, 253, packet, sizeof(packet), &n) == MULIB_DNS_OK && n == 255);
    assert(mulib_dns_name_decode(packet, n, 0, before, &used) == MULIB_DNS_OK && !strcmp(text, before));
    memcpy(sentinel, packet, 255); used = 999;
    assert(mulib_dns_name_encode(text, 253, packet, 254, &used) == MULIB_DNS_LIMIT);
    assert(used == 999 && !memcmp(packet, sentinel, 255));
    text[63] = 'a';
    assert(mulib_dns_name_normalize(text, 253, before) == MULIB_DNS_INVALID);
    memset(packet, 0, sizeof(packet));
    for (size_t i = 1; i <= 129; ++i) {
        packet[2*i] = (unsigned char)(0xc0u | ((2*(i-1)) >> 8)); packet[2*i+1] = (unsigned char)(2*(i-1));
    }
    assert(mulib_dns_name_decode(packet, 260, 258, before, &used) == MULIB_DNS_LIMIT);
}

static void rr_init(mulib_dns_rr_t *rr, const char *name, uint16_t type,
                    const unsigned char *data, size_t length, uint32_t ttl) {
    memset(rr, 0, sizeof(*rr)); assert(strlen(name) < sizeof(rr->name)); strcpy(rr->name, name);
    rr->class_ = MULIB_DNS_IN; rr->type = type; rr->ttl = ttl; rr->data = data; rr->data_length = length;
}

static void test_codec(void) {
    unsigned char packet[2048], again[2048], changed[2048], a[4] = {192,0,2,1}, a6[16] = {0};
    const unsigned char txt[] = {3,'a',0,'b',0};
    mulib_dns_writer_t w, other; mulib_dns_message_t m, m2; mulib_dns_iterator_t it;
    mulib_dns_question_t q; mulib_dns_rr_t rr, parsed; unsigned sec; size_t saved, count;
    memset(&q, 0, sizeof(q)); strcpy(q.name, "www.example.org"); q.type = MULIB_DNS_A; q.class_ = MULIB_DNS_IN;
    assert(mulib_dns_writer_init(&w, packet, sizeof(packet), 42, MULIB_DNS_QR|MULIB_DNS_AA) == MULIB_DNS_OK);
    assert(mulib_dns_writer_question(&w, &q) == MULIB_DNS_OK);
    rr_init(&rr, q.name, MULIB_DNS_A, a, sizeof(a), 120);
    saved = w.length;
    assert(mulib_dns_writer_rr(&w, 1, &rr) == MULIB_DNS_OK && packet[saved] == 0xc0 && packet[saved+1] == 12);
    rr_init(&rr, "www.example.org", MULIB_DNS_AAAA, a6, sizeof(a6), 90);
    assert(mulib_dns_writer_rr(&w, 1, &rr) == MULIB_DNS_OK);
    rr_init(&rr, "example.org", MULIB_DNS_MX, NULL, 0, 100); strcpy(rr.target, "mail.example.org"); rr.preference = 10;
    assert(mulib_dns_writer_rr(&w, 1, &rr) == MULIB_DNS_OK);
    rr.type = MULIB_DNS_CNAME; strcpy(rr.name, "alias.example.org"); strcpy(rr.target, q.name);
    assert(mulib_dns_writer_rr(&w, 1, &rr) == MULIB_DNS_OK);
    rr.type = MULIB_DNS_PTR; strcpy(rr.name, "1.2.0.192.in-addr.arpa");
    assert(mulib_dns_writer_rr(&w, 1, &rr) == MULIB_DNS_OK);
    rr_init(&rr, "example.org", MULIB_DNS_NS, NULL, 0, 80); strcpy(rr.target, "ns.example.org");
    assert(mulib_dns_writer_rr(&w, 2, &rr) == MULIB_DNS_OK);
    rr_init(&rr, "example.org", MULIB_DNS_TXT, txt, sizeof(txt), 60);
    assert(mulib_dns_writer_rr(&w, 3, &rr) == MULIB_DNS_OK);
    saved = w.length;
    assert(mulib_dns_writer_rr(&w, 1, &rr) == MULIB_DNS_INVALID && w.length == saved);
    assert(mulib_dns_writer_question(&w, &q) == MULIB_DNS_INVALID);
    assert(mulib_dns_message_parse(packet, w.length, 0, &m) == MULIB_DNS_OK);
    assert(mulib_dns_message_parse(packet, w.length, 7, &m2) == MULIB_DNS_LIMIT);
    assert(m.header.counts[0] == 1 && m.header.counts[1] == 5 && m.header.counts[2] == 1 && m.header.counts[3] == 1);
    assert(mulib_dns_writer_init(&other, again, sizeof(again), m.header.id, m.header.flags) == MULIB_DNS_OK);
    mulib_dns_iterator_init(&it, &m);
    assert(mulib_dns_next_rr(&it, &parsed, &sec) == MULIB_DNS_INVALID);
    assert(mulib_dns_next_question(&it, &q) == MULIB_DNS_OK);
    assert(mulib_dns_writer_question(&other, &q) == MULIB_DNS_OK);
    assert(mulib_dns_next_question(&it, &q) == MULIB_DNS_END);
    count = 0;
    while (mulib_dns_next_rr(&it, &parsed, &sec) == MULIB_DNS_OK) {
        if (parsed.type == MULIB_DNS_MX) assert(parsed.preference == 10 && !strcmp(parsed.target, "mail.example.org"));
        if (parsed.type == MULIB_DNS_TXT) assert(parsed.data_length == sizeof(txt) && !memcmp(parsed.data, txt, sizeof(txt)));
        assert(mulib_dns_writer_rr(&other, sec, &parsed) == MULIB_DNS_OK); ++count;
    }
    assert(count == 7 && other.length == w.length && !memcmp(packet, again, w.length));
    for (size_t i = 0; i < w.length; ++i) assert(mulib_dns_message_parse(packet, i, 0, &m2) < 0);
    memcpy(changed, packet, w.length); changed[w.length] = 0;
    assert(mulib_dns_message_parse(changed, w.length+1, 0, &m2) == MULIB_DNS_INVALID);
    /* First RR starts immediately after the question, with two-byte compressed owner. */
    saved = 12+17+4;
    memcpy(changed, packet, w.length); changed[saved+2] = 0; changed[saved+3] = 6;
    assert(mulib_dns_message_parse(changed, w.length, 0, &m2) == MULIB_DNS_UNSUPPORTED);
    memcpy(changed, packet, w.length); changed[saved+11] = 5;
    assert(mulib_dns_message_parse(changed, w.length, 0, &m2) < 0);
    rr.type = MULIB_DNS_TXT; rr.data_length = 1; rr.data = txt;
    assert(mulib_dns_rr_validate(&rr) == MULIB_DNS_INVALID);
    assert(mulib_dns_writer_init(&other, again, 34, 1, 0) == MULIB_DNS_OK);
    assert(mulib_dns_writer_question(&other, &q) == MULIB_DNS_OK);
    rr_init(&rr, q.name, MULIB_DNS_A, a, 4, 1); saved = other.length;
    assert(mulib_dns_writer_rr(&other, 1, &rr) == MULIB_DNS_LIMIT && other.length == saved && other.counts[1] == 0);
    assert(mulib_dns_message_parse(again, other.length, 0, &m2) == MULIB_DNS_OK);
    assert(mulib_dns_writer_init(&other, again, sizeof(again), 1, 0) == MULIB_DNS_OK);
    strcpy(q.name, ".");
    for (size_t i = 0; i < 300; ++i) assert(mulib_dns_writer_question(&other, &q) == MULIB_DNS_OK);
    assert(mulib_dns_message_parse(again, other.length, 0, &m2) == MULIB_DNS_OK);
    memset(again, 0, 12); again[4] = 4; again[5] = 1;
    assert(mulib_dns_message_parse(again, 12, 0, &m2) == MULIB_DNS_LIMIT);
}

typedef struct transport_state { mulib_nameserver_t *server; unsigned calls; int mode; } transport_state_t;
static int transport(void *user, const unsigned char *q, size_t qn, unsigned char *r, size_t cap, size_t *rn) {
    transport_state_t *state = (transport_state_t *)user; int rc;
    ++state->calls;
    if (state->mode == 1) return MULIB_DNS_TRANSPORT;
    if (state->mode == 2) { *rn = cap+1; return MULIB_DNS_OK; }
    rc = mulib_nameserver_answer(state->server, q, qn, r, cap, rn);
    if (rc == MULIB_DNS_OK && state->mode == 3) r[0] ^= 1;
    return rc;
}

static void test_cache(void) {
    mulib_dnscache_t cache, empty; mulib_dnscache_entry_t entries[2]; mulib_dnscache_result_t value, out;
    memset(&value, 0, sizeof(value)); value.type = MULIB_DNS_A; value.count = 1; value.ttl = 10; value.addresses[0][0] = 1;
    assert(mulib_dnscache_init(&cache, entries, 2) == MULIB_DNS_OK);
    assert(mulib_dnscache_init(&empty, NULL, 0) == MULIB_DNS_OK);
    assert(mulib_dnscache_init(&empty, NULL, SIZE_MAX) == MULIB_DNS_INVALID);
    assert(mulib_dnscache_put(&empty, "x", 1, &value, 100) == MULIB_DNS_LIMIT);
    assert(mulib_dnscache_put(&cache, "EXAMPLE.", 8, &value, 100) == MULIB_DNS_OK);
    value.addresses[0][0] = 99;
    assert(mulib_dnscache_get(&cache, "example", 7, MULIB_DNS_A, 103, &out) == MULIB_DNS_OK);
    assert(out.ttl == 7 && out.addresses[0][0] == 1);
    out.ttl = 444;
    assert(mulib_dnscache_get(&cache, "example", 7, MULIB_DNS_AAAA, 103, &out) == MULIB_DNS_NOTFOUND && out.ttl == 444);
    assert(mulib_dnscache_put(&cache, "other", 5, &value, 100) == MULIB_DNS_OK);
    value.ttl = 20;
    assert(mulib_dnscache_put(&cache, "third", 5, &value, 101) == MULIB_DNS_OK);
    assert(mulib_dnscache_get(&cache, "example", 7, MULIB_DNS_A, 101, &out) == MULIB_DNS_NOTFOUND);
    assert(mulib_dnscache_get(&cache, "other", 5, MULIB_DNS_A, 110, &out) == MULIB_DNS_NOTFOUND);
    assert(mulib_dnscache_expire(&cache, 100) == 1);
    assert(mulib_dnscache_put(&cache, "edge", 4, &value, UINT64_MAX-3) == MULIB_DNS_OK);
    assert(mulib_dnscache_get(&cache, "edge", 4, MULIB_DNS_A, UINT64_MAX, &out) == MULIB_DNS_OK && out.ttl == 17);
    value.ttl = 0;
    assert(mulib_dnscache_put(&cache, "edge", 4, &value, UINT64_MAX) == MULIB_DNS_OK);
    assert(mulib_dnscache_get(&cache, "edge", 4, MULIB_DNS_A, UINT64_MAX, &out) == MULIB_DNS_NOTFOUND);
    value.count = 9;
    assert(mulib_dnscache_put(&cache, "edge", 4, &value, 0) == MULIB_DNS_LIMIT);
}

static void test_limits(void) {
    unsigned char packet[7000], reply[1024], addresses[9][4] = {{0}};
    mulib_dns_writer_t w; mulib_dns_question_t q = {".", MULIB_DNS_A, MULIB_DNS_IN};
    mulib_dns_message_t m; size_t saved; mulib_dns_rr_t records[9]; mulib_nameserver_t server;
    mulib_dnsresolver_t resolver; mulib_dnscache_result_t result; transport_state_t state;
    assert(mulib_dns_writer_init(&w, packet, sizeof(packet), 0, 0) == MULIB_DNS_OK);
    for (size_t i = 0; i < MULIB_DNS_MAX_RECORDS; ++i) assert(mulib_dns_writer_question(&w, &q) == MULIB_DNS_OK);
    assert(mulib_dns_message_parse(packet, w.length, 0, &m) == MULIB_DNS_OK);
    saved = w.length;
    assert(mulib_dns_writer_question(&w, &q) == MULIB_DNS_LIMIT && w.length == saved);
    assert(mulib_dns_writer_init(&w, packet, sizeof(packet), 0, 0) == MULIB_DNS_OK);
    for (unsigned i = 0; i < 200; ++i) {
        (void)snprintf(q.name, sizeof(q.name), "n%u.example.org", i);
        assert(mulib_dns_writer_question(&w, &q) == MULIB_DNS_OK);
    }
    assert(w.suffix_count == 128 && mulib_dns_message_parse(packet, w.length, 0, &m) == MULIB_DNS_OK);
    for (size_t i = 0; i < 9; ++i) rr_init(&records[i], "www.example.org", MULIB_DNS_A, addresses[i], 4, 10);
    assert(mulib_nameserver_init(&server, "example.org", 11, records, 9) == MULIB_DNS_OK);
    state.server = &server; state.calls = 0; state.mode = 0;
    assert(mulib_dnsresolver_init(&resolver, transport, &state, NULL, 1) == MULIB_DNS_OK);
    memset(&result, 0, sizeof(result)); result.ttl = 777;
    assert(mulib_dnsresolver_resolve(&resolver, "www.example.org", 15, MULIB_DNS_A, 0, packet, sizeof(packet), reply, sizeof(reply), &result) == MULIB_DNS_LIMIT && result.ttl == 777);
    records[0].ttl = UINT32_MAX;
    assert(mulib_nameserver_init(&server, "example.org", 11, records, 1) == MULIB_DNS_OK);
    assert(mulib_dnsresolver_resolve(&resolver, "www.example.org", 15, MULIB_DNS_A, 0, packet, sizeof(packet), reply, sizeof(reply), &result) == MULIB_DNS_OK && result.ttl == 0);
    assert(mulib_nameserver_init(&server, ".", 1, records, MULIB_DNS_MAX_RECORDS) == MULIB_DNS_LIMIT);
}

static void test_reply_policy(void) {
    unsigned char query[512], reply[512]; size_t qn;
    mulib_dns_message_t q, r; mulib_dns_question_t question;
    assert(mulib_dnsclient_query("example.org", 11, MULIB_DNS_A, 42, 1, query, sizeof(query), &qn) == MULIB_DNS_OK);
    assert(mulib_dns_message_parse(query, qn, 0, &q) == MULIB_DNS_OK);
    memcpy(reply, query, qn); reply[2] |= 0x80; reply[12+1] = 'E';
    assert(mulib_dns_message_parse(reply, qn, 0, &r) == MULIB_DNS_OK);
    assert(mulib_dnsclient_validate_reply(&q, &r) == MULIB_DNS_OK);
    reply[qn-3] = MULIB_DNS_AAAA;
    assert(mulib_dns_message_parse(reply, qn, 0, &r) == MULIB_DNS_OK);
    assert(mulib_dnsclient_validate_reply(&q, &r) == MULIB_DNS_MISMATCH);
    reply[qn-3] = MULIB_DNS_A; reply[3] = 3;
    assert(mulib_dns_message_parse(reply, qn, 0, &r) == MULIB_DNS_OK);
    assert(mulib_dnsclient_validate_reply(&q, &r) == MULIB_DNS_NOTFOUND);
    reply[3] = 2;
    assert(mulib_dns_message_parse(reply, qn, 0, &r) == MULIB_DNS_OK);
    assert(mulib_dnsclient_validate_reply(&q, &r) == MULIB_DNS_RESPONSE);
    reply[3] = 0x20;
    assert(mulib_dns_message_parse(reply, qn, 0, &r) == MULIB_DNS_OK);
    assert(mulib_dnsclient_validate_reply(&q, &r) == MULIB_DNS_UNSUPPORTED);
    reply[3] = 0; reply[2] |= 8;
    assert(mulib_dns_message_parse(reply, qn, 0, &r) == MULIB_DNS_OK);
    assert(mulib_dnsclient_validate_reply(&q, &r) == MULIB_DNS_UNSUPPORTED);
    memcpy(reply, query, qn); reply[2] = 0x80;
    assert(mulib_dns_message_parse(reply, qn, 0, &r) == MULIB_DNS_OK);
    assert(mulib_dnsclient_validate_reply(&q, &r) == MULIB_DNS_MISMATCH);
    memset(reply, 0, 12); reply[2] = 0x81; reply[1] = 42;
    assert(mulib_dns_message_parse(reply, 12, 0, &r) == MULIB_DNS_OK);
    assert(mulib_dnsclient_validate_reply(&q, &r) == MULIB_DNS_MISMATCH);
    assert(mulib_dnsclient_validate_query(&r, &question) == MULIB_DNS_UNSUPPORTED);
}

static int inplace_reply(void *user, const unsigned char *q, size_t qn,
    unsigned char *r, size_t cap, size_t *rn) {
    assert(qn <= cap);
    memmove(r, q, qn); r[2] |= 0x80;
    if (user) r[13] = 'z';
    *rn = qn; return MULIB_DNS_OK;
}
static void test_inplace_exchange(void) {
    unsigned char packet[512]; size_t qn, rn = 999;
    mulib_dns_message_t reply;
    memset(&reply, 0, sizeof(reply)); reply.length = 777;
    assert(mulib_dnsclient_query("example.org", 11, MULIB_DNS_A, 42, 1, packet, sizeof(packet), &qn) == MULIB_DNS_OK);
    assert(mulib_dnsclient_exchange(inplace_reply, packet, packet, qn, packet, sizeof(packet), &rn, &reply) == MULIB_DNS_MISMATCH);
    assert(rn == 999 && reply.length == 777);
    assert(mulib_dnsclient_query("example.org", 11, MULIB_DNS_A, 42, 1, packet, sizeof(packet), &qn) == MULIB_DNS_OK);
    assert(mulib_dnsclient_exchange(inplace_reply, NULL, packet, qn, packet, sizeof(packet), &rn, &reply) == MULIB_DNS_OK);
    assert(rn == qn && reply.data == packet);
}

static void test_stack(void) {
    unsigned char q[512], r[1024], a[4] = {192,0,2,7}, b[4] = {192,0,2,8}, a6[16] = {0x20,1,0xd,0xb8};
    mulib_dns_rr_t records[4]; mulib_nameserver_t server; mulib_dnsnmeserver_t alias;
    transport_state_t state; mulib_dnsresolver_t resolver; mulib_dnscache_t cache; mulib_dnscache_entry_t slots[2];
    mulib_dnscache_result_t result; mulib_dns_message_t query, reply; size_t qn, rn; unsigned calls;
    rr_init(&records[0], "www.example.org", MULIB_DNS_A, a, 4, 10);
    rr_init(&records[1], "www.example.org", MULIB_DNS_A, b, 4, 20);
    rr_init(&records[2], "www.example.org", MULIB_DNS_AAAA, a6, 16, 30);
    rr_init(&records[3], "alias.example.org", MULIB_DNS_CNAME, NULL, 0, 30); strcpy(records[3].target, records[0].name);
    assert(mulib_nameserver_init(&server, "example.org", 11, records, 4) == MULIB_DNS_OK);
    assert(mulib_dnsnmeserver_init(&alias, "example.org", 11, records, 4) == MULIB_DNS_OK);
    state.server = &server; state.calls = 0; state.mode = 0;
    assert(mulib_dnscache_init(&cache, slots, 2) == MULIB_DNS_OK);
    assert(mulib_dnsresolver_init(&resolver, transport, &state, &cache, 65535) == MULIB_DNS_OK);
    assert(mulib_dnsresolver_resolve(&resolver, "WWW.Example.ORG.", 16, MULIB_DNS_A, 100, q, sizeof(q), r, sizeof(r), &result) == MULIB_DNS_OK);
    assert(result.count == 2 && result.ttl == 10 && !memcmp(result.addresses[0], a, 4));
    assert(state.calls == 1 && resolver.next_id == 0);
    a[0] = 198;
    assert(mulib_dnsresolver_resolve(&resolver, "www.example.org", 15, MULIB_DNS_A, 105, NULL, 0, NULL, 0, &result) == MULIB_DNS_OK);
    assert(result.ttl == 5 && result.addresses[0][0] == 192 && state.calls == 1);
    assert(mulib_dnsresolver_resolve(&resolver, "www.example.org", 15, MULIB_DNS_A, 110, q, sizeof(q), r, sizeof(r), &result) == MULIB_DNS_OK);
    assert(state.calls == 2 && result.addresses[0][0] == 198);
    assert(mulib_dnsresolver_resolve(&resolver, "www.example.org", 15, MULIB_DNS_AAAA, 110, q, sizeof(q), r, sizeof(r), &result) == MULIB_DNS_OK);
    assert(result.count == 1 && result.type == MULIB_DNS_AAAA && !memcmp(result.addresses[0], a6, 16));
    assert(mulib_dnsresolver_resolve(&resolver, "alias.example.org", 17, MULIB_DNS_A, 110, q, sizeof(q), r, sizeof(r), &result) == MULIB_DNS_UNSUPPORTED);
    assert(mulib_dnsresolver_resolve(&resolver, "missing.example.org", 19, MULIB_DNS_A, 110, q, sizeof(q), r, sizeof(r), &result) == MULIB_DNS_NOTFOUND);
    assert(mulib_dnsresolver_resolve(&resolver, "elsewhere.org", 13, MULIB_DNS_A, 110, q, sizeof(q), r, sizeof(r), &result) == MULIB_DNS_RESPONSE);
    calls = state.calls;
    assert(mulib_dnsresolver_resolve(&resolver, "x", 1, MULIB_DNS_MX, 110, q, sizeof(q), r, sizeof(r), &result) == MULIB_DNS_UNSUPPORTED && state.calls == calls);
    assert(mulib_dnsclient_query("www.example.org", 15, MULIB_DNS_A, 7, 1, q, sizeof(q), &qn) == MULIB_DNS_OK);
    assert(mulib_dns_message_parse(q, qn, 0, &query) == MULIB_DNS_OK);
    assert(mulib_dnsnmeserver_answer(&alias, q, qn, r, sizeof(r), &rn) == MULIB_DNS_OK);
    assert(mulib_dns_message_parse(r, rn, 0, &reply) == MULIB_DNS_OK);
    assert(mulib_dnsclient_validate_reply(&query, &reply) == MULIB_DNS_OK && reply.header.counts[1] == 2);
    r[0] ^= 1;
    assert(mulib_dns_message_parse(r, rn, 0, &reply) == MULIB_DNS_OK);
    assert(mulib_dnsclient_validate_reply(&query, &reply) == MULIB_DNS_MISMATCH);
    assert(mulib_nameserver_answer(&server, q, qn, r, qn+16, &rn) == MULIB_DNS_OK);
    assert(mulib_dns_message_parse(r, rn, 0, &reply) == MULIB_DNS_OK && reply.header.counts[1] == 1);
    assert(mulib_dnsclient_validate_reply(&query, &reply) == MULIB_DNS_TRUNCATED);
    assert(mulib_nameserver_answer(&server, q, qn, r, qn-1, &rn) == MULIB_DNS_LIMIT);
    state.mode = 1;
    assert(mulib_dnsclient_exchange(transport, &state, q, qn, r, sizeof(r), &rn, &reply) == MULIB_DNS_TRANSPORT);
    state.mode = 2;
    assert(mulib_dnsclient_exchange(transport, &state, q, qn, r, sizeof(r), &rn, &reply) == MULIB_DNS_LIMIT);
    state.mode = 3;
    assert(mulib_dnsclient_exchange(transport, &state, q, qn, r, sizeof(r), &rn, &reply) == MULIB_DNS_MISMATCH);
    state.mode = 0;
    assert(mulib_dnsclient_query("example.org", 11, MULIB_DNS_A, 8, 0, q, sizeof(q), &qn) == MULIB_DNS_OK);
    assert(mulib_nameserver_answer(&server, q, qn, r, sizeof(r), &rn) == MULIB_DNS_OK);
    assert(mulib_dns_message_parse(r, rn, 0, &reply) == MULIB_DNS_OK && !(reply.header.flags & 15u) && !reply.header.counts[1]);
    assert(mulib_dnsclient_query("www.example.org", 15, MULIB_DNS_TXT, 8, 0, q, sizeof(q), &qn) == MULIB_DNS_OK);
    assert(mulib_nameserver_answer(&server, q, qn, r, sizeof(r), &rn) == MULIB_DNS_OK);
    assert(mulib_dns_message_parse(r, rn, 0, &reply) == MULIB_DNS_OK && !(reply.header.flags & 15u) && !reply.header.counts[1]);
    q[2] |= 0x80;
    assert(mulib_nameserver_answer(&server, q, qn, r, sizeof(r), &rn) == MULIB_DNS_UNSUPPORTED);
    strcpy(records[1].name, "alias.example.org");
    assert(mulib_nameserver_init(&server, "example.org", 11, records, 4) == MULIB_DNS_INVALID);
    strcpy(records[1].name, "notexample.org");
    assert(mulib_nameserver_init(&server, "example.org", 11, records, 4) == MULIB_DNS_INVALID);
}

static void test_address(void) {
    mulib_getaddr_address_t addresses[16]; mulib_inte6_t a, b; char text[40]; size_t n = 99;
    assert(mulib_getaddr_resolve("192.0.2.1", 443, 0, 0, addresses, 16, &n) == MULIB_GETADDR_OK);
    assert(n == 1 && addresses[0].family == 4 && addresses[0].port == 443 && addresses[0].bytes[3] == 1);
    assert(mulib_getaddr_format(addresses, text, sizeof(text)) == MULIB_GETADDR_OK && !strcmp(text, "192.0.2.1"));
    assert(mulib_getaddr_resolve("::1", 80, 6, 0, addresses, 16, &n) == MULIB_GETADDR_OK);
    assert(mulib_getaddr_format(addresses, text, sizeof(text)) == MULIB_GETADDR_OK && !strcmp(text, "::1"));
    assert(mulib_getaddr_resolve("::1", 80, 4, 0, addresses, 16, &n) == MULIB_GETADDR_NOTFOUND);
    assert(mulib_getaddr_resolve("::1", 80, 0, 0, NULL, 0, &n) == MULIB_GETADDR_LIMIT && n == 1);
    assert(mulib_getaddr_resolve("192.00.2.1", 80, 0, 0, addresses, 16, &n) == MULIB_GETADDR_INVALID);
    assert(mulib_getaddr_resolve("[::1]", 80, 0, 0, addresses, 16, &n) == MULIB_GETADDR_INVALID);
    assert(mulib_getaddr_resolve("fe80::1%1", 80, 0, 0, addresses, 16, &n) == MULIB_GETADDR_INVALID);
    assert(mulib_getaddr_resolve("localhost", 80, 0, 0, addresses, 16, &n) == MULIB_GETADDR_NOTFOUND);
#if MULIB_GETADDR_HAS_SYSTEM
    assert(mulib_getaddr_resolve("localhost", 80, 0, MULIB_GETADDR_SYSTEM, addresses, 16, &n) == MULIB_GETADDR_OK && n);
    assert(mulib_getaddr_resolve("localhost", 80, 0, MULIB_GETADDR_SYSTEM, NULL, 0, &n) == MULIB_GETADDR_LIMIT && n);
    assert(mulib_getaddr_resolve("0x7f000001", 80, 0, MULIB_GETADDR_SYSTEM, addresses, 16, &n) == MULIB_GETADDR_INVALID);
#else
    assert(mulib_getaddr_resolve("localhost", 80, 0, MULIB_GETADDR_SYSTEM, addresses, 16, &n) == MULIB_GETADDR_UNSUPPORTED);
#endif
    assert(mulib_inte6_parse("2001:db8::1", 11, &a));
    assert(mulib_inte6_format(&a, text, sizeof(text)) && !strcmp(text, "2001:db8::1"));
    assert(mulib_inte6_network(&a, 64, &b) && mulib_inte6_contains(&b, 64, &a));
    assert(!mulib_inet6_parse("1::2::3", 7, &a));
}

static void mutate_packet(const unsigned char *packet, size_t n) {
    unsigned char changed[1024], output[65535];
    assert(n <= sizeof(changed));
    for (size_t i = 0; i < n; ++i) for (unsigned c = 0; c < 256; ++c) {
        mulib_dns_message_t m, roundtrip; mulib_dns_iterator_t it; mulib_dns_question_t q;
        mulib_dns_rr_t rr; mulib_dns_writer_t w; unsigned sec; int rc;
        memcpy(changed, packet, n); changed[i] = (unsigned char)c;
        if (mulib_dns_message_parse(changed, n, 0, &m) != MULIB_DNS_OK) continue;
        assert(mulib_dns_writer_init(&w, output, sizeof(output), m.header.id, m.header.flags) == MULIB_DNS_OK);
        mulib_dns_iterator_init(&it, &m);
        while ((rc = mulib_dns_next_question(&it, &q)) == MULIB_DNS_OK) assert(mulib_dns_writer_question(&w, &q) == MULIB_DNS_OK);
        assert(rc == MULIB_DNS_END);
        while ((rc = mulib_dns_next_rr(&it, &rr, &sec)) == MULIB_DNS_OK) assert(mulib_dns_writer_rr(&w, sec, &rr) == MULIB_DNS_OK);
        assert(rc == MULIB_DNS_END && mulib_dns_message_parse(output, w.length, 0, &roundtrip) == MULIB_DNS_OK);
    }
}

static void test_mutations(void) {
    unsigned char packet[1024], a[4] = {192,0,2,1}, a6[16] = {0}, txt[] = {3,'a',0,'b'};
    size_t n; mulib_dns_writer_t w; mulib_dns_rr_t rr;
    mulib_dns_question_t q = {"www.example.org", MULIB_DNS_A, MULIB_DNS_IN};
    assert(mulib_dnsclient_query(q.name, 15, q.type, 42, 1, packet, sizeof(packet), &n) == MULIB_DNS_OK);
    mutate_packet(packet, n);
    assert(mulib_dns_writer_init(&w, packet, sizeof(packet), 42, MULIB_DNS_QR|MULIB_DNS_RD) == MULIB_DNS_OK);
    assert(mulib_dns_writer_question(&w, &q) == MULIB_DNS_OK);
    rr_init(&rr, q.name, MULIB_DNS_A, a, 4, 10);
    assert(mulib_dns_writer_rr(&w, 1, &rr) == MULIB_DNS_OK);
    rr.type = MULIB_DNS_AAAA; rr.data = a6; rr.data_length = 16;
    assert(mulib_dns_writer_rr(&w, 1, &rr) == MULIB_DNS_OK);
    rr.type = MULIB_DNS_CNAME; strcpy(rr.target, "alias.example.org");
    assert(mulib_dns_writer_rr(&w, 1, &rr) == MULIB_DNS_OK);
    rr.type = MULIB_DNS_MX; rr.preference = 10;
    assert(mulib_dns_writer_rr(&w, 1, &rr) == MULIB_DNS_OK);
    rr.type = MULIB_DNS_PTR;
    assert(mulib_dns_writer_rr(&w, 1, &rr) == MULIB_DNS_OK);
    rr.type = MULIB_DNS_NS;
    assert(mulib_dns_writer_rr(&w, 2, &rr) == MULIB_DNS_OK);
    rr.type = MULIB_DNS_TXT; rr.data = txt; rr.data_length = sizeof(txt);
    assert(mulib_dns_writer_rr(&w, 3, &rr) == MULIB_DNS_OK);
    mutate_packet(packet, w.length);
}

int main(void) {
    test_names(); test_codec(); test_cache(); test_limits(); test_reply_policy();
    test_stack(); test_address(); test_mutations(); test_inplace_exchange();
    puts("DNS stack tests passed"); return 0;
}
