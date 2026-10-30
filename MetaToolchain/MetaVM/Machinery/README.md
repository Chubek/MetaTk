# MetaVM Machinery

A dependency-free, register-based virtual machine. The implementation, public
`.h` interfaces, tests and example runner are ISO C11. `MetaVM.hpp` provides an
optional C++11 RAII interface. Only the C standard library is linked by the VM;
the wrapper uses the C++ standard library. There is no dependency on MetaIL,
MetaArch, LLVM, Lua, POSIX, executable-memory APIs or platform threading APIs.
The target requires 8-bit bytes and exact-width `int64_t`/`uint64_t` types.

## Build and run

```sh
cmake -S MetaVM/Machinery -B build/metavm -DCMAKE_BUILD_TYPE=Release
cmake --build build/metavm -j
ctest --test-dir build/metavm --output-on-failure
build/metavm/metavm-run MetaVM/Machinery/examples/factorial.mvm
build/metavm/metavm-run MetaVM/Machinery/examples/factorial.mvm --threaded
```

Both example runs print `3628800`. `cmake -S MetaVM` also builds Machinery.
The default build needs only a C compiler. Options:

| Option | Default | Purpose |
|---|---|---|
| `METAVM_BUILD_TESTS` | ON | C regression and deterministic mutation tests |
| `METAVM_BUILD_EXAMPLES` | ON | `metavm-run`, accepting assembly or binary modules |
| `METAVM_TEST_CPP` | OFF | Compile and test the optional C++11 wrapper |
| `METAVM_WARNINGS_AS_ERRORS` | ON | GCC/Clang warning checks |
| `METAVM_SANITIZE` | OFF | GCC/Clang AddressSanitizer + UndefinedBehaviorSanitizer |
| `BUILD_SHARED_LIBS` | OFF | Build shared instead of static library |

For embedding, `add_subdirectory(MetaVM/Machinery)` and link
`MetaVM::Machinery` (`MetaVM::VM` is a build-tree alias). Or install:

```sh
cmake --install build/metavm --prefix /your/prefix
```

An installed consumer uses `find_package(MetaVMMachinery CONFIG REQUIRED)` and
`target_link_libraries(app PRIVATE MetaVM::Machinery)`, then `#include "MetaVM.h"`.
No C++ compiler is required unless `METAVM_TEST_CPP` is enabled. All compilation
units can also be compiled directly with `cc -std=c11 -I MetaVM/Machinery`:
`MetaVM.c System.c Module.c Memory.c BinarySerialization.c Compiler.c Cache.c OS.c`.

## Embedding example

```c
#include "MetaVM.h"
#include <stdio.h>

int main(void) {
    static const char source[] =
        ".function main 0 3\n"
        "const r0 40\nconst r1 2\nadd r2 r0 r1\nret r2\n.end\n";
    mvm_module *module = NULL;
    mvm_vm *vm;
    mvm_error error;
    mvm_value result;
    mvm_status status = mvm_compile(source, sizeof(source) - 1, 0, &module, &error);
    if (status != MVM_OK) { puts(error.message); return 1; }
    vm = mvm_create(NULL, &error);
    if (!vm) { puts(error.message); mvm_module_destroy(module); return 1; }
    status = mvm_run_bytecode(vm, module, 0, NULL, 0, &result, &error);
    if (status != MVM_OK) puts(error.message);
    mvm_destroy(vm);
    mvm_module_destroy(module);
    return status == MVM_OK ? 0 : 1;
}
```

The module builder (`mvm_module_add_function`, `mvm_module_emit`, etc.) can be
used directly by another compiler. Builders copy names and instructions; forward
references are allowed while building. Verify before publishing a module.
Loading, serialization, optimization and execution also verify their input.

## Machine model and instruction set

Values are tagged `nil`, `bool`, signed 64-bit integers, or heap-object references.
Register windows are private to each function; arguments initialize `r0` through
`r(arity-1)` and all remaining registers begin as nil. Calls push explicit heap
frames, not recursive C calls. Each executed instruction consumes one step,
including calls and returns. The step budget covers the entire invocation.

| Assembly | Effect |
|---|---|
| `nop` | No operation |
| `const ra integer` | Assign signed-64 literal |
| `nil ra`, `bool ra 0\|1` | Assign nil or boolean |
| `mov ra rb` | Copy any value |
| `add/sub/mul/div/mod ra rb rc` | Integer arithmetic |
| `and/or/xor/shl/shr ra rb rc` | Integer bit operations |
| `eq ra rb rc` | Equality; result is boolean |
| `lt/le ra rb rc` | Signed integer comparison; result is boolean |
| `not ra rb` | Boolean negation of truthiness |
| `jmp label`, `jz ra label` | Branch unconditionally or when false |
| `call ra function rc` | Call; arguments start at rc; result goes to ra |
| `host ra import rc` | Invoke a registered C callback |
| `ret ra` | Return any value |
| `array ra rb` | Allocate rb nil elements (rb must be a nonnegative integer) |
| `get ra rb rc` | Read element rc of array rb |
| `set ra rb rc` | Store value rc at index rb in array ra |
| `len ra rb` | Array element count or string byte count |

