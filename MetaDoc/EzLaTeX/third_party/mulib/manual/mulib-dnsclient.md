# mulib-dnsclient

A synchronous, single-question DNS transaction layer over an application callback.
There is no implicit socket, global resolver configuration or background work.

## Integration and ownership

Define `MULIB_IMPL_DNSCLIENT` and its dependency `MULIB_IMPL_DNS` exactly once;
include `mulib-dnsclient.h`. Switches are never enabled by the header. All public
declarations have C linkage for C++ callers. No heap allocation or third-party
library is required by this layer.

`mulib_dnsclient_query(name,length,type,id,recursion,packet,capacity,&written)`
builds one IN question. Names follow the restricted ASCII rules in
[`mulib-dns`](mulib-dns.md). Supported question types are A, AAAA, NS, CNAME, PTR,
MX, TXT and ANY. The only optional query flag is RD. `id` is selected by the
application; this module does not supply randomness. `written` changes only on
success, but construction may modify the output buffer on failure.

## Query and reply validation

`validate_query(view,&question)` requires exactly one IN question, zero RRs,
opcode QUERY, and no flags except RD. A view must have been successfully parsed
by `mulib_dns_message_parse` and must still reference immutable valid storage.
Unsupported types/classes/flags return `MULIB_DNS_UNSUPPORTED`.

`validate_reply(query,reply)` requires QR, opcode QUERY, no reserved/AD/CD bits,
one question, and matching ID, canonical question name, type, class and echoed
RD. AA and RA are permitted. A mismatch returns `MISMATCH`; TC returns
`TRUNCATED`, including when some complete answers fit. RCODE 3 returns
`NOTFOUND`; any other nonzero RCODE returns `RESPONSE`. A valid empty NOERROR
response is `OK`; deciding whether it contains an address belongs to a resolver.
No assertion of authority, DNSSEC validation or authentic origin is made.

## Callback transactions

`exchange(transport,user,query,query_length,reply,reply_capacity,&reply_length,
&message)` parses and validates the query **before** invoking the callback.
The callback signature is:

```c
int transport(void *user, const unsigned char *query, size_t query_length,
              unsigned char *reply, size_t reply_capacity, size_t *reply_length);
```

Return `MULIB_DNS_OK` and set the number of bytes written, or return a negative
error. A callback returning zero is translated to `TRANSPORT`; other negative
statuses propagate. Reply capacity passed to it is capped at 65535. An oversized
reported length is rejected, but the callback itself must honor memory bounds.
The module parses the entire reply, then applies reply policy. It publishes
`reply_length` and the borrowed message view only on complete success. Reply
buffer contents are unspecified after any error. Query and reply buffers may
overlap if the transport can consume the query before overwriting it. The
expected header and question are copied before invoking the transport, so an
in-place reply cannot change what is matched. Callback state must outlive the
call. Nothing is retained.

The transport owns UDP peer/address checks, source ports, timeout/cancellation,
TCP framing, retry policy and any encryption. It must not silently substitute a
reply from a different peer. No automatic TCP retry follows TC. Sequential or
fixed transaction IDs are unsuitable for hostile untrusted networks without
additional transport protections.

## Limits and unsupported features

The wire codec limits apply: packet <=65535 bytes, total records <=1024, <=128
compression hops, and only A/AAAA/NS/CNAME/PTR/MX/TXT RRs. In particular, replies
with SOA negative proofs, OPT/EDNS or DNSSEC are rejected, not ignored. There is
no multi-question request, AXFR, DNS update, mDNS, IDNA or alias following.
The caller supplies all packet storage and synchronizes shared callback state.

## Example and tests

`examples/modules/dnsclient.c` implements an in-process echo-question transport,
checks a valid empty reply, then demonstrates ID-mismatch rejection. Build with
`cc -std=c11 -Iinclude examples/modules/dnsclient.c -o dnsclient`.
`tests/test_dns_stack.c` also tests authoritative replies, malformed flags,
truncation, callback failures and an oversized callback length.
