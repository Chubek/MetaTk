# 4. MetaIL language and binary specification

## Machine model

MetaIL is an integer register machine with 32 registers, a shared value/return
stack, and an instruction-index program counter. Registers initially read as
signed 64-bit zero. Arithmetic is deterministic across hosts; no operation
relies on C++ signed-overflow behavior.

The generic MetaVM supports other value kinds and custom instruction sets.
MetaIL itself uses integer values only. Its verifier checks instruction shapes,
register ranges, opcode values, canonical unused fields, source-map length, and
branch targets. It is not a theorem prover: division by zero, invalid shifts,
stack underflow, and dynamic return addresses are checked while executing.

## Text syntax

One instruction is written per line. Operands are comma-separated. Mnemonics
are case-insensitive; register names use `r0` through `r31`. Labels are
case-sensitive and may precede an instruction on the same line. Multiple
leading labels are permitted.

`;` and `#` introduce line comments outside quotes. Empty lines are ignored.
Constants accept decimal and `0x`, `0b`, or `0o` prefixes, with an optional sign.
IL constants must fit signed 64-bit range. To denote the all-ones bit pattern,
write `-1`, not unsigned decimal `18446744073709551615`.

```text
const r0, 0x2a
again: print r0
halt r0
```

Branch targets are labels or numeric instruction indices. They are never byte
offsets. A label at end-of-program is not a valid branch target; fallthrough to
the end is allowed, but explicit branches must identify an instruction.

## Complete instruction table

`d`, `a`, and `b` below are registers. `target` is a label or instruction index.

| Syntax | Semantics |
|---|---|
| `nop` | no state change |
| `const d, immediate` | assign a signed 64-bit constant |
| `mov d, a` | copy a register |
| `add d, a, b` | addition modulo 2^64 |
| `sub d, a, b` | subtraction modulo 2^64 |
| `mul d, a, b` | low 64 bits of the product |
| `div d, a, b` | signed division, truncating toward zero |
| `mod d, a, b` | signed remainder with dividend's sign |
| `and d, a, b` | bitwise AND |
| `or d, a, b` | bitwise OR |
| `xor d, a, b` | bitwise XOR |
| `shl d, a, b` | left shift of unsigned bit pattern |
| `shr d, a, b` | logical, zero-filling right shift |
| `eq d, a, b` | 1 if equal, otherwise 0 |
| `lt d, a, b` | signed less-than, yielding 0 or 1 |
| `jmp target` | unconditional branch |
| `jz a, target` | branch if register is zero |
| `jnz a, target` | branch if register is nonzero |
| `push a` | push an integer |
| `pop d` | pop an integer into a register |
| `call target` | push next instruction index, then branch |
| `ret` | pop instruction index and branch to it |
| `print a` | append an integer to captured output |
| `halt a` | stop, returning a register's integer value |

Shift counts outside `[0,63]` trap. Both division and remainder trap for a zero
divisor and for `INT64_MIN / -1`; this avoids host-dependent overflow. `shr`
is not an arithmetic signed shift: `shr(-1,63)` is 1.

Calls and ordinary values share one stack. A subroutine must balance its
temporary pushes before `ret`. There is no protected call stack or automatic
register save convention. A popped return address in `[0, program.size()]` is
valid; returning exactly to the end halts by fallthrough.

## C++ instruction representation

`Instruction` has `Op op`, three `uint8_t` register fields `a`, `b`, `c`, and an
`int64_t immediate`. For binary arithmetic, `a` is destination and `b/c` are
sources. For `mov`, `a/b` are destination/source. For `const`, `a` is destination
and `immediate` is the constant. Conditional branches use `a` and `immediate`.
Unconditional branches and calls use only `immediate`.

Unused register and immediate fields must be zero. This makes binary encodings
canonical and detects accidental struct misuse. A source map is either empty
or exactly as long as the instruction vector.

```cpp
metail::Builder b;
b.constant(0, 40).constant(1, 2)
 .binary(metail::Op::Add, 0, 0, 1)
 .halt(0);
auto result = b.finish();
```

The builder also supports `name`, `emit`, `label`, and `jump`. Symbolic jump
fixups are resolved only by `finish()`. Values returned earlier do not borrow
the builder's storage.

## Text and execution API

* `parse(source, file = {}, limits = {})` returns `metail::Result`.
* `verify(program, limits = {})` returns a vector of diagnostics.
* `emit(program)` returns canonical IL text with numeric branch targets.
* `run(program, vm_options = {}, limits = {})` returns `Execution`; the limits
  argument bounds verification, so a host can apply one ceiling to every stage.
* `evaluate(op, lhs, rhs)` computes one binary operation or returns `nullopt`
  for a trapping/unsupported operation.

`Execution` holds `status`, full `value`, captured `output`, attempted `steps`,
and diagnostics. Its boolean conversion requires clean halting. Fallthrough
returns the default result zero. A halted instruction counts as an executed
step; an attempted trapping instruction also counts. Step limits do not silently
pretend to be ordinary halts.

## MCIL wire format, version 1

All multi-byte quantities are little-endian, regardless of host endianness.
Serialization is field-by-field and does not copy a native C++ struct.

| Byte offset | Length | Content |
|---|---|---|
| 0 | 4 | ASCII `MCIL` |
| 4 | 1 | version 1 |
| 5 | 3 | reserved, all zero |
| 8 | 4 | instruction count, unsigned 32-bit |
| 12 | `12 * count` | instruction records |

Each record contains opcode byte, `a`, `b`, `c`, then the 64-bit two's-complement
immediate. Opcode values are the table order, starting at `nop = 0` and ending
at `halt = 23`. There is no padding, checksum, source map, filename, or debug
section. The exact file length must match the count: trailing bytes are errors.

`encode(program)` verifies first and returns an empty byte vector on invalid
input. `decode(bytes, limits)` validates the header, reserved bytes, record
count/length, and every decoded instruction. Never persist the native memory
layout of `Instruction` as a substitute for this format.

## Conservative optimization

`optimize(program)` propagates constants within basic blocks and replaces
foldable moves/arithmetic with constants. It clears knowledge at branch
targets and control-flow boundaries. Potentially trapping arithmetic is not
folded into an invented value.

The optimizer never deletes or inserts instructions. Numeric branch targets,
source-map positions, and instruction counts stay stable. It does not perform
global dataflow, dead-code elimination, register allocation, or interprocedural
analysis. These are deliberate scope boundaries rather than transformations
that claim success without changing anything.
