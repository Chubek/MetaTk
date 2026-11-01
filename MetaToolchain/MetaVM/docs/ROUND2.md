> The later [third pass](ROUND3.md) validates the real MetaML embedding and CMake,
> exposes Definition through MetaML, and expands SSAVM. This historical record is
> retained; its corresponding open items are superseded there.

# Second implementation pass

This pass keeps every prior source path and public entry point. The exact first
edition is preserved, byte for byte, at `history/round1.zip`. Existing catalog
profiles and native implementations remain in place. Historical documentation
has been retained; the current amendments here supersede its resolved gaps.

## Layers

| Layer | Entry point | Responsibility |
|---|---|---|
| Authoring | `vmweave/Builder.hpp` | Typed construction of Machinery assembly |
| MetaML embedding | `vmweave/MetaMLAdapter.hpp` | Actual MetaML Session, typed primitives, diagnostics |
| Definition IR | `vmweave/Definition.hpp` | Masked instruction definitions and scalar semantics |
| Lowering | `Definition::lower` | Decode a fixed guest binary into a verified Machinery module |
| Execution | `Definition::execute`, Machinery | Bounded execution and explicit state ownership |
| Native subsets | `VMLibrary` | Preserve the five existing machine implementations |
| Documentation | `VMDesc/MetaTeXAdapter.hpp` | Actual optional MetaTeX Engine integration |

## Definition IR and lowering

`Definition` introduces instruction encodings, fields, register/memory state and
SSA-style expression references. A rule has a word mask/value and a sequence of
operations. Operands refer only to earlier value-producing nodes. Verification
rejects ambiguous masks, duplicate rule names, malformed fields, invalid enum
values, forward references and operations after a control transfer.

Supported instruction widths are 8, 16 and 32 bits, with either byte order.
Register writes wrap to a declared width from 1 to 64 bits. Memory cells wrap
to unsigned bytes. Arithmetic uses Machinery's defined i64 semantics. The IR
normalizes comparison results to integer 0/1. Conditional branches test integer
nonzero. This differs from Machinery's own truthiness, where integer zero is
truthy; the lowering explicitly converts conditions before using JZ.

Operations include constants, signed/unsigned fields, register reads/writes,
byte reads/writes, arithmetic, bit operations, shifts, comparisons, absolute
jumps, conditional branches and halt. `execute` roots state across allocations,
zero-initializes unprovided state and returns copied registers/memory/result.
The result's step count is Machinery instructions, not guest instructions.

Version-1 limits:

- At most 4096 rules and 4096 nodes per rule.
- 1–4096 guest registers; at most 16,777,216 guest data bytes. Machinery's heap
  limit independently applies to its scalar-array representation.
- At most 65,536 guest code bytes, with complete fixed-width instruction words.
- A default 16 MiB authoring/lowering source budget and Machinery module limits.
- Branch destinations must resolve statically to aligned code addresses.
  Runtime-computed indirect jumps are explicitly rejected.
- Code and data memory are distinct. Self-modifying code is unsupported.
- Unknown instructions are rejected even if statically unreachable.
- A conditional/fallthrough path may not run off the end of the guest program.

This is **program-specialized bytecode lowering**, not a general generated native
CPU interpreter. The existing native catalog implementations have not been
silently replaced by this layer. Further work is needed to expose the complete
Definition IR through MetaML and to generate a reusable runtime decoder.

`vmweave/examples/ToyR16.hpp` supplies a specified example ISA, independent of
historical machine names. It has immediate load, add, subtract, byte store,
branch and halt instructions. Build and run:

```sh
make test
build/vmweave-definition-demo
build/vmweave-definition-demo toy-program.c
```

The demo returns `result=42 memory[3]=42`. The optional C output contains a
factory for the lowered module. Its `main` entry takes two Machinery arrays:
registers and data memory. `tests/definition_factory.c` shows a C host allocating,
rooting and initializing these arrays, then running the generated code under
both engines. The normal test workflow compiles and executes this C program.

## SSAVM source and verification

`ISSA::parse`, `ISSA::format` and `ISSA::verify` are added without removing the
existing instruction-vector runner. The bounded line-oriented source accepts
optional consecutive instruction numbers, parenthesized result references,
`[target] edge` branch operands and `#` or `;` comments. Decimal immediates are
range checked; unknown instructions, malformed tokens and extra operands fail.

