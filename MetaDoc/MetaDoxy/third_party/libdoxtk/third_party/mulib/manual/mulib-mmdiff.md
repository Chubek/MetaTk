# MuLIB binary memory diff

- **Header:** `include/mulib-mmdiff.h`
- **Implementation switches:** `MULIB_IMPL_MMDIFF`, **`MULIB_IMPL_VEC`**,
  **`MULIB_IMPL_BUFFER`**
- **Dependencies:** MuLIB vec and binary buffer only; ISO C11, C++17 declarations.

## Scope: memory means binary buffers

This is a binary-memory differencer producing verified COPY and INSERT commands
and applying them safely. It has **no claimed relationship to an undefined
external memtkx API, format or semantics**. It is not a process-memory inspector,
memory-mapping API, structural object differencer, or VCDIFF/bsdiff implementation.
All byte values, embedded zeroes and empty buffers are supported.

The generator indexes every source window of `min_match` bytes using a 64-bit
polynomial rolling hash and a chained hash table of offsets. At each target
position, candidate source offsets are compared byte-for-byte and the longest
match of at least `min_match` bytes becomes a COPY; otherwise that byte is inserted.
Compatible adjacent commands are coalesced. COPY offsets always refer to the
**original** source, so commands may reorder or repeat source bytes. Hash
collisions cannot make an incorrect COPY. Hashes are not cryptographic.

This is greedy, not an optimal/minimal patch algorithm. Highly repetitive input
or collisions can cause many candidate comparisons; explicit work/index caps
return LIMIT rather than permit unbounded work. Source hash construction uses
rolling updates; each inspected target window is hashed afresh. Initial indexing
is O(source length), additional matching work is data-dependent and capped.
Index memory is O(number of source windows), plus source/literal/command storage.

## Limits and ownership

```c
typedef struct mulib_mmdiff_limits {
    size_t min_match, max_bytes, max_index_entries, max_work;
} mulib_mmdiff_limits_t;
typedef struct mulib_mmdiff_command {
    mulib_mmdiff_kind_t kind;
    size_t offset, length;
} mulib_mmdiff_command_t;
typedef struct mulib_mmdiff_patch {
    mulib_vec_t commands;
    mulib_buffer_t source, inserts;
    size_t target_length;
} mulib_mmdiff_patch_t;
```

NULL options select `min_match=8`, 16 MiB per input, 1,000,000 source windows and
64,000,000 work units. `min_match` must be positive. Every other zero option is a
real limit, not “unlimited.” Work units count initial hash-window byte reads,
source rolling updates, target window byte reads, candidate visits and candidate
byte comparisons. They do not count output/source copying or the linear
hash-table initialization/index-chain reversal. No index is needed when either buffer is shorter
than `min_match`; a zero work/index cap can therefore still allow pure insertion.
Index/table allocation arithmetic and hash-table power-of-two growth are checked.

Initialize patches and buffers before use; free/reset them afterward. Do not
shallow-copy owning objects. COPY addresses the patch's `source` snapshot;
INSERT addresses `inserts`. This **full exact source snapshot** allows reliable
wrong-base rejection, at the cost of not guaranteeing a compact stored patch.
The representation is an in-memory C object, **not a wire/file format**. Never
cast untrusted bytes into these owning structures.

Define all three implementation switches before any includes in one C
translation unit. Dependencies are normally included but their implementation
switches are never implicitly defined. There are no external libraries/OS APIs.

## API, failure and application

```c
void mulib_mmdiff_init(mulib_mmdiff_patch_t *patch);
void mulib_mmdiff_free(mulib_mmdiff_patch_t *patch);
mulib_mmdiff_status_t mulib_mmdiff_make(
    const void *old_data, size_t old_length,
    const void *new_data, size_t new_length,
    const mulib_mmdiff_limits_t *limits, mulib_mmdiff_patch_t *out);
mulib_mmdiff_status_t mulib_mmdiff_apply(
    const mulib_mmdiff_patch_t *patch, const void *base, size_t base_length,
    size_t max_output, mulib_buffer_t *out);
```

Status codes are `MULIB_MMDIFF_OK` (zero), `MULIB_MMDIFF_INVALID`,
`MULIB_MMDIFF_LIMIT`, and `MULIB_MMDIFF_NOMEM`; no `errno` promise. NULL bytes
require zero length. NULL/already-reset free is safe. Objects are not internally
locked; concurrent mutation requires external synchronization.

Make and apply leave their initialized outputs unchanged on every error.
`apply` checks exact source bytes, metadata, command kinds, nonzero spans,
COPY/INSERT offset bounds, output arithmetic and the total advertised target
length before allocating output. An explicit `max_output` cap prevents oversized
results. It rejects invalid numeric spans but cannot validate forged/dangling
C pointers; all pointers must refer to real live allocations of the stated size.

Application builds a separate result, then replaces output, so base may alias
`out->data`. Output must not be either buffer member of the patch. COPY never
reads partially generated output. No string terminator is appended.

## Runnable example

```c
#include <assert.h>
#include <string.h>
#define MULIB_IMPL_VEC
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_MMDIFF
#include "mulib-mmdiff.h"
int main(void) {
    const char a[] = "abcdefgh12345678";
    const char b[] = "12345678!abcdefgh";
    mulib_mmdiff_patch_t patch;
    mulib_buffer_t result;
    mulib_mmdiff_init(&patch);
    mulib_buffer_init(&result);
    assert(mulib_mmdiff_make(a, sizeof(a)-1, b, sizeof(b)-1, NULL, &patch)
           == MULIB_MMDIFF_OK);
    assert(patch.commands.length == 3); /* COPY, INSERT, COPY */
    assert(mulib_mmdiff_apply(&patch, a, sizeof(a)-1, 100, &result)
           == MULIB_MMDIFF_OK);
    assert(result.length == sizeof(b)-1 && !memcmp(result.data,b,sizeof(b)-1));
    mulib_mmdiff_free(&patch);
    mulib_buffer_free(&result);
    return 0;
}
```

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/mmdiff.c -o ./mmdiff
./mmdiff
```

`tests/test_indexes.c` checks randomized binary forward/reverse roundtrips,
empty/repetitive/reordered/duplicated sources, concrete COPY generation,
wrong-base rejection, malformed commands/metadata, input/output/work/index
limits, alias-safe application and injected allocation failures.
