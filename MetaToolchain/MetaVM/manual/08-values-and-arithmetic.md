# Chapter 8 — Values, arithmetic, and truthiness

[Previous](07-command-line-reference.md) · [Contents](README.md) · [Next](09-modules-and-instructions.md)

## The tagged value contract

`mvm_value` contains `kind` and a union named `as`. Read only the union member
corresponding to the kind:

| Kind | Payload | Constructor |
|---|---|---|
| `MVM_NIL` | No meaningful payload | `mvm_nil()` |
| `MVM_BOOL` | `as.boolean` | `mvm_bool(int)` |
| `MVM_INT` | `as.integer`, signed i64 | `mvm_int(int64_t)` |
| `MVM_OBJECT` | `as.object`, VM-owned reference | Object allocation APIs |

Values are shallow copies. Copying an object-valued `mvm_value` neither clones
nor roots the object. A pointer's bit pattern is not a transferable value
between VMs, a persistent identifier, or a wire representation.

`mvm_value_equal` compares scalar values of the same kind and object identity.
Integer `1` is not boolean true. Two separately allocated identical strings
are not equal by content through the VM's equality operation. Use host byte
comparison when an application needs string-content equality.

## Truthiness is not C truthiness

Only nil and boolean false are false. **Every integer, including zero, is true.**
`jz` branches when `mvm_value_truthy` is false, and `not` negates that rule.

For an integer-zero test:

```text
const r1 0
eq r2 r0 r1
jz r2 nonzero
```

Here `eq` produces a boolean, so `jz` behaves as intended. Using `jz r0` directly
would not branch for integer zero. Definition lowering, SSAVM, SECD, and FORTH
have their own explicit scalar/condition conventions; do not carry Machinery's
truthiness into those profiles accidentally.

## Defined arithmetic

Arithmetic instructions require integer operands, without implicit bool-to-int
conversion. Add, subtract, and multiply wrap modulo 2^64. Bitwise AND/OR/XOR
operate on that bit representation. Left shift wraps; right shift is arithmetic.
Counts outside 0–63 fail rather than being masked to fit.

Division truncates toward zero. Remainder has the dividend's sign. Zero divisors
trap. The overflow edge `INT64_MIN / -1` returns `INT64_MIN`; its remainder is
zero. These semantics avoid C signed-overflow undefined behavior. Comparisons
`lt`/`le` are signed and return boolean values; `eq` supports the value-kind
equality contract above.

`mvm_integer_eval(op, a, b, &result)` supplies the same scalar semantics for
lowering, native profiles, and constant folding. Its comparison results are
integer 0/1, unlike executed Machinery comparison instructions' tagged bools.
Always check its status, especially for division and shift counts.

## Distinguish guest widths

Machinery i64 arithmetic does not automatically implement a narrower guest.
JVM int operations explicitly wrap i32, CHIP-8 wraps byte arithmetic, and
Definition writes mask to its declared register width. A compiler must make
that conversion boundary explicit rather than depend on host integer overflow.

**References:** [Value.h](../Machinery/Value.h), [instruction model](../Machinery/AbstractMachine.h).