Examples of the supported grammar:

```
0 int_arg
1 const 7
2 iadd (0) (1)
3 return (2)
```

A bare `int_arg` uses its occurrence's zero-based argument ordinal. An explicit
`int_arg N` selects argument N. Canonical formatting always writes explicit
indices and numbered instructions. Supported arithmetic aliases include
`iadd/isub/imul/idiv`; the formatter uses `add/sub/mul/div`.

Source is bounded to 16 MiB, lines to 128 KiB and tokens per line to 16,384.
Syntax and instruction verification failures report original source lines.
Empty/oversized programs are rejected. The source reader is the implemented
ISSA subset, not the entire appendix instruction set or a SafeTSA loader.

Verification now rejects malformed opcode enum values and branch entry into
the middle of a PHI/PFE group. PHI groups must end in PFE before any other
instruction. Runtime undefined-value, type, array, CEN and fuel checks remain.
Persistent array versions and simultaneous PHI commits are unchanged.

```sh
build/ssavm-run VMLibrary/SSAVM/fibonacci.ssa
# 55
```

## Atomic host binding and outputs

`mvm_register_natives` adds a transactional batch C API. All entries are validated
and all names allocated before committing the binding table. Duplicate names,
invalid callbacks, busy VMs, allocation failure or table growth failure leave
prior bindings unchanged. The old single-binding API remains available.
`Graphics::bind` now installs its two imports in one transaction; a collision
cannot leave `drm_clear` registered by itself. VM synchronization and callback
state lifetime remain caller responsibilities.

`mvm_write_file_atomic`, `mvm_save_file_atomic` and the C++ `Module::save_atomic`
add staged output APIs without
changing the old direct-write functions. A sibling is created exclusively,
closed and renamed into place. POSIX replacement is atomic; a platform whose
rename refuses to replace an existing file returns an error and preserves it.
This does not promise fsync durability or preserve destination permissions/xattrs;
the caller must control the destination directory.

`vmwv_emit_c_file` stages C generation and commits via the new write API. The
compiler CLI uses staged output for both binary compilation and C emission.
Invalid sources and symbols preserve the prior output. Existing unrelated
staging siblings are not overwritten. Factory symbols now reject C keywords,
reserved framework prefixes, leading underscores and names over 255 bytes.
Choose a unique application factory namespace. Generated factories use default
module-load limits, and oversized serialized modules are rejected before output.
Caller-owned stream emission remains available through `vmwv_emit_c`.

## Authoring fixes

Typed Builder operations now require an open function and in-range registers.
Failed validation leaves the accumulated source unchanged. Raw `emit` remains
available for the assembly bootstrap, with final verification at `finish`.
Raw text does not update typed structural state; pair typed function/end methods
when using the typed authoring API.

The original typed factorial example tested integer zero using Machinery JZ;
that loop would never terminate. It now compares the counter to zero first.
A dependency-free typed-builder regression verifies its result is 3628800.
MetaML failures retain the language diagnostic codes/messages, and authoring
input length/NUL checks are applied before evaluation.

## Observed verification and remaining limits

The regular fallback suite passes with warnings as errors, retaining the 5,056
Machinery checks, original wrapper and all 49 catalog examples. New tests cover
all Definition arithmetic operations against Machinery, both engines, byte
order, register/byte wrapping, signed fields, branch alignment, invalid rules,
fuel, source bounds, generated C execution, SSA parse/format, PHI structure,
source diagnostics, registry rollback, output preservation and CLI behavior.
Changed public C headers compile independently under C11 and C++11; new C++
headers compile independently under C++17. The refinement suite also passes
under `-O2 -DNDEBUG`. ASan/UBSan passes with leak detection disabled because of
the environment's existing LeakSanitizer restriction.

A Library search found an older `DSLtk.hpp` (SHA-256
`473d6ab3a9356b5f96064f5a02b3f3d4f552909bf4dcf6f95acf532a41780e7f`).
Compiling the actual supplied MetaML against it fails because the header lacks
`dsl::Diagnostic`, `dsl::Severity`, `dsl::Span` and `dsl::BasicToken` expected by
MetaML. It has not been substituted for the required matching dependency.
The MetaML adapter, MetaTeX adapter and libdrm hardware branch remain unvalidated.
CMake remains unavailable here; the fallback build is the observed path.
Forty-four catalog entries still do not implement native guest runtimes.
