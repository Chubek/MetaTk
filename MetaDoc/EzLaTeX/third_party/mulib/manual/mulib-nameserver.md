# mulib-nameserver

A no-I/O authoritative answer engine for one static zone. It converts a query
packet and a borrowed record table into a DNS reply in caller storage.

## Integration and ownership

Define `MULIB_IMPL_NAMESERVER`, `MULIB_IMPL_DNSCLIENT`, and `MULIB_IMPL_DNS`
explicitly once; include `mulib-nameserver.h`. No implementation dependencies
are enabled automatically. Declarations have C linkage in C++. No allocation,
system networking calls or third-party libraries are used.

`init(&server,origin,length,records,count)` copies and canonicalizes the origin,
but **borrows** the record array and every A/AAAA/TXT RDATA buffer. They must
remain immutable and valid for the server's lifetime. There is no destructor.
NULL records are valid at count zero. Count is limited to 1023, leaving room
for one question within the codec's total-record limit.

Each record is a `mulib_dns_rr_t`: owner, IN class, type, TTL, and either
`data`/`data_length` (A/AAAA/TXT) or `target`/MX `preference` (named records).
`mulib_dns_rr_validate` rules apply. Owners must be at or below the zone origin,
matched on a label boundary, with case/final-dot normalization. All records
must be IN. CNAME owners must have exactly one record and may not coexist with
other types. Targets may be outside the zone. There is no implicit zone-file
parser or default SOA/NS record. Initialization leaves `server` unchanged on
failure. Validation includes a bounded O(count²) CNAME-conflict check.

## Answer behavior

`answer(&server,query,query_length,reply,reply_capacity,&written)` first fully
parses a single standard IN question. Only query flag RD is allowed; there is
no EDNS/additional section or multi-question support. Unsupported requests
return an error rather than synthesizing FORMERR/NOTIMP replies. Malformed
packets do not produce a reply. Inputs/output must be disjoint; nothing is
retained from a query.

Replies echo the transaction ID, question and RD, set QR, and never set RA.

* Outside the configured zone: RCODE REFUSED (5), without AA.
* Existing in-zone owner: AA, NOERROR, all matching records in table order.
* Existing owner with no matching type: AA, NOERROR, zero answers (NODATA).
* Absent in-zone owner: AA, NXDOMAIN (3), zero answers.

The apex and empty nonterminals implied by descendant records exist, even if
they have no explicit records. ANY returns every record at the owner. A CNAME
is returned for any supported type at its owner, without chasing its target.
Answers use the codec's bounded suffix compression. TTLs are emitted unchanged.

Capacity is capped at 65535, not automatically at UDP's traditional 512 bytes:
the application chooses the appropriate transport size. If header/question do
not fit, `LIMIT` is returned. If a later RR does not fit, previously committed
complete RRs remain and the engine returns a valid successful packet with TC;
there are no partial wire RRs. Clients should discard truncated RRsets or retry
over their own transport. `written` changes only on success; output bytes are
unspecified on failure. Concurrent answers are safe with immutable server data
and separate output buffers.

## Supported subset and errors

Records: A, AAAA, NS, CNAME, PTR, MX and TXT only. Names are restricted ASCII DNS
labels; see [`mulib-dns`](mulib-dns.md). `INVALID` covers malformed data, CNAME
conflicts and out-of-zone owners; `UNSUPPORTED` covers types/classes/features;
`LIMIT` covers count/capacity limits. `OK` is 1, failures are negative.

This is not a complete authoritative DNS service: no SOA/negative TTL proof,
zone transfers, updates, delegation cuts, glue, wildcard synthesis, recursion,
DNSSEC, EDNS, rate limiting, network daemon or transport authentication. NS
records are ordinary answers, not delegation instructions. Negative replies
have no SOA and should not be negatively cached using an inferred lifetime.
The caller owns serving policy and deployment security.

## Example and tests

`cc -std=c11 -Iinclude examples/modules/nameserver.c -o nameserver` builds a
query/answer and NXDOMAIN example. `tests/test_dns_stack.c` verifies resolver
composition, NODATA/apex behavior, ID echoing, truncation, REFUSED, CNAME rules,
out-of-zone rejection and the compatibility facade.
