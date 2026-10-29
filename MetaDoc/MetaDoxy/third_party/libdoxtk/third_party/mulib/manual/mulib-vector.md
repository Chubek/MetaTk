# mulib-vector

Arbitrary-dimensional numerical vectors of doubles, using `mulib-vec` for
storage. This module provides algebra; `mulib-vec` remains the generic dynamic
container. Define `MULIB_IMPL_VECTOR` and `MULIB_IMPL_VEC` once. Link `-lm` on
systems with a separate math library.

## API and semantics

`init(v,n)` initializes a **fresh** object to n zero coordinates; `from(v,a,n)`
copies coordinates into a fresh object. Both return 0 on failure and leave an
initialized empty object. Do not initialize an already owning object without
first freeing it. `free` releases storage and leaves an empty reusable vector.
The public `elements` field should not be modified directly.

`size`, `data`, `get`, and `set` inspect or update coordinates. The `data`
pointer is borrowed and invalidated by replacement/free. Invalid indices or
arguments return 0 without changing output parameters.

`copy(out,v)`, `add(out,a,b)`, `sub(out,a,b)`, and `cross(out,a,b)` replace an
**initialized** output transactionally, allowing out to alias any input. Cross
product requires two 3D inputs. Addition/subtraction/dot/axpy require equal
dimensions. Failure leaves initialized output unchanged.

`scale(v,s)` and `axpy(dst,s,src)` update in place. `dot(a,b,&result)` uses a
long-double accumulator, not exact arithmetic. `norm(v)` accumulates with hypot
to avoid unnecessary intermediate squaring overflow. `normalize(v)` divides
by the norm only if it is finite and nonzero, otherwise leaves v unchanged.
Empty vectors have zero norm and dot product. Other arithmetic follows IEEE
floating-point behavior, including NaN/Infinity; no finite-only guarantee is
made. No synchronization is provided for shared mutable vectors.

## Example

```c
#define MULIB_IMPL_VEC
#define MULIB_IMPL_VECTOR
#include <mulib-vector.h>
#include <assert.h>
int main(void) {
    const double xy[] = {3, 4};
    mulib_vector_t v;
    assert(mulib_vector_from(&v, xy, 2));
    assert(mulib_vector_norm(&v) == 5);
    assert(mulib_vector_normalize(&v));
    mulib_vector_free(&v);
    return 0;
}
```

Runnable 3D example: `examples/modules/vector.c`.
