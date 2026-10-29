# MuLIB elf: checked ELF32/ELF64 views

**Header:** `include/mulib-elf.h`  
**Implementation switches:** `MULIB_IMPL_ELF`, `MULIB_IMPL_ENDIAN`  
**Link flags:** none. ISO C11, C++17 declaration compatibility.

## Supported scope

This is a non-owning ELF file view, not an executable loader or linker. It
decodes ELF32 and ELF64, both little- and big-endian, regardless of host byte
order/alignment. It never casts input to system ELF structures. Header/table
sizes, count arithmetic, declared file-backed payload ranges, symbol layouts,
and string bounds are checked before access. Larger-than-minimum header entries
are permitted, with their known prefixes decoded.

Supported records are the main header, arbitrary section and program headers,
STRTAB strings, SYMTAB/DYNSYM symbols, extended section/program counts and section
name indexes via section zero, and SHN_XINDEX symbols via SYMTAB_SHNDX. Numeric
fields with architecture-specific meanings remain raw. NOBITS and NULL sections
do not claim file payload bytes. SHN_UNDEF and reserved 16-bit symbol section
indexes remain numeric; nonreserved indexes and resolved extended indexes must
refer to available sections.

No relocations, symbol-version interpretation, DWARF, decompression of compressed
sections, architecture-specific ABI validation, or dynamic dependency loading is
performed. Unknown section/program types can still be viewed as bounded raw
records. This checks structural safety, **not** executable trustworthiness or
complete ELF semantic validity. No system APIs, feature macros, or system ELF
headers are needed.

## Open and lifetime

`mulib_elf_open(&view, bytes, size)` borrows input and updates the output only on
success. Keep all bytes alive and unchanged while the view or borrowed names are
used. The visible `mulib_elf_t` contains class (`bits` 32/64), byte order, machine,
type, entry point, flags, table offsets/strides/counts, and the section-name table
index. Do not edit these fields or manufacture a view instead of calling open.
Copying a view is safe while the underlying bytes remain alive.

Open validates magic, class, encoding/version, minimum header sizes, table
extents, section alignment powers of two, every file-backed section and program
range, PT_LOAD `filesz <= memsz` and offset/address alignment congruence, the
section-name table type, and symbol table strides/linked string table types.
Even zero-sized declared file-backed ranges must begin at/before end of file.
NULL section payload fields are ignored (they carry extended table metadata).
String termination and per-symbol names/index-extension contents are validated
when accessed, so a successful open alone does not validate every symbol name.

## Accessors

Every function returns zero on success. Outputs change only on success. Bounds
checks are against file byte length, never against a memory address derived from
an unchecked file integer.

| Function | Result |
| --- | --- |
| `mulib_elf_section(view, index, &section)` | Decoded `name`, `type`, flags, address, offset, size, link, info, alignment, entry size |
| `mulib_elf_program(view, index, &program)` | Decoded type/flags, file/virtual/physical addresses, file/memory size, alignment |
| `mulib_elf_bytes(view, offset, size, &bytes)` | Borrowed checked range; `(file_size, 0)` is valid |
| `mulib_elf_string(view, strtab, offset, &text, &length)` | Borrowed terminated string with byte length excluding NUL |
| `mulib_elf_section_name(view, index, &text, &length)` | Name via the header's section-name table |
| `mulib_elf_symbol_count(view, symtab, &count)` | Number of entries in SYMTAB/DYNSYM |
| `mulib_elf_symbol(view, symtab, index, &symbol)` | Decoded name offset, value, size, info, other, resolved section index |
| `mulib_elf_symbol_name(view, symtab, index, &text, &length)` | Symbol's name via its linked STRTAB |

Do not treat a NOBITS section's offset/size as file contents. Strings are byte
strings and are not UTF-8 validated. Symbol `info` and `other` are raw bytes;
binding/type/visibility can be decoded with the ELF ABI's masks. An SHN_XINDEX
lookup scans section headers for one uniquely associated SHNDX table, requires
at least 4-byte entries with exactly the symbol count, and checks the result.
Repeated extended-index symbol lookup is thus linear in section count; ordinary
accessors do not allocate.

Results are `EINVAL`, `EFORMAT` (invalid layout/content), `EBOUNDS` (unavailable
range/index), `ETYPE` (wrong table type), or `ENOTFOUND` (no section-name table).
No `errno`, allocation, or mutable global state.

## Runnable example

```c
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_ELF
#include "mulib-elf.h"
#include <assert.h>
int main(void) {
    unsigned char bytes[64] = { 0x7f, 'E', 'L', 'F', 2, 1, 1 };
    mulib_elf_t v;
    bytes[16] = 1; bytes[18] = 62; bytes[20] = 1; bytes[52] = 64;
    assert(mulib_elf_open(&v, bytes, sizeof(bytes)) == 0);
    assert(v.bits == 64 && v.machine == 62 && v.shnum == 0);
    return 0;
}
```

Build `examples/modules/elf.c` with
`cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/elf.c -o elf-example`
then run `./elf-example`. For an actual file, combine with `mulib-mmfile.h` or
`mulib-fs.h` and explicitly define their implementation switches as well.
`tests/test_storage.c` uses unaligned synthetic inputs across both classes and
byte orders, extended counts/indexes, malformed tables/strings, every truncated
prefix, and (on Linux) the running test executable's real headers/symbols.
