// Standalone C++17 RAII API over the ArchBFD object-format catalogue.
// Does not require DSLtk, MetaArch.hpp or the optional C++20 layer.
//
//   #include "ArchBFD.hpp"
//
//   archbfd::api::Catalogue db = archbfd::api::Catalogue::open("ArchBFDCatalogue.yaml");
//   archbfd::api::SpecView spec = db.load("ELF64");
//
// Every string is copied into the caller, so it stays valid after the
// document or the catalogue is closed.  The catalogue owns the documents it
// loads: a SpecView is only valid while its catalogue is alive.  A document
// loaded on its own is owned by its Spec.
#ifndef METAARCH_ARCHBFD_HPP
#define METAARCH_ARCHBFD_HPP
#include "ArchBFD.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace archbfd {
namespace api {

inline std::string copy_string(const char *s) { return s ? s : ""; }

inline void check_string(const std::string &s) {
    if (s.find('\0') != std::string::npos)
        throw std::invalid_argument("embedded NUL in name/path");
}

/* --------------------------------------------------------------------- */
/* Diagnostics                                                            */
/* --------------------------------------------------------------------- */

struct Diagnostic {
    int32_t severity;
    uint32_t line, column;
    std::string file, path, message;
};

class Error : public std::runtime_error {
    std::vector<Diagnostic> diagnostics_;

public:
    explicit Error(std::vector<Diagnostic> d)
        : std::runtime_error(d.empty() ? "ArchBFD operation failed" : d.back().message),
          diagnostics_(std::move(d)) {}
    const std::vector<Diagnostic> &diagnostics() const noexcept { return diagnostics_; }
};

/* Collects the diagnostics of the C API.  The collector owns the arena the
   C API writes into; the copied values stay valid after it is destroyed. */
class Diagnostics {
    DsymDiags diags_{};

public:
    Diagnostics() { dsym_diags_init(&diags_); }
    ~Diagnostics() { dsym_diags_free(&diags_); }
    Diagnostics(const Diagnostics &) = delete;
    Diagnostics &operator=(const Diagnostics &) = delete;

    DsymDiags *get() noexcept { return &diags_; }
    const DsymDiags *get() const noexcept { return &diags_; }
    size_t count() const noexcept { return diags_.count; }
    size_t errors() const noexcept { return diags_.errors; }
    size_t warnings() const noexcept { return diags_.warnings; }
    bool has_errors() const noexcept { return diags_.errors > 0; }

    std::vector<Diagnostic> copy() const {
        std::vector<Diagnostic> result;
        result.reserve(diags_.count);
        for (size_t i = 0; i < diags_.count; ++i) {
            const DsymDiag &d = diags_.items[i];
            result.push_back({d.severity, d.line, d.col, copy_string(d.file),
                              copy_string(d.path), copy_string(d.msg)});
        }
        return result;
    }
    /* Throws Error with the collected diagnostics when the call failed. */
    void rethrow_if_failed() const {
        if (diags_.errors) throw Error(copy());
    }
};

/* --------------------------------------------------------------------- */
/* Values                                                                 */
/* --------------------------------------------------------------------- */

struct Field {
    std::string name;
    int64_t offset, size;
    std::string desc;
};

struct Encoding {
    std::string field, target;
    uint64_t mask;
    int64_t shift;
};

struct Record {
    std::string name, desc;
    int64_t size;
    std::vector<Field> fields;
    std::vector<Encoding> encodings;
};

struct Constant {
    std::string name;
    int64_t value;
    std::string desc;
};

struct ConstantGroup {
    std::string name, desc;
    std::vector<Constant> items;
};

struct Relocation {
    std::string name;
    int64_t number, size, bitsize;
    bool pcrel, pcrel_offset, partial;
    int32_t overflow;
    int64_t rightshift, bitpos;
    uint64_t src_mask, dst_mask;
    std::string desc;
};

struct Section {
    std::string name, desc;
};

struct Source {
    std::string file, note;
};

struct ObjectSection {
    std::string name;
    int64_t type, offset, size, addr, align, link, info, entsize;
    int64_t rel_offset, rel_count;
    uint64_t flags;
};

struct ObjectSymbol {
    std::string name, bind, type;
    int64_t value, size, shndx, type_value, class_value;
};

struct ObjectRelocation {
    std::string type_name, symbol_name;
    int64_t offset, type, symbol, addend;
};

/* --------------------------------------------------------------------- */
/* A loaded document                                                      */
/* --------------------------------------------------------------------- */

/* A read-only view of a document.  It does not own it: the owner (a Spec or
   a Catalogue) must outlive the view. */
class SpecView {
protected:
    const ArchBFDSpec *spec_ = nullptr;