Arithmetic requires integer values; there are no implicit boolean conversions.
Add, subtract, multiply and left shift wrap modulo 2^64. Right shift is arithmetic
even on implementations where signed right shift differs. Shift counts outside
0–63 trap. Division truncates toward zero and modulo has the dividend's sign.
Division/modulo by zero trap; `INT64_MIN / -1` wraps to `INT64_MIN` and the
corresponding modulo is zero. These operations do not invoke C signed overflow.

Nil and boolean false are false. **Integer zero is true.** Equality requires the
same value kind; heap objects compare by identity, including strings. Array
access is bounds-checked. Strings are immutable byte sequences and can contain
embedded NUL bytes. Strings can be supplied as arguments or created by native
callbacks; the assembly language does not include string literals.

Instruction fields are specified in `AbstractMachine.h`. `a/b/c` are unsigned
32-bit fields; `immediate` is signed-64. CALL/HOST use `b` for a function/import
index, and `c` for the first argument register. Arity is taken from the target,
so a zero-argument call permits `c == register_count`. Branch immediates are
absolute instruction indices within the same function. Unused fields are
ignored. The verifier rejects unknown opcodes, invalid registers, argument
windows, function/import references, branch targets, empty functions, and
fallthrough at the end of a function (including unreachable instructions).
Type checks happen during execution.

### Engines

`MVM_ENGINE_BYTECODE` selects handlers using a switch for each instruction.
`MVM_ENGINE_THREADED` predecodes instructions into C function pointers for the
duration of a run. Both use an iterative dispatcher and the same semantic
handlers. The threaded engine does not use GNU labels-as-values, pointer casts
between data and functions, generated native code, or recursive handler calls.
Preparation is local to each invocation: immutable modules have no lazy caches
or hidden writes.

### Assembly grammar

`TerseSyntax.h` specifies the line-oriented grammar. Example:

```text
.import print 1
.function main 0 2
    const r0 42
    host r1 print r0
    ret r0
.end
```

Functions and imports have separate name spaces. Labels are local to a function.
Forward references work for all three kinds of names. Names are ASCII
`[A-Za-z_][A-Za-z0-9_]*`, at most 255 bytes. Integers are decimal with an optional
minus sign; arities, register counts and register indices are unsigned decimal.
Lines may use LF or CRLF; the final newline is optional. Commas and ASCII
whitespace separate operands; `#` and `;` introduce comments. A label occupies
its own line. Embedded NULs, duplicate names and unresolved references fail.
Assembly diagnostics include one-based line numbers, and verification/runtime
diagnostics include zero-based function and instruction indices where known.
`SIZE_MAX` denotes an unknown function/instruction; line zero denotes no source
location. Binary modules do not store source-line metadata.

## Ownership, GC and native functions

- Modules are independently owned; destroy with `mvm_module_destroy`. Views from
  module accessors borrow their storage until mutation/destruction. A running
  module must outlive the call and must not be mutated, even by a callback.
- Each VM owns its objects and roots. Values are shallow copies. Objects from
  another VM are rejected at API boundaries. A raw unrooted value must not be
  used after collection; freed object addresses can be reused.
- The non-moving, iterative mark/sweep collector follows arrays, including
  cycles. Roots are active frame registers, explicit `mvm_pin` handles, and the
  most recent successful execution result. The previous result is released
  when a new invocation starts. Validation failures before invocation setup
  leave the previous result rooted.
- Allocation attempts collection when the heap byte limit would be exceeded.
  `mvm_collect` also explicitly collects. Pin host-held objects **before another
  allocation**, including objects backing byte pointers passed to
  `mvm_string_new`. `mvm_pin` itself does not collect. Release with `mvm_unpin`.
  Destruction releases all remaining roots; C root handles cannot outlive the VM.
- Run results remain rooted until the next invocation; pin to keep them longer.
  C++ `metavm::Root` additionally keeps its VM alive. A raw C++ run result follows
  the same lifetime rule as a C result.
- Errors unwind every frame and leave the VM reusable. Output module pointers
  are NULL on failure, run results nil. `mvm_error *` is optional; the returned
  status is authoritative. Allocation failures return `MVM_OOM`, never abort.
  Simple accessor failures leave their output slots unchanged unless documented
  otherwise. All pointer/length inputs must describe valid accessible memory.
- `mvm_register_native` copies the name and borrows the callback/user pointer.
  All imports resolve by exact name and arity before execution, including unused
  imports. Registration during execution and duplicate names are rejected.
