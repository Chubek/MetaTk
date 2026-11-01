# mulib-port

Platform identification and checked `size_t` arithmetic. ISO C11; no OS or
third-party dependency. Define `MULIB_IMPL_PORT` once.

`MULIB_PORT_WINDOWS`, `MULIB_PORT_POSIX`, and `MULIB_PORT_LINUX` are numeric
0/1 detection macros, not promises of specific operating-system facilities.
`mulib_port_os()` and `mulib_port_cpu()` return borrowed static names;
unknown targets return `"unknown"`. `mulib_port_little_endian()` inspects the
host's byte order.

`add_size`, `mul_size`, and `align_size` return 1 on success and 0 for overflow,
NULL output, or zero alignment. Output is unchanged on failure. Alignment may
be any positive integer, not only a power of two. `secure_zero` erases through
a volatile byte pointer; it is not a substitute for OS memory locking or a
guarantee that copies/registers of a secret have been erased.

## Example

```c
#define MULIB_IMPL_PORT
#include <mulib-port.h>
#include <assert.h>
int main(void) {
    size_t stride;
    assert(mulib_port_align_size(13, 8, &stride) && stride == 16);
    return 0;
}
```

Compile: `cc -std=c11 -Iinclude examples/modules/port.c -o port`.
