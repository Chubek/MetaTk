# 10. C interoperability and the portable C backend

## Build

QScript's registered C callbacks, binary serialization and shared-library
loader are always available. Typed native calls use **libffi**, enabled by
default. Install the libffi development package, or configure with
`-DQSCRIPT_ENABLE_CFFI=OFF` for callback-only embedding. Query the build with
`qs_cffi_available()`.

Link C/C++ applications to `MetaChain::QScript`; CMake propagates libffi and
the platform loader dependency. `-DQSCRIPT_BUILD_SHARED=ON` produces a shared
QScript library for dynamic embedding. Static builds are position-independent.

Public headers under `qscript/include`:

| Header | API |
|---|---|
| `QScript-FFI.h` | Register/unregister C callbacks; includes `qs_call` for C → QScript |
| `QScript-Serialization.h` | Versioned, portable integer-vector wire format |
| `QScript-SharedLibrary.h` | Open/close shared libraries and look up symbols |
| `QScript-CFFI.h` | Typed native calls, native-function bindings, script buffers |

## Calling C from QScript, and QScript from C

```c
#include "qscript/include/QScript-FFI.h"
#include <string.h>

static qs_status add(const int64_t *args, size_t count,
                     int64_t *result, void *user) {
    (void)user;
    if (count != 2) return QS_ERROR;
    *result = args[0] + args[1];
    return QS_OK;
}

int main(void) {
    qs_context *c = qs_create();
    if (!c) return 1;
    if (qs_register_function(c, "host_add", 2, add, NULL) != QS_OK) {
        qs_destroy(c); return 1;
    }
    const char *source = "fn answer(x) { return host_add(x, 2); }";
    qs_program *p = qs_compile(c, source, strlen(source));
    int64_t arg = 40, result = 0;
    int ok = p && qs_call(c, p, "answer", &arg, 1, &result) == QS_OK
               && result == 42;
    qs_program_destroy(p);
    qs_destroy(c);
    return ok ? 0 : 1;
}
```

Register before compilation. Functions accept up to 16 arguments; names and
arities are checked at compile time. Names may be qualified (`host::add`).
They are available to imported script libraries as well. Duplicate names and
script declarations conflicting with a host binding are rejected.

Callbacks work with both `QS_ENGINE_INTERPRETER` and `QS_ENGINE_JIT`, including
`qs_call`. A non-`QS_OK` callback result becomes an execution diagnostic at
the script call site. Unregistering a function invalidates its previously
compiled calls; re-registering the same name gets a new binding ID. A program
with host calls must run with its original context's bindings. Callback user
data remains caller-owned. Use a separate context for callbacks that call back
into QScript; contexts are neither thread-safe nor reentrant.

C++ uses `Options::hosts`, a map of `HostFunction {id, arity, invoke}`. IDs must
be unique and nonnegative. Pass the same bindings to compilation and execution.
Set `Options::jit=true` for JIT execution; `Options::toolchain.jit` controls
its mode and thresholds. Callback exceptions become VM traps.

## Typed calls and shared libraries

1. `qs_shared_library_open(context, path)` loads a library eagerly with local
   symbol scope. On POSIX, a NULL path opens the current process.
2. `qs_shared_library_symbol` resolves a symbol, reporting loader errors via
   `qs_error`. Raw symbol addresses remain valid while the library is retained.
3. `qs_cffi_function_create` resolves a function and prepares its signature.
4. `qs_cffi_call` calls it using `qs_cffi_value` arguments, or `qs_cffi_bind`
   exposes it as a QScript function.
5. Destroy function and library handles when finished. Prepared functions and
   bindings retain their shared library, so closing the original handle does
   not invalidate them.

Types: `VOID` (result only), `I8/U8`, `I16/U16`, `I32/U32`, `I64/U64`, `FLOAT`,
`DOUBLE`, `POINTER`. The names have a `QS_CFFI_` prefix in C. `qs_cffi_value.type`
selects `data.i64`, `data.u64`, `data.f64`, or `data.pointer`. Both floating
types use `data.f64` in the C API. Argument types must match the signature;
narrow integers are range-checked before calling native code.

The implementation supports fixed-arity functions using the platform's default
C ABI, up to 16 arguments. Variadic functions, non-default calling conventions,
and structs passed by value require a C wrapper. Match the actual function
signature, and keep pointed-to storage alive through each native call. Native
code runs with the embedding process's privileges; VM instruction limits do
not limit time spent inside native code.

## The `cffi` script library

