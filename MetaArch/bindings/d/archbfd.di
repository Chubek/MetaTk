/++ ArchBFD C API D interface. Link with libmetaarch; this module needs no
    generated C headers and does not depend on the internal model layout.

    Handles are owned: use the matching free function (including with
    scope(exit)).  Documents loaded through a catalogue are owned by that
    catalogue and stay valid until it is closed; strings and view pointers
    are borrowed until then.  Read-only lookups may run in parallel; free,
    load and diagnostics mutation require exclusion.
    NULL create/load means failure; diagnostics are optional.
 +/
module archbfd;
import object : size_t;

struct ArchBFD;
struct ArchBFDSpec;
struct ArchBFDObject;
struct ArchBFDEntry;
struct ArchBFDRecord;
struct ArchBFDObjectSection;
struct ArchBFDObjectSymbol;
struct ArchBFDObjectReloc;
struct DsymArena;

// The diagnostics collector of the core library.  The layout mirrors
// DsymspecIR.h; a D caller owns it and passes it to the calls that report
// problems.  Passing null is allowed and discards the diagnostics.
struct DsymDiag {
    int severity;
    uint line, col;
    const(char)* file;
    const(char)* path;
    const(char)* msg;
}

struct DsymDiags {
    DsymArena* arena;
    DsymDiag* items;
    ulong count, cap, errors, warnings, max_errors;
}

extern(C) nothrow @nogc {
    void dsym_diags_init(DsymDiags* diags);
    void dsym_diags_free(DsymDiags* diags);
}

alias ArchBFDFormat = int;
alias ArchBFDByteOrder = int;
alias ArchBFDOverflow = int;
alias DsymFormat = int;

enum : int {
    ARCHBFD_FMT_ELF = 0,
    ARCHBFD_FMT_COFF = 1,
    ARCHBFD_FMT_PE = 2,
    ARCHBFD_FMT_MACHO = 3,
    ARCHBFD_FMT_ECOFF = 4,
    ARCHBFD_FMT_WASM = 5,
    ARCHBFD_FMT_OTHER = 6,
}
enum : int {
    ARCHBFD_BO_LITTLE = 0,
    ARCHBFD_BO_BIG = 1,
}
enum : int {
    ARCHBFD_OVF_DONT = 0,
    ARCHBFD_OVF_BITFIELD = 1,
    ARCHBFD_OVF_SIGNED = 2,
    ARCHBFD_OVF_UNSIGNED = 3,
    ARCHBFD_OVF_TRUNCATE = 4,
    ARCHBFD_OVF_ERROR = 5,
}
enum : uint { ARCHBFD_LOAD_STRICT = 1, ARCHBFD_LOAD_LINT = 2 }

// The catalogue and the documents it owns.
extern(C) nothrow @nogc {
    ArchBFD* archbfd_open(const(char)* path, DsymDiags* diags);
    ArchBFD* archbfd_scan(const(char)* dir, DsymDiags* diags);
    void archbfd_close(ArchBFD* db);
    size_t archbfd_count(const(ArchBFD)* db);
    const(ArchBFDEntry)* archbfd_entry(const(ArchBFD)* db, size_t index);
    const(ArchBFDEntry)* archbfd_find(const(ArchBFD)* db, const(char)* name);
    const(ArchBFDSpec)* archbfd_load(ArchBFD* db, const(ArchBFDEntry)* entry,
                                      uint flags, DsymDiags* diags);
    const(ArchBFDSpec)* archbfd_load_name(ArchBFD* db, const(char)* name,
                                           uint flags, DsymDiags* diags);
}

// A document loaded on its own.
extern(C) nothrow @nogc {
    ArchBFDSpec* archbfd_spec_load_file(const(char)* path, DsymFormat format,
                                         uint flags, DsymDiags* diags);
    ArchBFDSpec* archbfd_spec_load_buffer(const(char)* data, size_t length,
                                           DsymFormat format, const(char)* virtual_path,
                                           uint flags, DsymDiags* diags);
    void archbfd_spec_free(ArchBFDSpec* spec);
}

// Lookups inside a document.
extern(C) nothrow @nogc {
    int archbfd_find_reloc(const(ArchBFDSpec)* spec, const(char)* name);
    int archbfd_find_reloc_number(const(ArchBFDSpec)* spec, long number);
    const(ArchBFDRecord)* archbfd_find_record(const(ArchBFDSpec)* spec, const(char)* name);
    const(ArchBFDRecord)* archbfd_resolve_record(const(ArchBFDSpec)* spec, const(char)* name);
}

// The spellings of the enumerations.
extern(C) nothrow @nogc {
    const(char)* archbfd_format_name(ArchBFDFormat format);
    const(char)* archbfd_byte_order_name(ArchBFDByteOrder order);
    const(char)* archbfd_overflow_name(ArchBFDOverflow overflow);
}

// Object files.
extern(C) nothrow @nogc {
    ArchBFDObject* archbfd_object_open(const(char)* path, const(ArchBFDSpec)* spec,
                                        DsymDiags* diags);
    ArchBFDObject* archbfd_object_open_memory(const(void)* data, size_t length,
                                               const(ArchBFDSpec)* spec, DsymDiags* diags);
    void archbfd_object_close(ArchBFDObject* object);
    size_t archbfd_object_section_count(const(ArchBFDObject)* object);
    size_t archbfd_object_symbol_count(const(ArchBFDObject)* object);
    size_t archbfd_object_reloc_count(const(ArchBFDObject)* object);
    const(ArchBFDObjectSection)* archbfd_object_section(const(ArchBFDObject)* object,
                                                         size_t index);
    const(ArchBFDObjectSymbol)* archbfd_object_symbol(const(ArchBFDObject)* object,
                                                       size_t index);
    const(ArchBFDObjectReloc)* archbfd_object_reloc(const(ArchBFDObject)* object,
                                                     size_t index);
    const(char)* archbfd_object_kind(const(ArchBFDObject)* object);
    long archbfd_object_machine(const(ArchBFDObject)* object);
    long archbfd_object_type(const(ArchBFDObject)* object);
}
