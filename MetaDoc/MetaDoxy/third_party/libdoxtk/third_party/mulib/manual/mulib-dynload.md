# MuLIB dynload: native shared libraries

**Header:** `include/mulib-dynload.h`  
**Implementation switch:** `MULIB_IMPL_DYNLOAD`  
**MuLIB dependencies:** none. **Link:** `-ldl` on Linux/other libdl systems;
macOS uses its system loader, Windows its standard system libraries.

## Scope

POSIX builds use `dlopen(RTLD_NOW | RTLD_LOCAL)`, `dlsym`, and `dlclose`. Windows
builds use `LoadLibraryA`, `GetModuleHandleA`, `GetProcAddress`, and `FreeLibrary`.
Other platforms explicitly return `ENOTSUP`. Windows paths use the native ANSI
API encoding, not a promised UTF-8 conversion. This API does not implement a
plugin protocol, dependency isolation, custom search paths, lazy loading, or a
sandbox. Library constructors/destructors execute native code. Only load trusted
libraries; prefer explicit paths and understand the operating system's loader
search policy.

System headers are implementation-only, and no global feature-test macros are
defined. The declaration-only header is usable from C11 or C++17.

## API

```c
mulib_dynload_t library = MULIB_DYNLOAD_INIT;
int mulib_dynload_open(mulib_dynload_t *, const char *path, char *error, size_t capacity);
int mulib_dynload_symbol(const mulib_dynload_t *, const char *name, void **symbol,
                        char *error, size_t capacity);
int mulib_dynload_close(mulib_dynload_t *, char *error, size_t capacity);
```

Initialize each handle and do not copy it while live. Passing NULL as the path
opens the current process. POSIX owns that loader reference; Windows borrows the
main-module reference and does not pass it to `FreeLibrary`. Named Windows
modules are owned normally. Open fails with `ESTATE` if already open. Failure
leaves the handle unchanged.

Symbol lookup writes a borrowed native address on success only. POSIX's `dlerror`
protocol distinguishes a successful NULL-valued symbol from a failed lookup, so
use the return status, not merely `*symbol`. A symbol must not be used after the
owning library closes. Export visibility and symbol spelling are native-loader
concerns; opening the process does not export every private executable symbol.

ISO C does not guarantee object/function pointer interchangeability. To call a
function symbol use your platform's documented ABI conversion (commonly `memcpy`
between equal-sized pointer objects on POSIX), with exactly the exported function
type. Do not use a wrong prototype or assume all function pointers have the same
representation. The Windows implementation checks representational size before
copying `FARPROC` into the API's `void *` output.

Close is idempotent. A native close failure retains the handle for retry. Do not
close concurrently with lookups or calls through its symbols. No global mutable
MuLIB error buffer or process-global handle cache is maintained.

## Errors

All APIs return zero on success and negative `EINVAL`, `EIO` (native loader
failure), `ENOTSUP`, or `ESTATE`. On every call an optional caller-owned error
buffer receives a terminated message, truncated to capacity minus one; success
clears it. NULL buffers or zero capacity are allowed. Capacity one produces only
a NUL. Error buffers must not overlap input strings, output symbols, or handle
storage. POSIX native messages are copied immediately; Windows messages include
the native error number. There is no promised `errno` contract.

## Runnable example

```c
#define MULIB_IMPL_DYNLOAD
#include "mulib-dynload.h"
#include <assert.h>
int main(void) {
    mulib_dynload_t d = MULIB_DYNLOAD_INIT;
    char error[256];
    void *address = NULL;
    assert(mulib_dynload_open(&d, NULL, error, sizeof(error)) == 0);
    assert(mulib_dynload_symbol(&d, "malloc", &address, error, sizeof(error)) == 0);
    assert(address);
    assert(mulib_dynload_close(&d, error, sizeof(error)) == 0);
    return 0;
}
```

This inline example targets a dynamically linked POSIX process. The full
`examples/modules/dynload.c` also selects a Windows system library when relevant.
On Linux build/run with
`cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/dynload.c -ldl -o dynload-example`
then `./dynload-example`. Storage tests cover native failures, unchanged symbol
outputs, current-process lookup, tiny error buffers, state, and repeated close.
