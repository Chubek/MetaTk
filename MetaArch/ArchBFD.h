/*
 * ArchBFD.h — catalogue of BFD object-format descriptions.
 *
 * The ArchDB catalogue (ArchDB/x.dsymspec.yaml) describes instructions:
 * which encodings exist, how operands are constrained and how a machine
 * word is produced.  ArchBFD describes the *container*: the object file
 * formats that BFD (binutils/bfd) reads and writes — ELF, COFF, PE,
 * Mach-O, ECOFF and WebAssembly — with the byte layout of their headers,
 * their numeric constants and their relocation ("howto") tables.
 *
 * A document (.bfd.yaml, schema 1) looks like this:
 *
 *   schema: 1
 *   name: ELF64
 *   format: elf
 *   byte_order: little
 *   target_vector: elf64-x86-64
 *   arch: x86-64
 *   records:
 *     - name: ehdr
 *       size: 64
 *       fields:
 *         - {name: e_ident, offset: 0, size: 16}
 *   constants:
 *     - name: section_types
 *       items:
 *         - {name: SHT_PROGBITS, value: 1}
 *   relocations:
 *     - {name: R_X86_64_PC32, number: 2, size: 4, bitsize: 32, pcrel: true}
 *
 * A document may inherit the container layout of another one:
 *
 *   name: hppa
 *   container: ELF32        # records and constants come from ELF32
 *   relocations: [...]      # but the relocations are the HP PA-RISC ones
 *
 * Loading:
 *   ArchBFDSpec *s = archbfd_spec_load_file("ArchBFD/ELF64.bfd.yaml",
 *                                            DSYM_FORMAT_AUTO, 0, &diags);
 *   archbfd_spec_free(s);
 *
 * Every string and array belongs to the document's arena and is released by
 * archbfd_spec_free().  Lookup helpers return borrowed pointers that stay
 * valid until the document is freed; a resolved document is read-only and
 * may be read from several threads at once.
 */
#ifndef METAARCH_ARCHBFD_H
#define METAARCH_ARCHBFD_H

#include "DsymspecIR.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ARCHBFD_SCHEMA_VERSION 1
#define ARCHBFD_NAME_MAX 128

/* ---- enumerations (order == schema order) ------------------------------- */

typedef enum ArchBFDFormat {
    ARCHBFD_FMT_ELF = 0,
    ARCHBFD_FMT_COFF,
    ARCHBFD_FMT_PE,
    ARCHBFD_FMT_MACHO,
    ARCHBFD_FMT_ECOFF,
    ARCHBFD_FMT_WASM,
    ARCHBFD_FMT_OTHER
} ArchBFDFormat;

typedef enum ArchBFDByteOrder {
    ARCHBFD_BO_LITTLE = 0,
    ARCHBFD_BO_BIG
} ArchBFDByteOrder;

/* BFD's enum complain_overflow (bfd/bfd-in2.h). */
typedef enum ArchBFDOverflow {
    ARCHBFD_OVF_DONT = 0,
    ARCHBFD_OVF_BITFIELD,
    ARCHBFD_OVF_SIGNED,
    ARCHBFD_OVF_UNSIGNED,
    ARCHBFD_OVF_TRUNCATE,
    ARCHBFD_OVF_ERROR
} ArchBFDOverflow;

/* ---- records ------------------------------------------------------------ */

typedef struct ArchBFDEncoding {
    const char *field; /* field the bits live in */
    const char *target; /* what the bits mean: type, symbol, bind, ... */
    uint64_t mask;
    int64_t shift;
} ArchBFDEncoding;

typedef struct ArchBFDField {
    const char *name;
    int64_t offset; /* byte offset inside the record */
    int64_t size;   /* size in bytes */
    const char *desc;
} ArchBFDField;

typedef struct ArchBFDRecord {
    const char *name;
    const char *desc;
    int64_t size; /* total size in bytes */
    ArchBFDField *fields;
    size_t field_count;
    ArchBFDEncoding *encodings;
    size_t encoding_count;
} ArchBFDRecord;

typedef struct ArchBFDConstant {
    const char *name;
    int64_t value;
    const char *desc;
} ArchBFDConstant;

typedef struct ArchBFDConstantGroup {
    const char *name;
    const char *desc;
    ArchBFDConstant *items;
    size_t count;
} ArchBFDConstantGroup;

/* A BFD reloc_howto_type entry. */
typedef struct ArchBFDReloc {
    const char *name;
    int64_t number;  /* the format's relocation number */
    int64_t size;    /* bytes operated on (BFD size code translated) */
    int64_t bitsize; /* bits occupied in the instruction */
    bool pcrel;      /* PC relative */
    bool pcrel_offset;
    bool partial;    /* BFD partial_inplace */
    ArchBFDOverflow overflow;
    int64_t rightshift;
    int64_t bitpos;
    uint64_t src_mask;
    uint64_t dst_mask;
    const char *desc;
} ArchBFDReloc;

