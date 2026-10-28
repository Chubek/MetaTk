# MetaVM version 1

This is a new, explicitly versioned scalar register VM, not an existing external
bytecode standard. It is separate from MetaIL. It has no heap, raw addresses,
internal function stack, dynamic types, implicit external symbol resolution or
floating-point instructions. Host calls are the only external effects.

## Programs and ownership

`Program(slots, parameters, instructions, host_names, limits)` validates and owns
one function. Slots are signed 64-bit values. Parameters occupy `[0,parameters)`;
all other slots start at zero. Host names match `[A-Za-z_][A-Za-z0-9_.]*`, are
unique, and contain 1–255 bytes. Registers and branch targets must be in range.
An ordinary instruction may not fall off the final instruction, even if
unreachable. Unknown opcodes/versions are rejected.

Programs are immutable and safe for concurrent const reads. Copies share an
opaque identity. Frames/snapshots can resume only against that identity; loading
identical serialized bytes creates a new identity. Code and host tables are
never modified by execution. Moving/destroying an actively used Program or
changing a frame during execution is forbidden. Frames are caller-owned, not
thread-safe, and cannot be reentered from a host call.

Default limits: 4096 slots, 1,000,000 instructions, 1024 host names and 32 MiB
serialized input. Host calls accept 0–64 arguments. All decoding size/count checks
precede dependent allocations. Custom limits are explicit, not encoded in the
wire format. The MetaIL adapter additionally observes MetaIL's parser limits.

## Instruction fields

`Instruction` has `op`, `dst`, `a`, `b`, `imm`. Indices are unsigned 32-bit;
`imm` is signed 64-bit. Unused fields are ignored and preserved on serialization.
Numeric opcode values are the declaration order in `VM.hpp` (0 through 26).

| Opcode | Semantics |
| --- | --- |
| Constant | `slot[dst] = imm` |
| Move | `slot[dst] = slot[a]` |
| Add/Sub/Mul | `slot[dst] = slot[a] op slot[b]`, wrapping modulo 2^64 |
| Div/Mod | Signed division/remainder, truncating toward zero |
| Shl/Shr | Shift `slot[a]` by `slot[b]`; left wraps and right sign-extends |
| Band/Bor/Bxor | Bitwise operation on `a,b` |
| Neg/Bnot/Lnot | Unary operation on `a`; negation wraps, logical-not produces 0/1 |
| Eq/Ne/Lt/Le/Gt/Ge | Signed comparison on `a,b`, producing 0/1 |
| Land/Lor | Eager boolean operation on `a,b`, producing 0/1 |
| Jump | Next PC is `a` (instruction index, not byte offset) |
| Branch | If `slot[a] != 0`, next PC is `b`; otherwise next PC is `imm` |
| Call | Call host table index `a` with `imm` values starting at slot `b`; write return value to `dst` |
| Return | Return `slot[a]` |

Operands are read before writing a destination, so register aliasing is defined.
Ordinary instructions advance PC by one. Return/trap retain their own PC.

Division or remainder by zero traps; both `INT64_MIN / -1` and
`INT64_MIN % -1` trap. Shift counts outside 0–63 trap. Arithmetic traps write no
destination. These are explicit VM statuses, not C++ overflow or process signals.
There is no eager `select` opcode: vmweave lowers MetaIL's lazy `select` into
branches, and evaluates other expression operands left-to-right.

`run(program,frame,budget,host)` executes at most `budget` instructions. Every
attempted instruction, including a trapping/returning/calling instruction, counts
once. A zero budget does nothing. Exhaustion leaves the frame Running at its
next PC, so it can resume with a new budget. `frame.steps` is cumulative; requests
that could overflow it are rejected before execution. Instruction budgets do not
bound host callback wall time or memory: hosts must supply their own limits.

Host callbacks receive a name and a copied argument vector; the return value is
signed 64-bit. Missing callbacks trap. Exceptions become HostFailure with a
diagnostic; `bad_alloc` propagates after marking the call trapped, so retry cannot
replay a possibly performed external effect. A host must not mutate/destroy the
active frame or program. Host argument allocation failure before invocation may
propagate without executing the callback. Allocation failures are never converted
to successful bytecode execution.

## Binary wire format

All multibyte fields are little-endian, with no padding or native structs. Signed
immediates use 64-bit two's-complement bits. There is no executable native code
inside this format.

1. Four magic bytes: `4D 56 4D 00` (`MVM\0`).
2. Five uint32 fields: version (`1`), slot count, parameter count, instruction
   count, host count.
3. Each host: uint32 byte length, then exactly that many ASCII bytes, no NUL.
4. Each instruction: uint32 opcode, dst, a, b; then int64 immediate (24 bytes).

Truncation, trailing bytes, oversized counts, invalid names/indices/opcodes and
unsupported versions fail before a Program is published. Serialization is
byte-stable, locale-independent and round-trips all accepted fields.

## MetaIL adapter

`weave(source,function,metail_home,limits)` uses MetaIL's actual parser and returns
an owned Program. It compiles one named function. Every call expression targets
the explicit host namespace, including names matching other source functions;
there is no implicit recursive/module linker. Assumptions are rejected because
MetaIL does not specify their executable semantics. Must-definition analysis
rejects reads not initialized on every reachable predecessor, including loop
back edges. Reachable fallthrough is rejected. Expression temporary slots are
reused between statements, with peak usage checked against the slot limit.

MetaIL itself remains an IR/optimization library: no interpreter or JIT is
injected into it by this adapter.
