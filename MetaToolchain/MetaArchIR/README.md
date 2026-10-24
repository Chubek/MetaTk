# MetaArchIR: portable assembly

Read the [35-chapter manual](manual/README.md) for tutorials, language and API
references, CLI usage, worked examples, and verification guidance.

MetaArchIR is a C++20 assembly builder and `.mair` language implemented with
**DSLtk parser combinators**. It loads MetaArchDB through `metaarch::Spec::load_dsl`
and encodes through `Spec::encode` / `Spec::encode_form`. Database specifications
are parsed and validated, not scanned for declarations.

Instructions, physical registers, operand bindings, labels, sections and bytes
are explicit. Retargeting selects another database target and its encoders; it
does not translate an algorithm or invent an instruction mapping. There are no
optimization passes, analysis framework, virtual registers or register allocator.

## Build and run

From the MetaArch directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
build/marchir --db MetaArchDB --list-targets
build/marchir -T riscv64 --db MetaArchDB --emit hex \
  MetaArchIR/examples/cli/riscv64/decrement.mair -o -
# 1305f5ff67800000
ctest --test-dir build --output-on-failure -R 'metaarch_ir|mair_example'
```

`ARCHDB_BUILD_DSL=ON` enables the library, parser and CLI;
`ARCHDB_BUILD_TOOLS=ON` enables `marchir`. `ARCHDB_DSLTK_DIR` points at the directory
containing `DSLtk.hpp` (defaults to `../DSLtk`). Consumers in the CMake build link
`MetaArch::ir` and include `MetaArchIR/MetaArchIR.hpp`. The implementation is inline,
but its database operations require the MetaArch DSL and C libraries. This replaces
the old standalone, regex-based header's dependency-free build command.

The CLI installs with the existing tools; headers and MetaArchDB install under
`include/metaarch` and `share/metaarch`. An installed-header consumer must also
provide DSLtk's include directory and link the MetaArch DSL/C libraries. Set
`METAARCHDB` to the installed database directory or pass `--db`.

Database lookup: explicit path, `METAARCHDB`, `./MetaArchDB`, then `../MetaArchDB`.
There is no implicit download or external assembler invocation by the library.

## C++ API

```cpp
#include <MetaArchIR/MetaArchIR.hpp>

auto target = mair::target("riscv64", "MetaArchDB");
auto forms = target.forms("addi"); // all matching indices; no guessed overload
mair::Program p;
p.section(".text").global("decrement").label("decrement")
 .form(forms.at(0), {{"rd", mair::reg("x10")},
                     {"rs1", mair::reg("x10")}, {"imm", mair::imm(-1)}})
 .encode("ret");
auto image = mair::assemble(p, target);
auto bytes = image.flat();
auto gas = mair::render(p, target);
```

| API | Contract |
| --- | --- |
| `Program::emit` / `ins` | Native mnemonic and assembler-dialect operand text; supported mnemonic/alias checked by `render`. |
| `Program::encode` | Exact database encoder and named register/integer/symbol bindings; VM constraints apply. |
| `Program::form` | Form index and **declared operand order**, with explicit encoder parameter names; database kind, register class/width, immediate bounds and feature checks apply. |
| `section`, `global`, `label`, `align`, `ascii`, `data`, `comment` | Fluent assembly directives; duplicate labels and invalid names rejected. `align` is bytes, power of two up to 65536. |
| `reg`, `imm`, `symbol` | Physical register, signed 64-bit immediate, or symbol reference with signed addend. |
| `Target::forms`, `parameters`, `register_info` | Query database forms, encoder parameter names and physical registers. Parameter enumeration is **not** a form operand-order mapping. |
| `Target::spec`, `description` | Const access to the underlying public MetaArch API and selected target record. |
| `parse` | Owned program from source; optional source filename and limits. |
| `render` | GAS-style text; checked encoders/forms become exact `.byte` sequences. |
| `assemble` | Owned section image, symbol offsets and relocation records. |
| `Image::flat` | Bytes from zero or one section; rejects unresolved relocations and multiple sections. |
| `read_file`, `compile_file` | Bounded file input; compilation validates before opening output and refuses an input/output alias. |

`Program`, `Image` and ordinary returned strings/vectors own their contents.
`Target` copies share an immutable loaded specification. `register_info` copies
the record, but its string fields, `spec()` and `description()` borrow from that
shared specification. Keep a Target copy alive while using them. Never mutate the
underlying database through C pointers. Const operations can run concurrently;
mutation of one Program requires caller synchronization. No lazy backend load is
performed. `statements()` is a const view, invalidated by program mutation.

Semantic failures throw `mair::Error`, with `location.file/line/column` for parsed
statements. File-system operations may also throw `std::filesystem::filesystem_error`
and allocation may throw `std::bad_alloc`. Parsing/emission returns no partial
result on failure. `Target{}` and moved-from targets reject database access.

### Encoding and relocation boundaries

The low-level `encode` operation does not imply form constraints. Prefer `form`
when operand validation is required. Form bindings are explicit: do not infer
operand order from the encoder's first-use parameter order. The underlying API
rejects hidden operands/custom constraints and unsupported form operand kinds;
MetaArchIR preserves those errors rather than bypassing them.

Native instructions use the target assembler's operand syntax. `render` checks
their mnemonic, not their register classes or operand encodability. For x86,
native integer suffixes `b/w/l/q` are accepted against a declared base mnemonic;
conditional branch names come from the database's condition table. Native text
is intended for an external assembler and is rejected by `assemble`.

Symbol inputs use `encode`, for example `sym=@callee-4`. They must correspond to
an emitted relocation parameter. The image retains the symbol name, addend,
section/field offset and size, bit offset/width, relocation name and PC-relative
flag. Numeric relocation inputs are retained too, with an empty symbol name.
Relocations are **not resolved**, including references to local labels: arbitrary
database relocation expressions belong to a linker. Relocatable encodings cannot
be rendered as plain `.byte` text or silently flattened. Use JSON/image output.
Object-file writing, branch relaxation and symbolic checked forms are not provided.

Encoding coverage and correctness follow the selected MetaArchDB specification;
this layer does not repair or supplement incomplete ISA encoders.

## Language

One statement per line (LF or CRLF). A final newline is optional. Labels occupy
their own line. Spaces/tabs separate the opcode from operands. Directives accept
an optional leading dot. `//` starts an unquoted trailing comment; `#` starts a
whole-line comment only, preserving ARM native immediates such as `#42`.

