# MetaAS

MetaArch-backed, two-pass assembler in C++20 with a C11-compatible API.
MetaArch loads architecture specifications and encodes instructions; MetaAS adds
source expressions, labels, sections, data directives, and object metadata.

## Build

From `MetaChain`:

```sh
cmake -S MetaAS -B build/metaas -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build/metaas -j
ctest --test-dir build/metaas --output-on-failure
build/metaas/metaas --spec ../MetaArch/ArchDB/ppc32.dsymspec.yaml \
    --format hex MetaAS/examples/ppc32.asm
build/metaas/metaas --spec MetaAS/tests/fixture.march \
    --format hex MetaAS/examples/toy.asm
```

`cmake -S . -B build` also builds MetaAS through the MetaChain project.
`METAAS_METAARCH_DIR` defaults to the sibling `MetaArch` directory. The dependency
uses its existing libyaml, yyjson, libxml2 and DSLtk setup (system libraries first,
FetchContent fallback). For an offline checkout, pass e.g.
`-DFETCHCONTENT_SOURCE_DIR_YYJSON=/path/to/yyjson`.

Options: `METAAS_BUILD_TOOLS`, `BUILD_TESTING`, `BUILD_SHARED_LIBS`,
`METAAS_WARNINGS_AS_ERRORS` (default ON), and `METAAS_SANITIZERS` (ASan/UBSan).
Sanitizer instrumentation also covers the bundled MetaArch dependency.
`cmake --install build/metaas --prefix /desired/prefix` installs the library,
headers, CLI, and the dependency's installation files. For CMake consumers, use
`add_subdirectory(path/to/MetaAS)` and link `MetaAS::MetaAS` to carry dependencies
automatically. Shared-library consumers can link `-lmetaas`; static consumers
also need MetaArch's DSL/core libraries and their codec dependencies.

## CLI

```sh
metaas --spec ARCH --list-encoders
metaas --spec ARCH --target 0 --origin 0x1000 --format bin -o code.bin code.asm
metaas --spec ARCH --bind addi=rd,ra,imm --format hex code.asm
metaas --spec ARCH --format json -o code.mas.json code.asm
```

Architecture inputs: `.march`, YAML, JSON, XML, and S-expression specifications.
Input `-` means stdin; output defaults to stdout. `--format` is `bin`, `hex`, or
`json`. Errors have filename, line and column when available. Exit codes:
0 success, 1 invalid source/specification or unresolved binary, 2 command-line
usage error, 3 I/O/runtime failure. Assembly completes before output is opened.

### Instructions and bindings

```asm
.insn addi rd=3, ra=3, imm=-1   # low-level encoder, named parameters
mov dst=r1, imm=42             # register names accepted
mov r1, 42                    # requires an explicit mov=[dst,imm] binding
```

Mappings come from `.march` `assembly` declarations, `--bind ENC=P1,P2`, or
`Options::bindings`. Parameter order is never inferred from encoder bytecode.
With a binding, regular instructions check visible form operands, register
class/width, immediate bounds, and target features. Hidden/implicit operands
are omitted from the mapping. Named instructions without bindings and `.insn`
use low-level encoding, including encoder `REQUIRE` checks. `.insn` deliberately
bypasses form checks. This permits the existing database's raw encoders to be
used without inventing missing operand mappings.

Mnemonic aliases are supported when they select one encoder. Ambiguous
mnemonics require an explicit encoder name. This is an encoder-oriented syntax,
not a GAS/NASM parser; complex memory operands and custom form constraints
require explicit low-level parameters.

### Labels, expressions, directives

Identifiers use ASCII letters, digits, `_`, `.`, and `$`; the first character
cannot be a digit. `.` alone means the current absolute address. Labels may
precede an instruction/directive on the same line. `#` and `;` start comments
outside strings.

Expressions support parentheses, unary `+ - ~`, and C-precedence binary
`* / % + - << >> & ^ |`. Arithmetic overflow, invalid divisions, and shifts
outside 0–63 are errors. Shifts operate on 64-bit bit patterns (right shift is
logical). Decimal integers are signed 64-bit; hexadecimal `0x`, binary `0b`,
and octal `0o` literals may specify all 64 bits. Registers are whole instruction
operands, not expression variables.