    const ArchBFDSpec *require() const {
        if (!spec_) throw std::logic_error("ArchBFD document is closed or moved-from");
        return spec_;
    }

public:
    SpecView() = default;
    explicit SpecView(const ArchBFDSpec *s) : spec_(s) {}

    const ArchBFDSpec *get() const noexcept { return spec_; }
    explicit operator bool() const noexcept { return spec_ != nullptr; }

    std::string name() const { return copy_string(require()->name); }
    std::string description() const { return copy_string(require()->description); }
    std::string target_vector() const { return copy_string(require()->target_vector); }
    std::string arch() const { return copy_string(require()->arch); }
    std::string container() const { return copy_string(require()->container); }
    std::string magic() const { return copy_string(require()->magic); }
    std::string format() const {
        return require()->has_format ? archbfd_format_name(require()->format) : "";
    }
    std::string byte_order() const {
        return require()->has_byte_order ? archbfd_byte_order_name(require()->byte_order) : "";
    }
    int64_t schema() const { return require()->schema; }

    size_t record_count() const { return require()->record_count; }
    size_t reloc_count() const { return require()->reloc_count; }
    size_t constant_group_count() const { return require()->constant_group_count; }
    size_t section_count() const { return require()->section_count; }
    size_t source_count() const { return require()->source_count; }
    size_t flag_count() const { return require()->flag_count; }

    std::string flag(size_t i) const {
        if (i >= require()->flag_count) throw std::out_of_range("flag index");
        return copy_string(require()->flags[i]);
    }

    Source source(size_t i) const {
        if (i >= require()->source_count) throw std::out_of_range("source index");
        const ArchBFDSource &s = require()->sources[i];
        return {copy_string(s.file), copy_string(s.note)};
    }

    Record record(size_t i) const {
        if (i >= require()->record_count) throw std::out_of_range("record index");
        const ArchBFDRecord &r = require()->records[i];
        Record out;
        out.name = copy_string(r.name);
        out.desc = copy_string(r.desc);
        out.size = r.size;
        for (size_t k = 0; k < r.field_count; ++k) {
            const ArchBFDField &f = r.fields[k];
            out.fields.push_back({copy_string(f.name), f.offset, f.size, copy_string(f.desc)});
        }
        for (size_t k = 0; k < r.encoding_count; ++k) {
            const ArchBFDEncoding &e = r.encodings[k];
            out.encodings.push_back({copy_string(e.field), copy_string(e.target), e.mask, e.shift});
        }
        return out;
    }

    std::vector<Record> records() const {
        std::vector<Record> out;
        out.reserve(record_count());
        for (size_t i = 0; i < record_count(); ++i) out.push_back(record(i));
        return out;
    }

    Relocation relocation(size_t i) const {
        if (i >= require()->reloc_count) throw std::out_of_range("relocation index");
        const ArchBFDReloc &r = require()->relocations[i];
        Relocation out;
        out.name = copy_string(r.name);
        out.number = r.number;
        out.size = r.size;
        out.bitsize = r.bitsize;
        out.pcrel = r.pcrel;
        out.pcrel_offset = r.pcrel_offset;
        out.partial = r.partial;
        out.overflow = static_cast<int32_t>(r.overflow);
        out.rightshift = r.rightshift;
        out.bitpos = r.bitpos;
        out.src_mask = r.src_mask;
        out.dst_mask = r.dst_mask;
        out.desc = copy_string(r.desc);
        return out;
    }

