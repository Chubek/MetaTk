# Tourchstone dyninvoke

`tourchstone/dyninvoke` is a libffi/dyncall-style foreign-call library over
Yellowstone qBFD and Sandstone ISA data.

- qBFD (`yellowstone/qbfd/qABI.hpp`) maps `Format`+`Arch` to a call ABI for
  ELF, Mach-O and PE, covering AMD64, AArch64 and RISC-V (32/64).
- Sandstone `.isa` files (`sandstone/isa/x86.isa`, `aarch64.isa`,
  `riscv64.isa`, `riscv32.isa`) are loaded at runtime to cross-check the
  compiled-in GPR/FPR register files.
- Layout queries (register/stack locations and hidden struct-return pointers)
  are available cross-host for the implemented subset of each ABI.
- Execution uses per-ABI trampolines and currently requires the requested
  ABI to match the host ABI. Closures are implemented for SysV AMD64.

## C API sketch

```c
const dinv_type_t* args[] = {&dinv_type_i32, &dinv_type_f64};
dinv_cif_t* cif = NULL;
dinv_cif_create(DINV_ABI_SYSV_AMD64, &dinv_type_i32, 2, args, &cif);
int32_t a = 2; double b = 2.5; int32_t r = 0;
void* argv[] = {&a, &b};
dinv_call(cif, fn, &r, argv);
dinv_cif_destroy(cif);
```

Structs are described with NULL-terminated element lists:

```c
static const dinv_type_t* pairElems[] = {&dinv_type_i32, &dinv_type_i32, NULL};
static const dinv_type_t pairT = {DINV_STRUCT, pairElems};
```

Call interfaces deep-copy their type graph; temporary descriptors may be released
after creation. Closures retain a signature snapshot and may outlive the source
cif. Cyclic graphs, void fields/arguments, invalid kinds and nesting beyond 64
levels are rejected. Null argument-value storage is rejected before marshalling.
`dinv_type_named` returns borrowed immutable builtin descriptors without an
out-pointer; `dinv_abi_can_execute` reports whether the build has the host-ABI
trampoline (not full signature/ABI conformance).

SysV mixed integer/FP return banks use independent register indices, and hidden
closure returns preserve the destination in RAX. AAPCS64 scalar stack slots use
eight-byte strides. Win64/AAPCS64 indirect aggregate arguments are explicitly
unsupported rather than incorrectly copied by value. Varargs, packed records,
vectors and long double are not modeled; broader cross-ABI aggregate conformance
still needs native target tests. Native functions/callbacks must not throw across
the C/assembly boundary.

## Tool

`dinvoke` opens a library with dynload, inspects it with qBFD, dumps the
calling-convention layout from the Sandstone ISA side, and optionally calls:

```sh
dinvoke --inspect --layout ./libfn.so my_add
dinvoke --ret i32 --arg i32:2 --arg i32:5 ./libfn.so dinvk_add2
dinvoke --abi sysv_amd64 --ret f64 --arg f64:1.5 --arg f64:2.25 ./libfn.so dinvk_addd
```

## Yellowstone extension

`yellowstone/qbfd/qABI.hpp` (included by `qBFD.hpp`) adds the
`qbfd::abi::CallConv` view (`SysV_AMD64`, `Win64`, `Aapcs64`, `Riscv64`,
`Riscv32`), format-aware selection, ISA file names, and host detection.

## XBind spec

`DynINVOKE.xbind` describes the callback-free ABI/type/layout query subset for
all XBind backends: D, Go, Java, Lua, NodeJS, OCaml, Python, Ruby, and Rust.
Direct calls, closure creation, `void **` argument arrays, and executable
function pointers are intentionally deferred until XBind IR supports callbacks
and buffer/out-parameter lowering.