- Callbacks receive borrowed rooted arguments. They can allocate, pin and collect.
  Additional callback temporaries need explicit pins across allocations. Return
  a scalar or a live object belonging to that VM. A callback error becomes
  `MVM_HOST_ERROR`; its message is preserved and its guest call site is attached.
  Native callbacks must return normally; C++ callbacks must catch exceptions.
  Reentrant execution on the same VM returns `MVM_BUSY`.

The instruction budget does not bound execution inside a host callback. The
embedding application defines its own native capabilities and callback budgets.

### Limits and concurrency

Default limits are 1,024 active frames, 1,048,576 total active registers, 64 MiB
of object headers/payloads, 16 MiB module input, and 10,000,000 instructions per
invocation. `mvm_create` accepts `mvm_limits`; all fields must be nonzero.
File/assembly/binary loaders accept a separate byte limit (zero selects the
default). Execution also bounds in-memory instruction bytes by the VM's module
byte limit. This is not a process-wide memory quota: parser tables, module names,
roots, native bindings and frame/predecode storage are separate allocations.

All operations on one VM or cache require caller synchronization. Independent VMs
may run concurrently against a shared **immutable** module. The library has no
mutable global state. Module builders, replacement and optimization require
exclusive access to that module. Standard C stdio provides the only filesystem
interface. `mvm_path_join` is a lexical slash join, not path normalization.

## Serialization, rewriting and caching

The binary format is canonical for a given instruction sequence: no C structure
padding, native endianness or object pointers are written. All unsigned integers
are little-endian; an i64 is its 64-bit two's-complement bit pattern.

```text
4 bytes "MVM\0"
u32 version (=1), u32 import_count, u32 function_count
imports:   name, u32 arity
functions: name, u32 arity, u32 registers, u32 instruction_count, instructions
name:      u32 byte_length, byte_length ASCII identifier bytes (no NUL)
instruction (24 bytes): u32 opcode, u32 a, u32 b, u32 c, i64 immediate
```

Opcode numbers are the declaration order in `AbstractMachine.h` (NOP=0,
CONST=1, ..., RET=23, ARRAY=24, GET=25, SET=26, LEN=27). Changes to wire semantics
require a new format version. Loaders reject truncation, unknown versions,
oversized counts, invalid names, malformed instructions and trailing data.
Serialization appends atomically to an initialized `mvm_buffer`; free that buffer
with `mvm_buffer_dispose`. Deserialization returns a new, verified module.
`mvm_load_file` detects binary magic and otherwise compiles assembly.

`mvm_module_replace` verifies a replacement and rolls it back on failure.
`mvm_optimize` removes self-moves/jumps-to-next by replacing them with NOP, and
folds adjacent constant operands inside a basic block using the same integer
semantics as execution. Branch entry points block unsafe folding. Trapping
operations remain in place. Instruction count, branch offsets, and instruction
budgets remain unchanged. The pass is intentionally conservative.

`MetaIL-ISel.h` and `MetaIL-Rewrite.h` expose scalar operation-name selection and
constant evaluation for an external compiler. They do not load or parse the
Lua-backed MetaIL language and do not implement a native JIT. The `mvm_*` API is
the Machinery interface; it is not an implementation of the separate historical
C++ `Program`/`Frame` interface referenced by `MetaJIT/TracingJIT.hpp`.

`mvm_cache` is a bounded LRU of verified, independently cloned module snapshots.
Replacing the original does not alter a cached module. Lookups return borrowed
views; insertion, clearing or destruction can invalidate them. Cache operations
do not affect a VM's dispatch preparation.

## Verification

The tests execute both engines, recursion and argument/result aliasing; check
every opcode and signed arithmetic edges; preserve traps through optimization;
reject unsafe control-flow rewrites; exercise cyclic GC, automatic collection,
foreign-object rejection, native callbacks and reentrancy; check rollback, LRU
eviction, file loading, exact binary bytes, every truncation of a valid module,
2,000 deterministic binary mutations and 1,000 random assembly inputs. Assertions
remain active in Release builds. C++ tests exercise ownership transfer and errors.

Reproduce compiler/sanitizer checks (substitute `clang`/`clang++` for GCC):

```sh
cmake -S MetaVM/Machinery -B build/metavm-gcc \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Release -DMETAVM_TEST_CPP=ON
cmake --build build/metavm-gcc -j
ctest --test-dir build/metavm-gcc --output-on-failure

cmake -S MetaVM/Machinery -B build/metavm-gcc-san \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Debug -DMETAVM_TEST_CPP=ON -DMETAVM_SANITIZE=ON
cmake --build build/metavm-gcc-san -j
ctest --test-dir build/metavm-gcc-san --output-on-failure
```