    std::vector<Relocation> relocations() const {
        std::vector<Relocation> out;
        out.reserve(reloc_count());
        for (size_t i = 0; i < reloc_count(); ++i) out.push_back(relocation(i));
        return out;
    }

    ConstantGroup constant_group(size_t i) const {
        if (i >= require()->constant_group_count) throw std::out_of_range("constant group index");
        const ArchBFDConstantGroup &g = require()->constants[i];
        ConstantGroup out;
        out.name = copy_string(g.name);
        out.desc = copy_string(g.desc);
        for (size_t k = 0; k < g.count; ++k)
            out.items.push_back({copy_string(g.items[k].name), g.items[k].value,
                                 copy_string(g.items[k].desc)});
        return out;
    }

    std::vector<ConstantGroup> constant_groups() const {
        std::vector<ConstantGroup> out;
        out.reserve(constant_group_count());
        for (size_t i = 0; i < constant_group_count(); ++i) out.push_back(constant_group(i));
        return out;
    }

    Section section(size_t i) const {
        if (i >= require()->section_count) throw std::out_of_range("section index");
        const ArchBFDSection &s = require()->sections[i];
        return {copy_string(s.name), copy_string(s.desc)};
    }

    std::vector<Section> sections() const {
        std::vector<Section> out;
        out.reserve(section_count());
        for (size_t i = 0; i < section_count(); ++i) out.push_back(section(i));
        return out;
    }

    /* Lookups.  They return -1 / false when the name is unknown. */
    int32_t find_reloc(const std::string &name) const {
        check_string(name);
        return archbfd_find_reloc(require(), name.c_str());
    }
    int32_t find_reloc_number(int64_t number) const {
        return archbfd_find_reloc_number(require(), number);
    }
    bool find_record(const std::string &name, Record *out) const {
        check_string(name);
        const ArchBFDRecord *r = archbfd_find_record(require(), name.c_str());
        if (!r) return false;
        if (out) {
            Record copy;
            copy.name = copy_string(r->name);
            copy.desc = copy_string(r->desc);
            copy.size = r->size;
            for (size_t k = 0; k < r->field_count; ++k)
                copy.fields.push_back({copy_string(r->fields[k].name), r->fields[k].offset,
                                       r->fields[k].size, copy_string(r->fields[k].desc)});
            for (size_t k = 0; k < r->encoding_count; ++k)
                copy.encodings.push_back({copy_string(r->encodings[k].field),
                                          copy_string(r->encodings[k].target),
                                          r->encodings[k].mask, r->encodings[k].shift});
            *out = std::move(copy);
        }
        return true;
    }
    bool constant(const std::string &group, const std::string &name, int64_t *value) const {
        check_string(group);
        check_string(name);
        return archbfd_constant(require(), group.c_str(), name.c_str(), value);
    }
};

/* An owned document. */
class Spec : public SpecView {
    std::unique_ptr<ArchBFDSpec, decltype(&archbfd_spec_free)> handle_;
    std::vector<Diagnostic> diagnostics_;

public:
    Spec(Spec &&) noexcept = default;
    Spec &operator=(Spec &&) noexcept = default;
    Spec(const Spec &) = delete;
    Spec &operator=(const Spec &) = delete;

    static Spec load(const std::string &path, DsymFormat format = DSYM_FORMAT_AUTO,
                     uint32_t flags = 0) {
        check_string(path);
        Diagnostics d;
        ArchBFDSpec *s = archbfd_spec_load_file(path.c_str(), format, flags, d.get());
        Spec out(s, d);
        if (!out.handle_) throw Error(d.copy());
        return out;
    }

