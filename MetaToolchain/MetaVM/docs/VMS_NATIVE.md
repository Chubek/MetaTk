# Existing JVM and FORTH subsets

These contracts describe the code that exists. Full JVM class loading and a
standard Forth environment remain outside their coverage. Both APIs borrow
immutable inputs, return owned scalar/container values, use no shared mutable
state, and throw `std::runtime_error` for malformed inputs, traps and resource
exhaustion. Allocation failures may propagate `std::bad_alloc`.

## JVM integer Code arrays

Include `VMLibrary/JVM/IntegerMethod.hpp` or `VMLibrary/JVM/JVM.hpp`.

```cpp
auto answer = metavm::machines::jvm_integer_method({0x10, 42, 0xac});
// bipush 42; ireturn => int32_t(42)
```

Signature: `jvm_integer_method(code, arguments={}, locals=16,
stack_limit=256, fuel=1000000)`. Arguments initialize local slots from zero;
other slots are uninitialized until written. Code must be nonempty and at most
65,535 bytes. Locals must hold the arguments and may not exceed 65,535. Stack
capacity must be 1–65,535. Fuel is a literal executed-instruction ceiling.

Supported instructions are NOP; ICONST_M1/0–5; BIPUSH; SIPUSH; ILOAD and
ILOAD_0–3; ISTORE and ISTORE_0–3; POP; DUP; IADD, ISUB, IMUL, IDIV, IREM,
INEG; IINC; IFEQ/NE/LT/GE/GT/LE; IF_ICMPEQ/NE/LT/GE/GT/LE; GOTO; IRETURN.
Other opcodes, including unreachable ones, fail. Operands are decoded to
instruction boundaries before execution; every static branch must point to an
instruction start within the same array. Branch deltas are signed big-endian
16-bit offsets from the branch opcode. Operand/local/stack checks remain dynamic.

Arithmetic wraps i32. Signed division truncates toward zero; INT32_MIN divided
by −1 returns INT32_MIN and its remainder is zero. Zero divisors trap. IRETURN
returns the top integer; this subset does not prove a JVM verifier stack shape or
require the rest of the stack to be empty. Falling off Code traps. No `.class`
reader, constant pool, calls, references, exceptions, floating point, wide slots,
monitors or object/GC runtime is implemented.

## FORTH signed i64 words

Include `VMLibrary/FORTH/Forth.hpp` or `VMLibrary/FORTH/FORTH.hpp`.

```cpp
auto stack = metavm::machines::forth(": square dup * ; 6 square 6 +");
// stack == std::vector<int64_t>{42}
```

Signature: `forth(source, fuel=1000000, stack_limit=65536)`. Source is limited
to 16 MiB. Stack capacity must be 1–1,000,000 values; return frames are capped
at 4,096. Fuel charges executed tokens. Results list the stack bottom to top.

Input is whitespace-tokenized and case-sensitive. Decimal signed i64 literals
use `std::from_chars`; no base switching, floating literals, leading `+` syntax
or comments are implemented. `: name ... ;` defines a word. The entire input is
parsed before execution, so later definitions are visible to earlier main-body
tokens; this is not the standard interactive Forth compiler. Repeated names
replace earlier dictionary entries; dictionary entries take precedence over
built-in words. Recursion runs on explicit return frames, not the C++ call stack.

Words: `+ - * / mod dup drop swap over = <`. Add/subtract/multiply wrap i64.
Division truncates toward zero; remainder has the dividend's sign. INT64_MIN/−1
wraps to INT64_MIN, with remainder zero. Zero divisors trap. Comparisons produce
−1 for true and 0 for false. Unknown words, bad definitions, stack underflow,
stack/frame limits and exhausted fuel fail. There are no standard control-flow
words, variables, memory, I/O, source interpreter state or host OS words.

See [CLOSURE_VMS.md](CLOSURE_VMS.md) for SECD/Krivine and [DVI.md](DVI.md) for
the new DVI native API. [VMS.md](VMS.md) lists every legacy identity and its
outstanding compatibility requirements.
