# mulib-dyncall

A small **exactly typed native C invocation** API. It deliberately avoids a
generic untyped function pointer, signature string, argument byte stack,
function-pointer casts, or arbitrary ABI dispatcher. No libffi dependency.

## Integration and supported signatures

Define `MULIB_IMPL_DYNCALL` once in a C11 TU; no other module is required.
Declarations support C++17 and have C linkage. The six supported signatures
are exactly:

```c
int64_t function(void);
int64_t function(int64_t);
int64_t function(int64_t, int64_t);
double  function(void);
double  function(double);
double  function(double, double);
```

Corresponding pointer typedefs are `mulib_dyncall_i64_0_fn` through `i64_2_fn`
and `mulib_dyncall_f64_0_fn` through `f64_2_fn`. Pass one to its matching
`mulib_dyncall_i64_0/1/2` or `mulib_dyncall_f64_0/1/2` call, followed by the
typed arguments and a pointer to the result. For example:

```c
int64_t answer;
/* add must really be declared int64_t add(int64_t, int64_t). */
int status = mulib_dyncall_i64_2(add, 20, 22, &answer);
```

These are ordinary compiler-generated native C calls; the compiler supplies
the host's calling convention. There is **no runtime type conversion**. A
function declared `int f(int,int)` is not compatible just because it happens
to return a small integer. Variadic functions, mixed integer/float arguments,
structures, pointers as integer arguments, foreign conventions, and additional
arity are not supported. Never cast an incompatible pointer to bypass the
type system; the library cannot validate a caller-forged pointer.

## Ownership and failure rules

Pointers are borrowed for the duration of the synchronous call. The caller
keeps the function's code/shared library and result storage alive, and provides
any synchronization needed by the called function. No function pointers or
arguments are stored; there is no allocation or mutable global state.

Zero means the function was called once and its result stored. A NULL function
or NULL output returns `-1`, without calling anything or changing output.
The API does not catch exceptions, longjmp, signals, traps, arithmetic errors,
or application-level failures in the called function. There are no platform
backends: supported signatures work wherever the C types exist.

`mulib-jit` uses `i64_0` after its separate, explicitly platform-specific
executable-address conversion. No conversion is performed by dyncall itself.

## Runnable example

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  examples/modules/dyncall.c -o /tmp/opencode/mulib-dyncall
/tmp/opencode/mulib-dyncall
```

`tests/test_codegen.c` exercises all six signatures and NULL rejection without
any incompatible function-pointer casts.
