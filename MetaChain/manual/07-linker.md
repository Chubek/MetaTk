# 7. LDtk ELF64 static linker

## Inputs and outputs

LDtk links supported little-endian x86-64 ELF `ET_REL` objects into a static,
non-PIE `ET_EXEC` image. Input bytes may come from ASMtk or another ELF producer.
No process is spawned and no system `ld` is called by the library.

```cpp
ld_tk::Linker linker;
auto linked = linker.link({first_object, second_object}, {"first.o", "second.o"});
if (linked) {
    bool written = ld_tk::Linker::write_file("program", linked.image);
}
```

`Result` contains `image`, component diagnostics, and a DSLtk AST `report`.
The report describes entry, output section addresses/offsets/sizes, and image
size. It is human-inspectable metadata, not a stable replacement for the ELF
format. No usable image is emitted on validation or relocation failure.

`link_files(paths)` reads inputs through bounded standard streams. `read_file`
returns an optional byte vector; `write_file` reports stream/close failures.
These helpers do not set executable permissions or provide atomic replacement.
For deployment, use your application's atomic-write and permission policy.

## Structural validation

The linker validates magic, ELF class/data/version, machine, input type, header
sizes, section-header ranges, string-table indices/ranges, section bounds,
power-of-two alignment, the null section, and symbol/relocation tables. ELF
section/symbol names must terminate within the table and fit 4096 bytes.

The symbol table must have the normal null first symbol and consistent
local/global ordering. Defined symbols must fit their containing sections.
Unsupported bindings/types, invalid section indices, or malformed relocation
entries produce errors rather than unchecked pointer access.

The implementation's packed ELF structures require a little-endian host.
This is checked explicitly; a big-endian host does not silently generate
host-endian ELF structures under a little-endian ELF identifier. MCIL remains
host-independent because its codec writes fields separately.

## Allocated sections and layout

Allocated `PROGBITS`, `NOBITS`, and `NOTE` sections are supported. Compatible
sections merge by name, type, and flags, with alignment respected at each input
contribution. `NOBITS` is materialized as zeros, not implemented as a sparse
file or zero-filesize load segment.

GNU property notes are copied as allocated note bytes; the output does not
announce a GNU-property program header or enable CET from those notes. Do not
use this linker when a deployment requires propagation/enforcement of a
particular ELF security-property protocol.

The executable contains a read-only header load segment, separately
permissioned page-aligned section load segments, and `PT_GNU_STACK` with read/
write but no execute permission. Writable/executable input sections are
rejected. The linker does not collapse all code and data into an RWX segment.

The default image base is `0x400000` and must be page-aligned. File offsets and
virtual addresses are checked for overflow, and per-section/combined/output
limits are enforced before an image is returned. The output intentionally has
no section-header/symbol table: operating-system loading uses program headers.
Use the DSLtk report or retain the input objects for debugging metadata.

## Symbols

Local symbols resolve within their own object. Strong global definitions must
be unique. Weak definitions are fallbacks; a strong definition overrides a
weak one. An unresolved weak reference has value zero. `SHN_ABS` definitions
are supported. Common symbols, TLS, GNU unique symbols, and indirect functions
are not supported.

The default entry symbol is `_start`. It must resolve to an address strictly
inside an executable output section. A symbol at a section's end or in writable
data is not accepted as a valid entry point.

`allow_undefined` is retained in `Options` for source compatibility but true is
rejected for static `ET_EXEC`: there is no runtime resolver to fill missing
definitions. Missing referenced strong symbols are errors.

## Relocation formulas

Let `S` be symbol address, `A` the signed RELA addend, and `P` the address of the
relocation field:

| Relocation | Width | Formula/check |
|---|---|---|
| `R_X86_64_NONE` | none | ignored |
| `R_X86_64_64` | 8 | `S + A`, bounded 64-bit representation |
| `R_X86_64_PC32` | 4 | `S + A - P`, signed 32-bit |
| `R_X86_64_PLT32` | 4 | same static relative formula; no dynamic PLT |
| `R_X86_64_32` | 4 | `S + A`, unsigned 32-bit |
| `R_X86_64_32S` | 4 | `S + A`, signed 32-bit |

The arithmetic does not require compiler-specific `__int128` and does not
overflow signed C++ arithmetic. A relocation must fit within its original
input target section, not merely within a larger merged output section.
An input contribution cannot use a malformed relocation to overwrite the next
object's contribution.

Only `SHT_RELA` is supported. `SHT_REL`, extended indices/counts, multiple
symbol tables, unsupported allocated flags, unsupported allocated section
types, and unlisted relocation types are rejected. Nonallocated debug content
is not emitted or relocated into a debugger-readable output section.

## Two-object example

```text
; start.s
.global _start
.extern finish
_start: call finish
```

```text
; finish.s
.global finish
finish:
mov rdi, 7
mov rax, 60
syscall
```

```sh
build/metachain object start.s -o start.o
build/metachain object finish.s -o finish.o
build/metachain link start.o finish.o -o program
chmod +x program
./program
```

## Deliberate exclusions

No archive member extraction, shared libraries, dynamic relocations, PIE/PIC
runtime support, GOT synthesis, TLS layout, COMDAT selection, symbol versioning,
linker scripts, relaxation, section garbage collection, or platform C-runtime
startup is implemented. Compile system-language objects with those features
disabled only when their remaining ELF constructs meet the supported contract.
When building conventional hosted C/C++ programs, use the platform toolchain
unless you have audited every emitted requirement.