| Directive | Meaning |
|---|---|
| `.text`, `.data`, `.rodata`, `.bss` | Select/create a section |
| `.section NAME` | Select/create a named section |
| `label:` | Define a section-relative symbol |
| `.global NAME,...`, `.globl NAME,...` | Mark defined/external symbols global |
| `.extern NAME,...` | Declare external symbols |
| `.equ NAME, EXPR` | Define an immutable absolute constant |
| `.byte`, `.short`, `.word`, `.quad` | Emit 1/2/4/8-byte expressions in target byte order |
| `.ascii "...",...`, `.asciz "...",...` | Emit bytes; `.asciz` terminates each string |
| `.zero N`, `.space N[, FILL]` | Emit zero/fill bytes |
| `.align N[, FILL]` | Power-of-two byte alignment |
| `.org OFFSET[, FILL]` | Advance to a section-relative offset |

String escapes: `\n \r \t \0 \\ \" \xHH`. Layout arguments and `.equ` may use
only previously defined absolute constants. Sections appear in first-use order,
are aligned after the preceding section, and resume on subsequent selection.
`.bss` currently emits zero bytes like any other section; no NOBITS format.

Defaults per assembly: 16 MiB source and output, 100,000 statements/symbols,
256 sections, and expression nesting 64 (hard ceiling 256). CLI byte limits
are configurable with `--max-source` and `--max-output`; C++ exposes all limits.
Section addresses are limited to signed 64-bit.

## Output and relocation semantics

`bin` and `hex` use the selected origin and concatenate aligned sections,
including zero-filled inter-section padding. Absolute data references to local
symbols are resolved. Undefined symbols must be explicitly declared external.
Architecture-specific relocation records, including references to local labels,
require a linker and cause binary output to fail.
Symbolic operand field-range checks are deferred until linking; their addends
are not treated as final operand values.

`json` writes deterministic **MetaAS version 1** metadata: architecture/target,
origin, section addresses/alignment/hex bytes, symbols (section index or null),
and relocations with field size, bit offset/width, byte order, symbol, addend,
parameter, expression, overflow and addend policies. `data=true` marks MetaAS
absolute data relocations; other records come from MetaArch. It is a custom
object interchange format, not ELF/COFF/Mach-O, and no linker is included.

Symbolic data/relocation expressions support `symbol + addend` and differences
between defined symbols in the same section. Instruction operands without an
`EMIT_RELOCATION` parameter resolve local labels to the fixed layout's absolute
addresses. `.` is also absolute. Such values are already baked into the bytes;
consumers must preserve those addresses. MetaAS does not infer PC-relative
semantics, relax branches, or infer additional fixups. ISA coverage and encoder
correctness follow the selected MetaArch specification.

## APIs

`include/MetaAS.hpp` exposes `Assembler::load`, `assemble`, `encoders`,
`target_count`, `Options`, `Image::binary`, and `Image::json`.

```cpp
auto assembler = metaas::Assembler::load("architecture.march");
if (!assembler) { /* inspect assembler.diagnostics */ }
else {
    auto result = assembler.value->assemble("mov r1, 42\n");
    if (result) {
        auto bytes = result.value->binary();
        // Check bytes before accessing bytes.value.
    }
}
```

Loaded assemblers are copyable and read-only; simultaneous assembly calls use
independent state. Results own their strings and bytes. Allocation failures can
throw from C++; other errors use `Result::diagnostics`.

`include/MetaAS.h` provides opaque C11 contexts and images. Create with
`mas_create`, load with `mas_load`, configure with `mas_bind`/`mas_set_*`, and
assemble with `mas_assemble`. Retrieve sections, binary, or JSON with
`mas_image_*`. Images survive context destruction. Binary/JSON pointers expire
on the context's next operation; section pointers survive until image destruction.
Failed loads preserve the previous architecture. Contexts require caller
exclusion, null destroy is safe, and C entry points contain exceptions.

See `tests/CAPI.c` for a complete C example and `REPORT.md` for verification.
