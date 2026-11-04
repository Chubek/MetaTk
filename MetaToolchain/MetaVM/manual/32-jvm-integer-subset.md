# Chapter 32 — The JVM integer Code-array subset

[Previous](31-drmvm-graphics.md) · [Contents](README.md) · [Next](33-forth-subset.md)

## API and input representation

```cpp
int32_t metavm::machines::jvm_integer_method(
    const std::vector<uint8_t>& code,
    const std::vector<int32_t>& arguments = {},
    size_t locals = 16, size_t stack_limit = 256,
    uint64_t fuel = 1000000);
```

The input is a raw method Code instruction array, not a `.class` file, JAR,
constant pool, or class/method descriptor. Arguments initialize successive local
slots. Other slots are uninitialized and trap on load rather than becoming
usable zero values. All values are signed Java-style int32.

```cpp
#include "VMLibrary/JVM/IntegerMethod.hpp"
auto answer = metavm::machines::jvm_integer_method(
    {0x10,6, 0x10,7, 0x68, 0xac});
// bipush 6; bipush 7; imul; ireturn => 42
```

The complete [native SDK example](examples/native.cpp) includes this path.

## Implemented bytecodes

| Bytes | Operations |
|---|---|
| `00` | `nop` |
| `02`–`08` | `iconst_m1` through `iconst_5` |
| `10`, `11` | Signed `bipush`, `sipush` |
| `15`, `1a`–`1d` | `iload`, `iload_0` through `_3` |
| `36`, `3b`–`3e` | `istore`, `istore_0` through `_3` |
| `57`, `59` | `pop`, `dup` |
| `60`, `64`, `68` | `iadd`, `isub`, `imul` |
| `6c`, `70`, `74` | `idiv`, `irem`, `ineg` |
| `84` | `iinc` with signed-byte delta |
| `99`–`9e` | Integer-versus-zero conditional branches |
| `9f`–`a4` | Two-integer conditional branches |
| `a7`, `ac` | `goto`, `ireturn` |

The conditional order is equal, not-equal, less-than, greater-or-equal,
greater-than, less-or-equal. Branch offsets and `sipush` operands are signed
big-endian 16-bit values; branch offsets are relative to the opcode's start.
The implementation uses explicit sequencing when reading these bytes rather
than relying on expression evaluation order.

## Verification and dynamic checks

Predecoding walks the entire array, rejects unsupported/truncated instructions,
records boundaries, then validates every static branch destination, including
unreachable branches. A branch into an operand byte or outside the array fails.
This is not the JVM's complete class-file/type verifier.

Execution checks stack underflow/overflow, local bounds/initialization, valid PC,
and fuel. Running off the end fails; unsupported input is never replaced with
NOP. Add/subtract/multiply/negate wrap i32. Division truncates toward zero,
zero divisors trap, and `INT32_MIN / -1` wraps to `INT32_MIN` with zero remainder.

## Resource and compatibility limits

Code must contain 1–65,535 bytes. Local count is at most 65,535 and must fit all
arguments. Stack limit is 1–65,535; default is 256. Fuel defaults to 1,000,000
and zero permits no executed instruction. Failures throw `std::runtime_error`;
allocation can throw `std::bad_alloc`. Inputs are borrowed, result is a scalar,
and calls do not share interpreter state.

There are no objects, fields, arrays, long/float/double values, method calls,
exceptions, monitors, class initialization, loader, GC, native JNI, or standard
library. Pin a JVMS edition and class/runtime contract before expanding this
subset or advertising `.class` compatibility.

**References:** [header](../VMLibrary/JVM/IntegerMethod.hpp), [decoder](../VMLibrary/JVM/IntegerMethod.cpp).