    static Spec load_buffer(std::string_view data, DsymFormat format,
                            const std::string &path = "<memory>", uint32_t flags = 0) {
        check_string(path);
        Diagnostics d;
        ArchBFDSpec *s = archbfd_spec_load_buffer(data.data(), data.size(), format, path.c_str(),
                                                   flags, d.get());
        Spec out(s, d);
        if (!out.handle_) throw Error(d.copy());
        return out;
    }

    Spec(ArchBFDSpec *s, const Diagnostics &d)
        : SpecView(s), handle_(s, archbfd_spec_free), diagnostics_(d.copy()) {}

    void close() noexcept {
        handle_.reset();
        spec_ = nullptr;
    }
    const std::vector<Diagnostic> &diagnostics() const noexcept { return diagnostics_; }
    /* Serialise the document in any supported format. */
    std::string write(DsymFormat format, uint32_t flags = 0) const {
        Diagnostics d;
        DsymBuf buf;
        dsym_buf_init(&buf);
        bool ok = archbfd_spec_write(require(), format, flags, &buf, d.get());
        std::string result = ok ? std::string(buf.data ? buf.data : "", buf.len) : std::string();
        dsym_buf_free(&buf);
        if (!ok) throw Error(d.copy());
        return result;
    }
};

/* --------------------------------------------------------------------- */
/* The catalogue                                                          */
/* --------------------------------------------------------------------- */

/* Owns the documents it loads; the views it hands out stay valid until the
   catalogue is closed. */
class Catalogue {
    std::unique_ptr<ArchBFD, decltype(&archbfd_close)> handle_;

public:
    Catalogue(Catalogue &&) noexcept = default;
    Catalogue &operator=(Catalogue &&) noexcept = default;
    Catalogue(const Catalogue &) = delete;
    Catalogue &operator=(const Catalogue &) = delete;

    static Catalogue open(const std::string &path) {
        check_string(path);
        Diagnostics d;
        ArchBFD *db = archbfd_open(path.c_str(), d.get());
        Catalogue out(db);
        if (!out.handle_) throw Error(d.copy());
        return out;
    }

    static Catalogue scan(const std::string &dir) {
        check_string(dir);
        Diagnostics d;
        ArchBFD *db = archbfd_scan(dir.c_str(), d.get());
        Catalogue out(db);
        if (!out.handle_) throw Error(d.copy());
        return out;
    }

    Catalogue(ArchBFD *db) : handle_(db, archbfd_close) {}

    size_t count() const { return archbfd_count(handle_.get()); }
    void close() noexcept {
        handle_.reset();
    }

    std::string entry_name(size_t i) const {
        const ArchBFDEntry *e = archbfd_entry(handle_.get(), i);
        if (!e) throw std::out_of_range("catalogue entry index");
        return copy_string(e->name);
    }
    std::string entry_path(size_t i) const {
        const ArchBFDEntry *e = archbfd_entry(handle_.get(), i);
        if (!e) throw std::out_of_range("catalogue entry index");
        return copy_string(e->path);
    }
    std::string entry_format(size_t i) const {
        const ArchBFDEntry *e = archbfd_entry(handle_.get(), i);
        if (!e) throw std::out_of_range("catalogue entry index");
        return copy_string(e->format);
    }
    std::string entry_target_vector(size_t i) const {
        const ArchBFDEntry *e = archbfd_entry(handle_.get(), i);
        if (!e) throw std::out_of_range("catalogue entry index");
        return copy_string(e->target_vector);
    }

    /* Load a document by catalogue name, target vector or architecture. */
    SpecView load(const std::string &name, uint32_t flags = 0) {
        check_string(name);
        Diagnostics d;
        const ArchBFDSpec *s = archbfd_load_name(handle_.get(), name.c_str(), flags, d.get());
        if (!s) throw Error(d.copy());
        return SpecView(s);
    }
};

/* --------------------------------------------------------------------- */
/* Object files                                                           */
/* --------------------------------------------------------------------- */

/* Reads an object file with a description.  The description must outlive
   the object. */
class Object {
    std::unique_ptr<ArchBFDObject, decltype(&archbfd_object_close)> handle_;