typedef struct ArchBFDSection {
    const char *name;
    const char *desc;
} ArchBFDSection;

typedef struct ArchBFDSource {
    const char *file; /* file inside the binutils tree */
    const char *note;
} ArchBFDSource;

/* ---- a document --------------------------------------------------------- */

typedef struct ArchBFDSpec ArchBFDSpec;

struct ArchBFDSpec {
    DsymArena *arena;
    const char *source_path; /* file it was loaded from, if any */
    const char *base_dir;    /* directory of source_path ("." if none) */

    int64_t schema;
    const char *name;
    ArchBFDFormat format;
    bool has_format;
    const char *description;
    ArchBFDByteOrder byte_order;
    bool has_byte_order;
    const char *target_vector;
    const char *arch;
    const char *container;    /* name of the container document */
    int32_t container_index;  /* resolved index in the catalogue, -1 if none */
    const char *magic;        /* magic bytes, hex encoded */
    size_t magic_len;         /* number of bytes (not hex characters) */
    const char **flags;
    size_t flag_count;
    ArchBFDSource *sources;
    size_t source_count;
    ArchBFDRecord *records;
    size_t record_count;
    ArchBFDConstantGroup *constants;
    size_t constant_group_count;
    ArchBFDReloc *relocations;
    size_t reloc_count;
    ArchBFDSection *sections;
    size_t section_count;

    /* The container document, when the catalogue resolved it.  Borrowed: it
       stays alive as long as the catalogue does. */
    const ArchBFDSpec *container_spec;
    /* A container document this document loaded for itself.  Owned. */
    struct ArchBFDSpec *owned_container;

    struct ArchBFDIndex *index; /* private lookup tables */
};

/* ---- catalogue ---------------------------------------------------------- */

typedef struct ArchBFDEntry {
    const char *name;         /* catalogue key, e.g. "ELF64" */
    const char *file;         /* path relative to the catalogue root */
    const char *path;         /* resolved path */
    const char *format;       /* "elf", "coff", ... */
    const char *byte_order;   /* "little" / "big" */
    const char *arch;         /* emulated architecture */
    const char *target_vector; /* BFD target vector name */
} ArchBFDEntry;

typedef struct ArchBFD ArchBFD;

/* Open a catalogue file (any supported format) or scan a directory for
   *.bfd.{yaml,yml,json,xml,sexp}.  Returns NULL on failure. */
ArchBFD *archbfd_open(const char *path, DsymDiags *diags);
ArchBFD *archbfd_scan(const char *dir, DsymDiags *diags);
void archbfd_close(ArchBFD *db);

size_t archbfd_count(const ArchBFD *db);
const ArchBFDEntry *archbfd_entry(const ArchBFD *db, size_t i);
/* Lookup by catalogue name, then by target vector, then by architecture. */
const ArchBFDEntry *archbfd_find(const ArchBFD *db, const char *name);

/* Lazily load (and cache) a document. */
const ArchBFDSpec *archbfd_load(ArchBFD *db, const ArchBFDEntry *e, uint32_t flags,
                                 DsymDiags *diags);
const ArchBFDSpec *archbfd_load_name(ArchBFD *db, const char *name, uint32_t flags,
                                     DsymDiags *diags);

/* Serialise the catalogue. */
bool archbfd_write_catalogue(const ArchBFD *db, DsymFormat fmt, const char *root,
                              DsymBuf *out, DsymDiags *diags);

/* ---- documents ---------------------------------------------------------- */

enum {
    ARCHBFD_LOAD_STRICT = 1u << 0, /* warnings count as errors */
    ARCHBFD_LOAD_LINT = 1u << 1    /* extra consistency checks */
};

ArchBFDSpec *archbfd_spec_load_file(const char *path, DsymFormat fmt, uint32_t flags,
                                     DsymDiags *diags);
ArchBFDSpec *archbfd_spec_load_buffer(const char *data, size_t len, DsymFormat fmt,
                                       const char *virtual_path, uint32_t flags,
                                       DsymDiags *diags);
void archbfd_spec_free(ArchBFDSpec *spec);

/* Model -> canonical document tree -> wire format. */
bool archbfd_spec_write(const ArchBFDSpec *spec, DsymFormat fmt, uint32_t flags,
                        DsymBuf *out, DsymDiags *diags);

/* ---- lookup ------------------------------------------------------------- */

