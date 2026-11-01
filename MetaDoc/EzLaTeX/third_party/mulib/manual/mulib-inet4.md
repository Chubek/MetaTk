# mulib-inet4

Strict, length-aware IPv4 literals and network-prefix arithmetic. No sockets,
DNS, hostnames, ports, CIDR text, legacy octal/hex or integer-address syntax.

## Integration and grammar

Define `MULIB_IMPL_PARSEC` and `MULIB_IMPL_INET4` before inclusion in one C11
translation unit. Parsing uses MuLIB combinators to capture exactly four octets:
`digit{1,3} ('.' digit{1,3}){3}` followed by end of input. Semantic checks reject
values above 255 and leading zeroes in multi-digit octets. Input is 7..15 bytes;
whitespace, signs, NUL and trailing data are rejected.

## API and ownership

`mulib_inet4_t` is four copyable bytes in network order, not a native-endian
integer. `parse(text,length,&address)` and `format(&address,text,capacity)` return
1 on success, 0 on invalid input/insufficient storage/parser allocation failure.
Outputs are unchanged on failure. A 16-byte formatting buffer always suffices.
Parsing borrows input only during the call; captures are released internally.

`network(&address,prefix,&out)` clears host bits for prefix 0..32 and permits
in-place use. `contains(&network,prefix,&address)` ignores the network's host
bits. Prefix zero matches every address; invalid arguments return zero.

## Example and tests

See [`examples/modules/inet4.c`](../examples/modules/inet4.c):
`cc -std=c11 -Iinclude examples/modules/inet4.c -o inet4`.
`tests/test_network.c` covers malformed syntax, unchanged outputs and boundaries.
