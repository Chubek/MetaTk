# ArchBFD — object-format descriptions

`ArchBFD` is the second half of MetaArch: the architecture catalogue
(`ArchDB/`) describes *instructions*, `ArchBFD/` describes the *containers*
those instructions are packed into — the object file formats that BFD
(binutils/bfd) reads and writes.

```
ArchBFD/ELF64.bfd.yaml     64-bit ELF objects (x86-64 emulation)
ArchBFD/ELF32.bfd.yaml     32-bit ELF objects (i386 emulation)
ArchBFD/hppa.bfd.yaml      ELF32 objects for the HP PA-RISC emulation
ArchBFD/m68k.bfd.yaml      ELF32 objects for the Motorola 68000 emulation
ArchBFD/COFF.bfd.yaml      Common Object File Format (i386 emulation)
ArchBFD/PE.bfd.yaml        Portable Executable (PE32 and PE32+)
ArchBFD/MachO.bfd.yaml     Mach-O objects and images
ArchBFD/irix.bfd.yaml      IRIX ECOFF objects (MIPS emulation)
ArchBFD/WAT.bfd.yaml       WebAssembly binary format
ArchBFD/schema/MetaBFD.schema.json   the document schema
ArchBFDCatalogue.yaml      index of the documents
```

## What a document contains

A document (`.bfd.yaml`, schema 1) describes one BFD target vector:

| Key | Meaning |
|---|---|
| `name` | catalogue key, e.g. `ELF64` |
| `format` | `elf`, `coff`, `pe`, `macho`, `ecoff`, `wasm` or `other` |
| `byte_order` | `little` or `big` |
| `target_vector` | the BFD target vector, e.g. `elf64-x86-64` |
| `arch` | the architecture the backend emulates |
| `container` | name of the document holding the container layout (inherited) |
| `magic` | the magic bytes, hexadecimal |
| `records` | fixed-layout structures: file header, section header, symbol, relocation |
| `constants` | named numbers grouped by topic (`section_types`, `symbol_bindings`, …) |
| `relocations` | the BFD relocation (“howto”) table: number, size, PC-relative, overflow policy |
| `sections` | well-known section names |
| `source` | the binutils files the data was taken from |

Records list their fields with byte offsets and sizes; `encodings` say how a
packed field is split:

```yaml
  - name: rela
    desc: "ELF64 relocation entry (with addend)"
    size: 24
    fields:
      - {name: r_offset, offset: 0, size: 8}
      - {name: r_info, offset: 8, size: 8}
      - {name: r_addend, offset: 16, size: 8}
    encodings:
      - {field: r_info, target: type, mask: 4294967295, shift: 0}
      - {field: r_info, target: symbol, mask: 4294967295, shift: 32}
```

An encoding is read as `(value >> shift) & mask`: the shift comes first, the
mask always describes the width of the extracted part.

## Where the data comes from

The documents are **generated** from a binutils checkout (2.22 was used here)
by `tools/extract_bfd.py`, which parses the byte layouts in
`include/elf/external.h`, `include/coff/*.h` and `include/mach-o/external.h`,
and the relocation tables in `bfd/elf64-x86-64.c`, `bfd/elf32-i386.c`,
`bfd/elf32-m68k.c`, `bfd/elf-hppa.h`, `bfd/coff-i386.c` and `bfd/coff-mips.c`.

```sh
python3 tools/extract_bfd.py                 # regenerate ArchBFD/*.bfd.yaml
python3 tools/extract_bfd.py --check         # fail when they are out of date
python3 tools/extract_bfd.py --binutils /path/to/binutils --out ArchBFD
```

The checkout is an external input and is not part of the repository; the
committed documents work without it.  The generator is deterministic, and the
CTest case `archbfd_extractor_check` keeps the committed documents in sync
with the checkout when one is present.  Two independent checks confirm the
*meaning* of the numbers:

- `tests/check_bfd_schema.py` validates every document against
  `ArchBFD/schema/MetaBFD.schema.json` (needs PyYAML and jsonschema).
- `tests/check_bfd_data.py` compiles a small C file and compares the object
  with the ELF64 description: header sizes, the `r_info` split, the
  relocation numbers and the symbol table.

The one document that does **not** come from binutils is `WAT.bfd.yaml`: the
binutils 2.22 checkout has no WebAssembly BFD back end, so it follows the
WebAssembly core specification instead.  This is stated in the file header.

## API

```c
#include "ArchBFD.h"

DsymDiags diags;
dsym_diags_init(&diags);

ArchBFD *db = archbfd_open("ArchBFDCatalogue.yaml", &diags);
const ArchBFDSpec *spec = archbfd_load_name(db, "ELF64", 0, &diags);

/* Lookups. */
int32_t r = archbfd_find_reloc(spec, "R_X86_64_PC32");
int64_t v = 0;
archbfd_constant(spec, "section_types", "SHT_RELA", &v);

/* Read an object file with the description. */
ArchBFDObject *obj = archbfd_object_open("a.out.o", spec, &diags);
for (size_t i = 0; i < archbfd_object_reloc_count(obj); i++) {
    const ArchBFDObjectReloc *rel = archbfd_object_reloc(obj, i);
    printf("%s %s\n", rel->type_name, rel->symbol_name);
}
archbfd_object_close(obj);
archbfd_close(db);
dsym_diags_free(&diags);
```

The reader is driven by the description: the byte layout of every structure
comes from the record fields, the section and symbol kinds come from the
constant groups and the relocation names come from the relocation table.  Only
the *shape* of the formats (where the symbol table lives, how a relocation
entry is split) is in the code.  ELF, COFF and PE objects can be read today;
Mach-O, ECOFF and WebAssembly are descriptions only.

The reader reports the sections, the symbols and the relocations of an object.
It does not read the program headers, the dynamic information or the section
contents.

A document may inherit the container of another one (`container: ELF32`), so
the HP PA-RISC and m68k documents only carry their own relocations.  The
catalogue resolves the reference when it loads a document.

## CLI

```
archbfd validate [--strict] [--lint] [-i FMT] FILE...
archbfd convert -f yaml|json|xml|sexpr [-o OUT] [-i FMT] FILE
archbfd list [ArchBFDCatalogue.yaml|DIR]
archbfd catalogue ArchBFD --root ArchBFD -o ArchBFDCatalogue.yaml
archbfd info ArchBFD/ELF64.bfd.yaml
archbfd relocs ArchBFD/ELF64.bfd.yaml R_X86_64_PC32
archbfd records ArchBFD/ELF64.bfd.yaml ehdr
archbfd object [--catalogue FILE] [-f FORMAT] OBJECT
```

`archbfd object` picks the description whose magic matches the file (and that
can read it) and prints the sections, symbols and relocations it found.

## Adding a format

1. Add the document to `ArchBFD/`, either by hand or by extending
   `tools/extract_bfd.py`.
2. Register it in `ArchBFDCatalogue.yaml`
   (`archbfd catalogue ArchBFD --root ArchBFD -o ArchBFDCatalogue.yaml`).
3. Extend `ArchBFD/schema/MetaBFD.schema.json` if new keys were introduced.
4. Run the tests: `ctest --test-dir build`.
