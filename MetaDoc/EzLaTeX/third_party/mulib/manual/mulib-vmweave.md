# mulib-vmweave

Fault-preserving optimization of the common typed MIR. Despite its name this
module does not splice serialized bytecode, implement branches, or infer effects
for an arbitrary VM.

## Integration and API

Include `mulib-vmweave.h`. Explicitly define `MULIB_IMPL_MIR` and
`MULIB_IMPL_VMWEAVE` once. The optimizer itself does not require minivm; the
standalone example also implements MINIVM and CMACHINE to execute its result.

```c
mulib_mir_buffer_t output = {0};
mulib_mir_diagnostic_t diagnostic;
mulib_mir_status_t status = mulib_vmweave_optimize(&input, limit, &output, &diagnostic);
```

Input is borrowed and must remain immutable during the call. Output must be
empty and cannot alias input. Success transfers a new owned MIR buffer to the
caller; borrow it with `mulib_mir_view` and release with `mulib_mir_buffer_free`.
Input is never changed. Failure preserves empty output and releases scratch.
All diagnostics/status descriptions are from MIR. The returned record index
on success is not meaningful as an error location.

## Exact transformations

The entire input is first validated against `limit`, using the language in
`manual/mulib-mir.md`. All its opcodes are supported. A single forward pass:

1. Replaces uses of COPY records with their original sources.
2. Evaluates operations with known constant operands using
   `mulib_mir_evaluate`; successful evaluation becomes a typed CONST.
3. Leaves a faulting constant operation intact instead of treating a compiler
   error as execution, introducing undefined C arithmetic, or hiding the fault.

A reverse liveness pass then removes unnecessary CONST, COPY and nontrapping
comparisons/NOT, and rewrites surviving value IDs in a forward compaction pass.
All ADD/SUB/MUL/DIV/MOD/NEG records that were not successfully folded remain
live, even when their values are unused. Their dependencies remain live too.
PARAM records are retained, including unused Boolean parameter checks and the
unchanged exact argument-count contract. RETURN and its dependencies remain.
Surviving records retain their relative order, preserving first runtime fault.

For example, `6*7` becomes `CONST 42`, but unused `INT64_MAX+1` remains an ADD
and still faults. Neither `x*0 -> 0` nor reassociation is performed: those can
hide existing faults or introduce overflow. There is no global value numbering,
loop transformation, branch elimination, or aggressive algebraic optimizer.

## Limits and failures

Zero limits are actual zeros. Empty/malformed/cyclic MIR is rejected by common
validation. `limit` bounds input record count, and hence every scratch array:
two record buffers, one `size_t` mapping per input record, and one liveness byte
per input record. Each multiplication is checked. The output reserves at most
the input record count and does not grow. Allocation failures return NO_MEMORY;
representability/record bounds return RESOURCE_LIMIT. Work and memory are O(n).
No allocator injection interface is provided; normal C malloc/free ownership
applies. There is no retained global or borrowed scratch state.

## Runnable example

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/vmweave.c -o /tmp/vmweave
/tmp/vmweave
```

The assert example optimizes a four-record product to two records, lowers that
result with CMACHINE, and executes it on MINIVM to 42. Tests include dead
overflow/division-by-zero preservation and equivalence on parameterized input.
