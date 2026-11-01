# mulib-lazystream

Pipelines are limited to 256 stages including their source, bounding the recursive
`next` call chain. Constructing stage 257 returns NULL and leaves upstream ownership
with the caller. Destruction remains iterative and does not pull source values.

Define `MULIB_IMPL_LAZYSTREAM` in one translation unit. No module dependencies.

Streams are single-pass, pull-driven pipelines of borrowed `void *` values;
NULL is a valid element. They are not persistent sequences, memoized promises,
thread-safe objects or element owners.

Sources:

* `mulib_lazystream_source(next,context,cleanup)` wraps a generator. `next`
  returns 1 and writes an element, 0 for exhaustion, -1 for error. Cleanup is
  optional and runs once when the source is destroyed, not at exhaustion.
* `mulib_lazystream_array(values,count)` borrows an array and its elements.

Adapters allocate a stream, transferring ownership of upstream **only on
success**; failure returns NULL and leaves upstream untouched. Contexts remain
borrowed. Do not use an upstream after transferring it, share it between
pipelines, or destroy it separately.

* `map(upstream,map,context)`: callback writes a mapped pointer and returns 1;
  any other result fails the stream.
* `filter(upstream,predicate,context)`: 1 retains, 0 drops, anything else fails.
* `take(upstream,count)`: produces at most count elements, with no extra pull.
* `drop(upstream,count)`: skips the first count elements when first consumed.

`mulib_lazystream_next(stream,&value)` returns 1/0/-1 and clears value on end or
error. End/error are sticky: no further callbacks run. Pipeline construction
never pulls input. Infinite generators work with take; filters that never
accept and folds over infinite streams may not terminate.

`mulib_lazystream_fold(stream,accumulator,fold,context)` consumes the remaining
elements, passing each to a callback that mutates the accumulator. Nonzero
continues; zero aborts with -1. It returns 1 on exhaustion or -1 on stream/error
or callback abort. A fold callback abort consumes that element but does not
poison the stream, so iteration can resume. It does not destroy the pipeline.

`mulib_lazystream_destroy` iteratively destroys the pipeline and source context
cleanup; yielded elements remain caller-owned. Adapters pull upstream
recursively, so keep pipeline depth within your platform's call-stack budget.
Borrowed values may be valid only until the next pull, depending on the source.