/* Relocation by name (exact match) or by number; -1 when not found. */
int32_t archbfd_find_reloc(const ArchBFDSpec *s, const char *name);
int32_t archbfd_find_reloc_number(const ArchBFDSpec *s, int64_t number);
const ArchBFDRecord *archbfd_find_record(const ArchBFDSpec *s, const char *name);
/* Constant lookup: group and name must both match. */
bool archbfd_constant(const ArchBFDSpec *s, const char *group, const char *name,
                      int64_t *value);
/* Lookups that follow the container chain: a document without a record of its
   own uses the one of its container. */
const ArchBFDRecord *archbfd_resolve_record(const ArchBFDSpec *s, const char *name);
bool archbfd_resolve_constant(const ArchBFDSpec *s, const char *group, const char *name,
                               int64_t *value);

/* ---- object files ------------------------------------------------------- */

/* An object file described by an ArchBFD document.  The reader is driven by
   the description: the record layouts, the constants and the relocation
   table come from the document, so a different description reads a different
   container without new code. */

typedef struct ArchBFDObject ArchBFDObject;

typedef struct ArchBFDObjectSection {
    const char *name; /* section name ("" when the section is unnamed) */
    int64_t type;     /* sh_type / s_type */
    uint64_t flags;
    int64_t offset; /* file offset of the contents */
    int64_t size;
    int64_t addr;   /* virtual address */
    int64_t align;
    int64_t link;   /* index of the related section */
    int64_t info;   /* extra information (the target section of a relocation) */
    int64_t entsize;
    int64_t rel_offset; /* file offset of the section's relocations (COFF) */
    int64_t rel_count;  /* number of relocations (COFF) */
    size_t index;
} ArchBFDObjectSection;

typedef struct ArchBFDObjectSymbol {
    const char *name;
    int64_t value;
    int64_t size;
    int64_t shndx;
    uint8_t info;
    uint8_t other;
    const char *bind; /* "local", "global", ... (ELF) */
    const char *type; /* "func", "object", ... (ELF) */
    int64_t type_value;  /* numeric symbol type (ELF st_info, COFF e_type) */
    int64_t class_value; /* numeric storage class (ELF binding, COFF e_sclass) */
    size_t index;
} ArchBFDObjectSymbol;

typedef struct ArchBFDObjectReloc {
    int64_t offset;      /* offset inside the target section */
    int64_t type;        /* relocation number */
    int64_t symbol;      /* symbol index, -1 when unknown */
    int64_t addend;
    const char *type_name;   /* from the description, NULL when unknown */
    const char *symbol_name; /* from the symbol table, NULL when unknown */
    size_t index;
} ArchBFDObjectReloc;

/* Read an object file with the given description.  Returns NULL on failure
   and adds a diagnostic.  The description is borrowed; it must outlive the
   object. */
ArchBFDObject *archbfd_object_open(const char *path, const ArchBFDSpec *spec,
                                    DsymDiags *diags);
/* Same, for an image already in memory (the buffer is copied). */
ArchBFDObject *archbfd_object_open_memory(const void *data, size_t len,
                                           const ArchBFDSpec *spec, DsymDiags *diags);
void archbfd_object_close(ArchBFDObject *obj);

size_t archbfd_object_section_count(const ArchBFDObject *obj);
const ArchBFDObjectSection *archbfd_object_section(const ArchBFDObject *obj, size_t i);
size_t archbfd_object_symbol_count(const ArchBFDObject *obj);
const ArchBFDObjectSymbol *archbfd_object_symbol(const ArchBFDObject *obj, size_t i);
size_t archbfd_object_reloc_count(const ArchBFDObject *obj);
const ArchBFDObjectReloc *archbfd_object_reloc(const ArchBFDObject *obj, size_t i);
/* The container kind that was read, e.g. "elf64" or "coff". */
const char *archbfd_object_kind(const ArchBFDObject *obj);
/* The machine type the object declares (ELF e_machine, COFF machine). */
int64_t archbfd_object_machine(const ArchBFDObject *obj);
/* The object type the object declares (ELF e_type, 1 = ET_REL). */
int64_t archbfd_object_type(const ArchBFDObject *obj);

/* Spelling of the enumerations. */
const char *archbfd_format_name(ArchBFDFormat f);
const char *archbfd_byte_order_name(ArchBFDByteOrder v);
const char *archbfd_overflow_name(ArchBFDOverflow v);

/* Decode the magic of a document into bytes.  Returns the number of bytes
   written (0 when the document has no magic or `cap` is too small). */
size_t archbfd_magic_bytes(const ArchBFDSpec *s, uint8_t *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif /* METAARCH_ARCHBFD_H */
