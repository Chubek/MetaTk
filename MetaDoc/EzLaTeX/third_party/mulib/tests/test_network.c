#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#include "mulib-inet4.h"
#include "mulib-inte6.h"
#include <assert.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <arpa/inet.h>
#endif

static void ipv4(void) {
    static const char *const bad[] = { "", "1", "1.2.3", "1.2.3.4.5", "01.2.3.4", "1.2.3.256",
        "1.2.3.-1", "+1.2.3.4", "1..2.3", "1.2.3.4 ", " 1.2.3.4", "1.2.3.0x4", "1.2.3.000" };
    mulib_inet4_t a, b, net, sentinel = {{1,2,3,4}}; char text[16];
    assert(mulib_inet4_parse("192.0.2.255",11,&a));
    assert(mulib_inet4_format(&a,text,sizeof(text)) && !strcmp(text,"192.0.2.255"));
    assert(mulib_inet4_network(&a,24,&net) && net.bytes[3] == 0);
    assert(mulib_inet4_contains(&net,24,&a));
    assert(mulib_inet4_parse("192.0.3.0",9,&b));
    assert(!mulib_inet4_contains(&a,24,&b) && mulib_inet4_contains(&a,0,&b));
    assert(mulib_inet4_network(&a,32,&net) && !memcmp(&net,&a,sizeof(a)));
    net = sentinel; assert(!mulib_inet4_network(&a,33,&net) && !memcmp(&net,&sentinel,sizeof(net)));
    strcpy(text,"keep"); assert(!mulib_inet4_format(&a,text,4) && !strcmp(text,"keep"));
    for (size_t i = 0; i < sizeof(bad)/sizeof(*bad); ++i) {
        b = sentinel; assert(!mulib_inet4_parse(bad[i],strlen(bad[i]),&b)); assert(!memcmp(&b,&sentinel,sizeof(b)));
    }
    assert(!mulib_inet4_parse("1.2.3.4\0",8,&b));
    assert(!mulib_inet4_parse(NULL,0,&b));
}
static void ipv6(void) {
    static const char *const pairs[][2] = {
        {"::","::"}, {"0:0:0:0:0:0:0:1","::1"}, {"2001:DB8:0:1:0:0:0:1","2001:db8:0:1::1"},
        {"1:0:0:2:0:0:3:4","1::2:0:0:3:4"}, {"1:2:3:4:5:6:7:0","1:2:3:4:5:6:7:0"},
        {"1:2:3:4:5:6::","1:2:3:4:5:6::"}, {"::ffff:192.0.2.1","::ffff:c000:201"},
        {"1:2:3:4:5:6:192.0.2.1","1:2:3:4:5:6:c000:201"}
    };
    static const char *const bad[] = { "", ":", ":::1", "1:::2", "1::2::3", "1:2:3:4:5:6:7", "1:2:3:4:5:6:7:8:9",
        "1:2:3:4:5:6:7:8::", "::1:2:3:4:5:6:7:8", "12345::", "::g", "[::1]", "fe80::1%eth0",
        "1.2.3.4", "::ffff:192.00.2.1", "::ffff:192.0.2.256", "1:2:3:4:5:6:7:192.0.2.1", "::1:" };
    mulib_inet6_t a, b, net, sentinel = {{42}}; char text[40];
    for (size_t i = 0; i < sizeof(pairs)/sizeof(*pairs); ++i) {
        assert(mulib_inet6_parse(pairs[i][0],strlen(pairs[i][0]),&a));
        assert(mulib_inet6_format(&a,text,sizeof(text)) && !strcmp(text,pairs[i][1]));
        assert(mulib_inet6_parse(text,strlen(text),&b) && !memcmp(&a,&b,sizeof(a)));
#if defined(__unix__) || defined(__APPLE__)
        { unsigned char native[16]; assert(inet_pton(AF_INET6,pairs[i][0],native) == 1 && !memcmp(native,a.bytes,16)); }
#endif
    }
    for (size_t i = 0; i < sizeof(bad)/sizeof(*bad); ++i) {
        b = sentinel; assert(!mulib_inet6_parse(bad[i],strlen(bad[i]),&b) && !memcmp(&b,&sentinel,sizeof(b)));
    }
    assert(mulib_inet6_parse("2001:db8::abcd",14,&a));
    assert(mulib_inet6_network(&a,64,&net) && mulib_inet6_contains(&net,64,&a));
    assert(mulib_inet6_contains(&net,0,&a));
    assert(mulib_inet6_network(&a,128,&net) && !memcmp(&net,&a,sizeof(a)));
    assert(!mulib_inet6_network(&a,129,&net));
    strcpy(text,"keep"); assert(!mulib_inet6_format(&a,text,4) && !strcmp(text,"keep"));
    for (unsigned prefix = 0; prefix <= 128; ++prefix) {
        assert(mulib_inet6_network(&a,prefix,&net));
        for (unsigned bit = 0; bit < 128; ++bit) {
            unsigned mask = 128u >> (bit%8);
            assert((net.bytes[bit/8] & mask) == (bit < prefix ? a.bytes[bit/8] & mask : 0));
        }
    }
    for (unsigned i = 0; i < 2000; ++i) {
        for (unsigned j = 0; j < 16; ++j) a.bytes[j] = (unsigned char)(i*37+j*61+(i >> (j%8)));
        if (i % 3 == 0) memset(a.bytes+4,0,8);
        assert(mulib_inet6_format(&a,text,sizeof(text)));
        assert(mulib_inet6_parse(text,strlen(text),&b) && !memcmp(&a,&b,sizeof(a)));
#if defined(__unix__) || defined(__APPLE__)
        { unsigned char native[16]; assert(inet_pton(AF_INET6,text,native) == 1 && !memcmp(native,a.bytes,16)); }
#endif
    }
}
int main(void) { ipv4(); ipv6(); return 0; }
