# mulib-rpc

Synchronous JSON-RPC 2.0 **server dispatch**, independent of HTTP, sockets,
threads, and process state. Includes single requests, batches, notifications,
IDs, parse/validation/method/handler errors. It is not a transport or an RPC
client with outstanding-call tracking.

## Integration

Define `MULIB_IMPL_RPC`, `MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`,
`MULIB_IMPL_STRBUF`, and `MULIB_IMPL_UTF8` before inclusion in one translation
unit. Uses the public JSON DOM; no external library or private JSON internals.

## Methods and ownership

Pass a borrowed array of `mulib_rpc_method_t {name, handler, user}`. Names must
be unique, non-NULL, and not begin with the reserved `rpc.` prefix. Empty method
names are legal. Registrations and user pointers must remain valid throughout
the call. No global registry exists; separate calls are independent except for
your callback state.

The handler receives borrowed `params`, NULL if absent, and an initially NULL
result pointer. Return 0 with a newly owned JSON result (NULL means JSON null).
Return an integer JSON-RPC error code otherwise; an optional owned result then
becomes `error.data`. Ownership of that result always transfers to dispatch,
even for notifications or on later failure. Do not return borrowed params.
The dispatcher supplies standard error messages, or `Server error` for custom
codes. Parameter schema checking belongs to the handler (return -32602).

## API and behavior

- `mulib_rpc_dispatch(methods,count,request,max_batch,&response)` borrows the DOM
  request and returns 1 with an owned response, or NULL for notifications only.
  Destroy a response with `mulib_json_destroy`.
- `mulib_rpc_process(methods,count,bytes,length,max_batch,&text)` parses exactly
  the supplied UTF-8 JSON bytes, dispatches, and returns malloc-owned compact
  JSON text (free with `free`), or NULL for notifications only.
- Both return 0 on local failure/invalid registry and leave output unchanged.
  Callbacks can already have executed if a later allocation fails; dispatch is
  not transactional. Do not blindly retry non-idempotent operations.

Requests require `jsonrpc:"2.0"` and a string method. Optional params must be an
array or object. Unknown members are ignored. IDs may be string (including
embedded NUL), number, or null; absence alone means notification. ID bytes/types
are preserved through the DOM. Numeric fidelity is that of the JSON DOM's
double representation, not arbitrary-precision integers; strings are preferable
for very large IDs. Fractional numeric IDs are accepted, though discouraged by
JSON-RPC. Embedded NUL method names never accidentally match a shorter C name.

Invalid request objects/nonobjects yield -32600 with null ID. Valid unknown
methods yield -32601 with the supplied ID. Valid notifications execute handlers
but never produce responses, even for unknown methods or handler errors.
Invalid notification-shaped objects still produce -32600. Malformed JSON,
partial JSON, and trailing non-whitespace input yield -32700 with null ID.
The JSON parser does not expose a separate allocation-failure status, so a
failed source parse is reported as -32700 if an error response can be allocated.

Batch responses preserve request order, omit valid notifications, and return
NULL for an all-notification batch. An empty batch yields one -32600 object,
not an empty array. `max_batch==0` selects 64. Oversized batches yield one -32000
`Batch limit exceeded` object **without invoking any handler**. No nested-batch
dispatch occurs; a nested array is an invalid request element.

## Runnable example

[`examples/modules/rpc.c`](../examples/modules/rpc.c) registers `add`, dispatches
an exact byte-length JSON request, and checks the result through the DOM:

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/rpc.c -o ./rpc
./rpc
```
