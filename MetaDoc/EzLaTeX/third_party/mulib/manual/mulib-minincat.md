# mulib-minincat

`mulib-minincat.h` is an allocation-free, one-way blocking binary stream relay.
Explicit read/write callbacks can wrap files, pipes, sockets or memory. It has
no network or HTTP dependency and is not a netcat command-line replacement.

## Integration

Define only `MULIB_IMPL_MININCAT` in one C11 translation unit and include the
header. No transitive implementation switches are required. C++ declaration
inclusion uses C linkage.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/minincat.c -o /tmp/opencode/minincat
/tmp/opencode/minincat
```

The runnable example relays embedded-NUL bytes using a two-byte scratch buffer
and a writer that accepts just one byte per call.

## API and IO contracts

`mulib_minincat_relay(read_fn, reader, write_fn, writer, buffer, capacity,
max_bytes, transferred)` borrows all pointers. A non-NULL scratch buffer,
positive capacity, both functions and non-NULL `transferred` are required.
Reader and writer contexts can be NULL if the callbacks permit it.

* Read callback receives `(reader, buffer, requested_capacity, count)`. It must
  write no more than that capacity, return exactly 1 on success and set count.
  Zero count means EOF. Any other return value is IO failure. A reported count
  exceeding capacity is rejected (but cannot undo an actual buffer overrun).
* Write callback receives `(writer, data, remaining_length, count)`. It must
  return 1 and report positive progress not exceeding remaining_length. Short
  writes are retried until the read chunk is drained. Zero progress, oversized
  count or another return code is IO failure. Inputs are borrowed only until
  callback return. Do not retain the scratch pointer.

The scratch area and output counter must not overlap the callback state they
overwrite. The reader must not overwrite unread data in an overlapping source.
Callbacks own handles and all opening/closing, flushing, timeout, retry-on-EINTR
and would-block behavior. There is no scheduler, polling or asynchronous API.

## Completion and limits

* `MULIB_MININCAT_OK` (1): the reader reported EOF before the byte budget.
* `MULIB_MININCAT_LIMIT` (2): exactly max_bytes have been written; this is a
  deliberate bounded stop, not an IO error.
* `MULIB_MININCAT_INVALID` (-1): invalid arguments.
* `MULIB_MININCAT_IO` (-2): read/write callback contract or IO failure.

`*transferred` starts at zero and is updated after each successful partial write,
including when a later operation fails. Counts are bounded by max_bytes; size_t
arithmetic cannot wrap. The reader is never asked for more than the remaining
budget. At the budget, no lookahead read is performed: EOF exactly at max_bytes
still returns LIMIT. A zero budget returns LIMIT with no IO. An IO failure can
leave a read chunk partly written; unread/unwritten portions are not retained
for resumption and handles are not closed.

No allocations, implicit terminators, text conversions, extra read probes or
TLS operations occur. Full duplex requires application scheduling of two
directions; calling two blocking relays sequentially is not a duplex pump.
Never use a blocking callback without an appropriate application timeout when
its peer is untrusted.

`tests/test_web_stack.c` exercises binary EOF relay, short writes, exact/zero
budget, invalid callbacks, oversized read counts and zero-progress writes.
