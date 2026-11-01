# MuLIB line text diff

- **Header:** `include/mulib-txtdiff.h`
- **Implementation switches:** `MULIB_IMPL_TXTDIFF`, **`MULIB_IMPL_VEC`**,
  **`MULIB_IMPL_BUFFER`**
- **Dependencies:** MuLIB vec and binary buffer; ISO C11, C++17 declarations.

## Scope and algorithm

Exact byte-preserving, line-based diff and safe application of an owning,
in-memory patch. A line includes its terminating LF if present. Empty input has
zero lines; a final LF does not create a spurious empty line. CR, NUL and all
other bytes are ordinary content, so CRLF and a missing final LF are preserved
and distinguished. No Unicode normalization or text decoding occurs.

The generator builds a full longest-common-subsequence table over lines and
produces KEEP, DELETE and INSERT spans. This minimizes the number of inserted
plus deleted **lines**, not edit-byte count. Ties prefer deletion. Adjacent
compatible spans are coalesced. This is not a unified/context diff parser,
three-way merger, fuzzy patcher, command-line program, or external patch format.

For n/m lines, table space and table iterations are O((n+1)(m+1)); each line
comparison also compares bytes. Total time additionally includes those byte
comparisons and copying. Default limits cap each input at 16 MiB and the table
at 1,000,000 cells; a cell is `size_t`. These bounds deliberately reject overly
large quadratic jobs rather than pretending an unbounded algorithm is safe.

## Data and ownership

```c
typedef struct mulib_txtdiff_limits {
    size_t max_bytes, max_cells;
} mulib_txtdiff_limits_t;
typedef struct mulib_txtdiff_edit {
    mulib_txtdiff_kind_t kind;
    size_t offset, length;
} mulib_txtdiff_edit_t;
typedef struct mulib_txtdiff_patch {
    mulib_vec_t edits;
    mulib_buffer_t source, inserts;
    size_t target_length;
} mulib_txtdiff_patch_t;
```

Initialize/free patches using the API; initialize result buffers with
`mulib_buffer_init`. Never shallow-copy either owning object. The patch owns
an **exact source snapshot**, inserted bytes and the edit vector. KEEP/DELETE
offsets address `source`; INSERT offsets address `inserts`. All generated edits
are nonempty spans of complete lines. The source copy makes wrong-base rejection
exact, not dependent on a collision-prone hash, but means patches are not
necessarily storage-efficient. They are not serialized and must not be loaded
by treating untrusted file bytes as C structures/pointers.

Each implementation switch must be explicitly defined in one translation unit
before including dependency headers. No dependency's switch is implicitly
enabled. All memory uses standard-library allocation; no OS/external libraries.

## API and errors

```c
void mulib_txtdiff_init(mulib_txtdiff_patch_t *patch);
void mulib_txtdiff_free(mulib_txtdiff_patch_t *patch);
mulib_txtdiff_status_t mulib_txtdiff_make(
    const void *old_data, size_t old_length,
    const void *new_data, size_t new_length,
    const mulib_txtdiff_limits_t *limits, mulib_txtdiff_patch_t *out);
mulib_txtdiff_status_t mulib_txtdiff_apply(
    const mulib_txtdiff_patch_t *patch, const void *base, size_t base_length,
    size_t max_output, mulib_buffer_t *out);
```

Status is `MULIB_TXTDIFF_OK` (zero), `MULIB_TXTDIFF_INVALID`,
`MULIB_TXTDIFF_LIMIT`, or `MULIB_TXTDIFF_NOMEM`; there is no `errno` contract.
NULL input data is valid only with length zero. A NULL limits pointer uses the
defaults; explicit zero fields are actual limits. `max_cells` includes the
empty sentinel row and column, so even two empty inputs require one cell.
Allocation-size arithmetic is checked before table/vector allocation.

Both make and apply are **transactional**: an initialized output is replaced only
after success and is unchanged on all errors. `apply` checks the exact base,
buffer/vector metadata, edit kinds, nonzero lengths, bounded offsets, contiguous
KEEP/DELETE consumption of every source byte, output-length arithmetic and the
advertised target length. Its explicit `max_output` cap is checked before output
allocation. KEEP/DELETE spans must start and end on source line boundaries;
INSERT spans must start and end on literal-buffer line boundaries. A span may
include an unterminated final line only at the end of its source/literal buffer.
If KEEP or INSERT emits such a line, no later edit may emit more bytes (later
DELETE edits are allowed). Thus changing a final newline requires replacing the
whole affected line, not splitting off its newline. All validation precedes
construction, and malformed scripts preserve the output. As with any C API,
allocated pointers must actually be valid: numeric validation cannot make forged
or dangling pointers safe.

`out` may already contain data and the base may alias `out->data`; construction
uses a separate buffer before releasing old output. `out` must not be
`&patch->source` or `&patch->inserts`. Results have an explicit byte length, no
appended NUL. NULL/already-reset patch free is safe. No concurrent mutation.

## Runnable example

```c
#include <assert.h>
#include <string.h>
#define MULIB_IMPL_VEC
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_TXTDIFF
#include "mulib-txtdiff.h"
int main(void) {
    const char a[] = "one\ntwo\nlast";
    const char b[] = "one\nnew\ntwo\nlast\n";
    mulib_txtdiff_patch_t patch;
    mulib_buffer_t result;
    mulib_txtdiff_init(&patch);
    mulib_buffer_init(&result);
    assert(mulib_txtdiff_make(a, sizeof(a)-1, b, sizeof(b)-1, NULL, &patch)
           == MULIB_TXTDIFF_OK);
    assert(mulib_txtdiff_apply(&patch, a, sizeof(a)-1, 1024, &result)
           == MULIB_TXTDIFF_OK);
    assert(result.length == sizeof(b)-1 && !memcmp(result.data,b,sizeof(b)-1));
    mulib_txtdiff_free(&patch);
    mulib_buffer_free(&result);
    return 0;
}
```

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/txtdiff.c -o ./txtdiff
./txtdiff
```

`tests/test_indexes.c` independently checks minimal line edit counts and random
forward/reverse roundtrips (including more than 64 lines, NUL/CRLF/missing final
LF), empty inputs, wrong bases, limits, malformed edit spans/metadata, split-line
scripts and output after an unterminated line, alias-safe application and
allocation-failure preservation.