    const ArchBFDObject *require() const {
        if (!handle_) throw std::logic_error("ArchBFD object is closed or moved-from");
        return handle_.get();
    }

public:
    Object(Object &&) noexcept = default;
    Object &operator=(Object &&) noexcept = default;
    Object(const Object &) = delete;
    Object &operator=(const Object &) = delete;

    static Object open(const std::string &path, const SpecView &spec) {
        check_string(path);
        Diagnostics d;
        ArchBFDObject *o = archbfd_object_open(path.c_str(), spec.get(), d.get());
        Object out(o);
        if (!out.handle_) throw Error(d.copy());
        return out;
    }

    static Object open_buffer(std::string_view data, const SpecView &spec) {
        Diagnostics d;
        ArchBFDObject *o = archbfd_object_open_memory(data.data(), data.size(), spec.get(), d.get());
        Object out(o);
        if (!out.handle_) throw Error(d.copy());
        return out;
    }

    Object(ArchBFDObject *o) : handle_(o, archbfd_object_close) {}

    void close() noexcept { handle_.reset(); }
    std::string kind() const { return copy_string(archbfd_object_kind(require())); }
    int64_t machine() const { return archbfd_object_machine(require()); }
    int64_t type() const { return archbfd_object_type(require()); }

    size_t section_count() const { return archbfd_object_section_count(require()); }
    size_t symbol_count() const { return archbfd_object_symbol_count(require()); }
    size_t relocation_count() const { return archbfd_object_reloc_count(require()); }

    ObjectSection section(size_t i) const {
        const ArchBFDObjectSection *s = archbfd_object_section(require(), i);
        if (!s) throw std::out_of_range("object section index");
        ObjectSection out;
        out.name = copy_string(s->name);
        out.type = s->type;
        out.flags = s->flags;
        out.offset = s->offset;
        out.size = s->size;
        out.addr = s->addr;
        out.align = s->align;
        out.link = s->link;
        out.info = s->info;
        out.entsize = s->entsize;
        out.rel_offset = s->rel_offset;
        out.rel_count = s->rel_count;
        return out;
    }

    ObjectSymbol symbol(size_t i) const {
        const ArchBFDObjectSymbol *s = archbfd_object_symbol(require(), i);
        if (!s) throw std::out_of_range("object symbol index");
        ObjectSymbol out;
        out.name = copy_string(s->name);
        out.value = s->value;
        out.size = s->size;
        out.shndx = s->shndx;
        out.bind = copy_string(s->bind);
        out.type = copy_string(s->type);
        out.type_value = s->type_value;
        out.class_value = s->class_value;
        return out;
    }

    ObjectRelocation relocation(size_t i) const {
        const ArchBFDObjectReloc *r = archbfd_object_reloc(require(), i);
        if (!r) throw std::out_of_range("object relocation index");
        ObjectRelocation out;
        out.type_name = copy_string(r->type_name);
        out.symbol_name = copy_string(r->symbol_name);
        out.offset = r->offset;
        out.type = r->type;
        out.symbol = r->symbol;
        out.addend = r->addend;
        return out;
    }

    std::vector<ObjectSection> sections() const {
        std::vector<ObjectSection> out;
        out.reserve(section_count());
        for (size_t i = 0; i < section_count(); ++i) out.push_back(section(i));
        return out;
    }
    std::vector<ObjectSymbol> symbols() const {
        std::vector<ObjectSymbol> out;
        out.reserve(symbol_count());
        for (size_t i = 0; i < symbol_count(); ++i) out.push_back(symbol(i));
        return out;
    }
    std::vector<ObjectRelocation> relocations() const {
        std::vector<ObjectRelocation> out;
        out.reserve(relocation_count());
        for (size_t i = 0; i < relocation_count(); ++i) out.push_back(relocation(i));
        return out;
    }
};

} // namespace api
} // namespace archbfd

#endif // METAARCH_ARCHBFD_HPP
