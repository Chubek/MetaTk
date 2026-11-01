# mulib-dns

Allocation-free DNS packet construction and validation, with bounded compression.
This is a focused wire codec, not a recursive resolver or a full DNS implementation.

## Integration

Define `MULIB_IMPL_DNS` in exactly one C11 translation unit and include
`mulib-dns.h`. There are no implementation dependencies, sockets or third-party
libraries. Other translation units include declarations normally; declarations
have C linkage in C++. Never define another module's implementation switch for
the application.

## Names and supported wire format

`mulib_dns_name_normalize(text,length,out)` accepts ASCII letters, decimal digits,
hyphens and underscores in labels of 1..63 bytes. Names may have a final dot;
canonical output is lowercase without that dot. The root is `"."`; an empty
string is invalid. Maximum canonical text length is 253 bytes and expanded wire
length, including the root octet, is 255 bytes. There are no escaped labels,
arbitrary binary labels, Unicode/IDNA conversion or wildcard syntax. Explicit
lengths reject embedded NUL bytes. Output arrays are always at least 256 bytes.

`name_encode` produces an uncompressed name. `name_decode(packet,length,offset,
out,&consumed)` accepts ordinary labels and backward 14-bit compression pointers.
`consumed` counts bytes at the original location, not the expanded suffix.
Forward/self pointers, truncation and oversized expansion fail. A traversal has
at most 128 pointer hops. The decoder structurally validates a pointer target;
it does not prove that a target was an earlier owner-label boundary. Unsupported
extended label types or binary label characters return `UNSUPPORTED`.

The codec handles one complete DNS payload, without TCP's two-byte length prefix.
Header fields use host byte order in C. Packet size is 12..65535 bytes and the
sum of all four header counts is at most 1024. `message_parse` accepts a smaller
`max_records`; zero selects 1024, and larger limits are capped at 1024. It rejects
trailing bytes, validates every section and publishes its view only on success.
It does not interpret QR/opcode/reserved flags; `mulib-dnsclient` validates those.

Supported RRs are A (exactly 4 bytes), AAAA (16), NS, CNAME, PTR, MX, and TXT.
NS/CNAME/PTR have exactly one encoded target name. MX has a 16-bit preference
and target. TXT is one or more length-prefixed byte strings, including empty
strings; its payload may contain NUL or non-ASCII bytes. An empty TXT RDATA is
invalid. All other RR types, including SOA, OPT/EDNS and DNSSEC, are rejected with
`UNSUPPORTED`, even in authority/additional sections. Unknown nonzero question
types/classes are structurally decoded; transaction policy is a separate layer.

## Message views and ownership

`mulib_dns_message_t` borrows the complete immutable packet. Initialize an iterator
with `iterator_init`, consume questions with `next_question`, then RRs with
`next_rr`. `END` is zero, `OK` is one, errors are negative. `next_rr` also reports
section 1 (answer), 2 (authority), or 3 (additional). Calling it before consuming
questions is invalid. The view, packet and iterator must remain valid together.
Views supplied to iterators/client policy functions must come from a successful
parse and must not be modified or have their source bytes changed.

Question/owner/target names are copied into fixed arrays. RR `data` borrows the
original RDATA, while `target` contains its expanded canonical name. To construct
or reserialize NS/CNAME/PTR/MX, use `target` (and MX `preference`); the writer
ignores their `data` fields. A/AAAA/TXT use `data`/`data_length` instead. Source
strings, RDATA and writer storage must not overlap. No destructor is necessary.

## Construction and errors

`writer_init` starts an empty packet. Append all questions first, then RRs in
nondecreasing section order with `writer_question`/`writer_rr`. The writer updates
header counts after each successful append. Its `length` is the complete current
packet length; there is no separate finish call. Capacity above 65535 is capped.
Its 128-entry suffix dictionary automatically emits backward pointers to the
longest known suffix; once full, it stops recording suffixes but still encodes
valid names. Compression is optional on the wire; root names stay uncompressed.

On an append failure, writer length, counts, dictionary and the committed packet
prefix remain unchanged. Bytes after the committed length are unspecified. Do
not mutate writer internals. `name_encode`, `name_decode`, normalization, parsing
and iterator output arguments remain unchanged on error. `rr_validate` checks
application-supplied records before encoding. `INVALID` denotes malformed data or
arguments; `LIMIT` denotes capacity/record/hop limits; `UNSUPPORTED` denotes a
feature outside this subset. Higher-level transaction errors share the enum.

## Example and verification

`cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/dns.c -o dns`
builds a runnable question/answer compression example. `tests/test_dns_stack.c`
covers each supported RR, byte-for-byte compressed roundtrips, truncated packets,
cyclic pointers, maximum names, count limits and deterministic malformed mutations.