```text
section .text
global decrement
decrement:
form 11, rd=x10, rs1=x10, imm=-1
encode ret

section .rodata
align 8
message:
ascii "Hello\n\000"
word 0x1234
long -1
```

```ebnf
line       = [ statement ], [ comment ], newline ;
statement  = identifier, ":"
           | ["."], ("section" | "global" | "globl"), identifier
           | ["."], ("align" | "balign"), integer
           | ["."], "ascii", string
           | ["."], ("byte" | "word" | "long" | "quad"), value, {",", value}
           | ["."], "encode", identifier, {",", binding}
           | ["."], "form", nonnegative_integer, {",", binding}
           | identifier, [native_operand, {",", native_operand}] ;
binding    = identifier, "=", (integer | identifier | symbol) ;
symbol     = "@", identifier, [("+" | "-"), unsigned_integer] ;
value      = integer | identifier ;
```

Identifiers use ASCII letters, `_` or `.` initially, then letters, digits, `_`,
`.` or `$`. Integers are signed 64-bit decimal, `0x` hex, `0b` binary or `0o` octal;
leading zero alone does not change the base. `byte/word/long/quad` always mean
1/2/4/8 bytes. Values must fit signed or unsigned width (still within int64).
Symbol data values are supported in text output only. Numeric data is normalized
to decimal; text output uses `.2byte/.4byte/.8byte` so assembler-specific `.word`
widths cannot change its meaning. These spellings are accepted on input too.

Strings are byte strings: `\n`, `\r`, `\t`, `\"`, `\\`, and one-to-three-digit
octal escapes in 0..255. NUL/non-ASCII bytes round-trip using three-digit octal.
`ascii` adds no terminator. Quotes, escapes and nested `()`, `[]`, `{}` protect
commas in native operands. Empty operands, trailing commas, mismatched delimiters,
control characters and statement-injection separators are errors.

Default limits are 16 MiB input/output, one million parsed/emitted statements,
and 64 levels of operand nesting. Pass `Limits` to override budgets; native
builder operands retain a hard nesting ceiling of 64. Layout padding is zero;
instruction-safe alignment is the caller's responsibility.

## CLI

`marchir --help` lists every option. Default output replaces the input extension
with `.s`, `.bin`, `.hex` or `.json`; `-` means stdin/stdout. Stdin defaults to stdout.
`--check` validates the chosen output mode without opening output. `--max-bytes`
bounds input, encoded bytes and serialized output. `--` ends options.

```sh
build/marchir -T riscv64 --db MetaArchDB --list-forms
build/marchir -T riscv64 --db MetaArchDB --list-encoders
build/marchir -T riscv64 --db MetaArchDB --list-registers
build/marchir -T amd64 --db MetaArchDB --emit json \
  MetaArchIR/examples/cli/amd64/call.mair -o call.json
```

JSON schema version 1 contains `target`, `target_index`, `sections` (name,
alignment, hexadecimal bytes), `symbols` (name, section index, offset, global),
and `relocations` (name, parameter, symbol, section, offset, size, value,
bit_offset, bit_width, byte_order, pc_relative). `byte_order` is the emitted field's
actual order, which can differ from the selected target. Offsets/sizes are bytes except explicit bit
fields. It is a relocatable section image, not an ELF/COFF/Mach-O object.

Exit statuses: **0** success, **1** invalid source/target, **2** usage error,
**3** I/O failure. Diagnostics go to stderr; generated output goes to the selected
file or stdout. Target loading failures, including a missing target, are status 1.

## Examples and verification

All eleven API examples build and run in CTest. `Compute.cpp` emits sum-of-squares;
`GCD.cpp` emits Euclid's subtraction algorithm with zero-input handling. Native
execution is checked on x86-64 Linux hosts. Additional examples cover checked forms,
retargeting, section layout and external relocations. CLI examples include native
amd64 assembly, AArch64 addition and RISC-V decrement.

Tests cover all 28 database loads, signed immediate boundaries, independent
encoding anchors, endian-specific data, symbols/relocations, parsing errors,
byte-string round-trips, budgets, CLI exit codes and preservation of output on
validation failure. When installed, `llvm-mc` and `llvm-objcopy` independently
cross-check the AArch64 and RISC-V examples.