Enable it with `qs_cffi_enable(context)` or `qscript --cffi`, then `use cffi;`.
Values remain signed 64-bit integers: unsigned 64-bit values, floating values
and pointers cross the VM as bit patterns. `FLOAT` uses the low 32 bits;
`DOUBLE` uses all 64. Narrow integer parameters are checked, not truncated.
Void functions return zero.

| Functions | Meaning |
|---|---|
| `alloc(bytes)`, `size(buffer)`, `release(handle)` | Zero-filled buffers; release buffers, libraries or functions |
| `read(buffer,offset,width)`, `write(buffer,offset,width,value)` | Checked little-endian access, widths 1, 2, 4 or 8 |
| `byte(buffer,offset)`, `put_byte(buffer,offset,value)` | Byte access; `put_byte` requires 0..255 |
| `copy(dst,dst_offset,src,src_offset,count)` | Checked, overlap-safe byte copy |
| `address(buffer)` | Native pointer bits, valid until the buffer is released |
| `open(path_buffer)` | Load a NUL-terminated path; `open(0)` opens this process on POSIX |
| `symbol(library,name_buffer)` | Resolve a NUL-terminated symbol name to pointer bits |
| `function(library,name_buffer,result_type,types,count)` | Prepare a callable function handle |
| `arg(types,index,type)` | Pack an argument type into a four-bit signature slot |
| `call0(f)` … `call8(f,a,...)` | Call a prepared function; count must match |

Buffers and handles belong to the context and persist across runs. Handles are
typed, checked and never reused within a context. The script resource limits
are 4,096 live handles and 64 MiB of buffer storage per context. Names and paths
must be NUL-terminated; fresh buffers already contain zero bytes.

For example, on a POSIX process exporting `abs` from its C library:

```qscript
use cffi;
let name=alloc(4);
put_byte(name,0,'a'); put_byte(name,1,'b'); put_byte(name,2,'s');
let library=open(0);
let absolute=function(library,name,I32,I32,1);
let result=call1(absolute,-42);
release(absolute); release(library); release(name);
return result;
```

See also `qscript/examples/cffi.qs` for packed data and buffer copying.

## Data exchange and serialization

`qs_values_encode` and `qs_values_decode` encode vectors of signed 64-bit
integers. The wire format is exactly:

* bytes 0..3: ASCII `QSV1`;
* bytes 4..7: unsigned 32-bit little-endian element count;
* subsequent bytes: eight bytes per value, little-endian two's-complement.

There is no native struct padding or pointer representation. A NULL destination
with zero capacity queries the required byte/element count. Short destinations
and malformed/truncated/trailing input are rejected before writing output.
Encoding supports at most `UINT32_MAX` elements, subject to address-space limits.
Input and output storage must not overlap. These two functions do not need a
context and report status directly.

To exchange serialized data with a running script:

1. Encode C integers with `qs_values_encode`.
2. Copy the bytes into a context buffer with `qs_cffi_buffer_create`.
3. Pass its integer handle to `qs_call`. Script code can use `read(b,8,8)` for
   the first value and `write(b,16,8,value)` for the second.
4. Copy bytes back using `qs_cffi_buffer_read`, then `qs_values_decode`.
5. Release the handle with `qs_cffi_release`.

`qs_cffi_buffer_write` updates an existing buffer without moving its storage.
Read/write APIs check bounds before copying. `endian` provides packed-byte
manipulation and signed extension; `integer` adds midpoint and checked alignment.

## Portable C99 backend

```sh
build/qscript c program.qs -o program.c
cc -std=c99 -O2 -DQSCRIPT_STANDALONE program.c -o program
./program
```

The generated standalone program prints script output followed by `result N`.
Alternatively, omit `QSCRIPT_STANDALONE` and call `qs_generated_run` with step
and stack budgets and output/host callbacks. Status codes are documented in
the generated source: 0 halt, 1 arithmetic/return trap, 2 step limit, 3 stack or
allocation limit, 4 callback failure. Host and output callbacks return zero
for success. Generated host callbacks receive the binding ID and all 32 integer
registers; the embedding binding determines its argument count.

C APIs use `qs_emit(..., QS_C_SOURCE, ...)`; C++ uses `qscript::emit_c` or
`metachain::lower_c99` from `metachain/CSource.hpp`. Arithmetic preserves
MetaIL's wrap-around, logical shifts and traps without C signed-overflow UB.
Each instruction retains a label and consumes a step. The backend supports
host calls through its explicit callback interface; exported binding IDs must
be connected to the corresponding host functions by the embedding application.

QScript's process-local host bindings cannot be saved as MCIL bytecode or linked
into static syscall-only ELF executables. These requests produce diagnostics.
Pure script programs retain those existing output formats.
