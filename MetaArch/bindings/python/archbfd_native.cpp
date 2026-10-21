// The ArchBFD part of the nanobind extension.
//
// The classes are registered in the same module as the architecture API, but
// under their own names: the ArchBFD model has its own diagnostics and its
// own error type, and Python sees them as ArchBFDDiagnostic/ArchBFDError.
#include "ArchBFD.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

namespace nb = nanobind;
namespace bfd = archbfd::api;
using namespace nb::literals;

void archbfd_register(nb::module_ &m) {
    nb::class_<bfd::Diagnostic>(m, "ArchBFDDiagnostic")
        .def_ro("severity", &bfd::Diagnostic::severity)
        .def_ro("line", &bfd::Diagnostic::line)
        .def_ro("column", &bfd::Diagnostic::column)
        .def_ro("file", &bfd::Diagnostic::file)
        .def_ro("path", &bfd::Diagnostic::path)
        .def_ro("message", &bfd::Diagnostic::message);
    nb::exception<bfd::Error> error(m, "ArchBFDError", PyExc_RuntimeError);
    nb::register_exception_translator([](const std::exception_ptr &p, void *payload) {
        try {
            std::rethrow_exception(p);
        } catch (const bfd::Error &e) {
            auto type = static_cast<PyObject *>(payload);
            nb::object instance = nb::borrow<nb::object>(type)(nb::str(e.what()));
            instance.attr("diagnostics") = nb::cast(e.diagnostics());
            PyErr_SetObject(type, instance.ptr());
        }
    }, error.ptr());

    nb::class_<bfd::Field>(m, "ArchBFDField")
        .def_ro("name", &bfd::Field::name)
        .def_ro("offset", &bfd::Field::offset)
        .def_ro("size", &bfd::Field::size)
        .def_ro("desc", &bfd::Field::desc);
    nb::class_<bfd::Encoding>(m, "ArchBFDEncoding")
        .def_ro("field", &bfd::Encoding::field)
        .def_ro("target", &bfd::Encoding::target)
        .def_ro("mask", &bfd::Encoding::mask)
        .def_ro("shift", &bfd::Encoding::shift);
    nb::class_<bfd::Record>(m, "ArchBFDRecord")
        .def_ro("name", &bfd::Record::name)
        .def_ro("desc", &bfd::Record::desc)
        .def_ro("size", &bfd::Record::size)
        .def_ro("fields", &bfd::Record::fields)
        .def_ro("encodings", &bfd::Record::encodings);
    nb::class_<bfd::Constant>(m, "ArchBFDConstant")
        .def_ro("name", &bfd::Constant::name)
        .def_ro("value", &bfd::Constant::value)
        .def_ro("desc", &bfd::Constant::desc);
    nb::class_<bfd::ConstantGroup>(m, "ArchBFDConstantGroup")
        .def_ro("name", &bfd::ConstantGroup::name)
        .def_ro("desc", &bfd::ConstantGroup::desc)
        .def_ro("items", &bfd::ConstantGroup::items);
    nb::class_<bfd::Relocation>(m, "ArchBFDRelocation")
        .def_ro("name", &bfd::Relocation::name)
        .def_ro("number", &bfd::Relocation::number)
        .def_ro("size", &bfd::Relocation::size)
        .def_ro("bitsize", &bfd::Relocation::bitsize)
        .def_ro("pcrel", &bfd::Relocation::pcrel)
        .def_ro("pcrel_offset", &bfd::Relocation::pcrel_offset)
        .def_ro("partial", &bfd::Relocation::partial)
        .def_ro("overflow", &bfd::Relocation::overflow)
        .def_ro("rightshift", &bfd::Relocation::rightshift)
        .def_ro("bitpos", &bfd::Relocation::bitpos)
        .def_ro("src_mask", &bfd::Relocation::src_mask)
        .def_ro("dst_mask", &bfd::Relocation::dst_mask)
        .def_ro("desc", &bfd::Relocation::desc);
    nb::class_<bfd::Section>(m, "ArchBFDSection")
        .def_ro("name", &bfd::Section::name)
        .def_ro("desc", &bfd::Section::desc);
    nb::class_<bfd::Source>(m, "ArchBFDSource")
        .def_ro("file", &bfd::Source::file)
        .def_ro("note", &bfd::Source::note);
    nb::class_<bfd::ObjectSection>(m, "ArchBFDObjectSection")
        .def_ro("name", &bfd::ObjectSection::name)
        .def_ro("type", &bfd::ObjectSection::type)
        .def_ro("flags", &bfd::ObjectSection::flags)
        .def_ro("offset", &bfd::ObjectSection::offset)
        .def_ro("size", &bfd::ObjectSection::size)
        .def_ro("addr", &bfd::ObjectSection::addr)
        .def_ro("align", &bfd::ObjectSection::align)
        .def_ro("link", &bfd::ObjectSection::link)
        .def_ro("info", &bfd::ObjectSection::info)
        .def_ro("entsize", &bfd::ObjectSection::entsize)
        .def_ro("rel_offset", &bfd::ObjectSection::rel_offset)
        .def_ro("rel_count", &bfd::ObjectSection::rel_count);
    nb::class_<bfd::ObjectSymbol>(m, "ArchBFDObjectSymbol")
        .def_ro("name", &bfd::ObjectSymbol::name)
        .def_ro("value", &bfd::ObjectSymbol::value)
        .def_ro("size", &bfd::ObjectSymbol::size)
        .def_ro("shndx", &bfd::ObjectSymbol::shndx)
        .def_ro("bind", &bfd::ObjectSymbol::bind)
        .def_ro("type", &bfd::ObjectSymbol::type)
        .def_ro("type_value", &bfd::ObjectSymbol::type_value)
        .def_ro("class_value", &bfd::ObjectSymbol::class_value);
    nb::class_<bfd::ObjectRelocation>(m, "ArchBFDObjectRelocation")
        .def_ro("type_name", &bfd::ObjectRelocation::type_name)
        .def_ro("symbol_name", &bfd::ObjectRelocation::symbol_name)
        .def_ro("offset", &bfd::ObjectRelocation::offset)
        .def_ro("type", &bfd::ObjectRelocation::type)
        .def_ro("symbol", &bfd::ObjectRelocation::symbol)
        .def_ro("addend", &bfd::ObjectRelocation::addend);

    nb::class_<bfd::SpecView>(m, "ArchBFDSpecView")
        .def_prop_ro("name", &bfd::SpecView::name)
        .def_prop_ro("description", &bfd::SpecView::description)
        .def_prop_ro("format", &bfd::SpecView::format)
        .def_prop_ro("byte_order", &bfd::SpecView::byte_order)
        .def_prop_ro("target_vector", &bfd::SpecView::target_vector)
        .def_prop_ro("arch", &bfd::SpecView::arch)
        .def_prop_ro("container", &bfd::SpecView::container)
        .def_prop_ro("magic", &bfd::SpecView::magic)
        .def_prop_ro("schema", &bfd::SpecView::schema)
        .def_prop_ro("record_count", &bfd::SpecView::record_count)
        .def_prop_ro("reloc_count", &bfd::SpecView::reloc_count)
        .def_prop_ro("constant_group_count", &bfd::SpecView::constant_group_count)
        .def_prop_ro("section_count", &bfd::SpecView::section_count)
        .def_prop_ro("source_count", &bfd::SpecView::source_count)
        .def("source", &bfd::SpecView::source, "index"_a)
        .def("record", &bfd::SpecView::record, "index"_a)
        .def("relocation", &bfd::SpecView::relocation, "index"_a)
        .def("constant_group", &bfd::SpecView::constant_group, "index"_a)
        .def("section", &bfd::SpecView::section, "index"_a)
        .def("find_reloc", &bfd::SpecView::find_reloc, "name"_a)
        .def("find_reloc_number", &bfd::SpecView::find_reloc_number, "number"_a)
        .def("find_record", [](const bfd::SpecView &s, const std::string &name) {
            bfd::Record record;
            if (!s.find_record(name, &record)) return nb::none();
            return nb::cast(record);
        }, "name"_a)
        .def("constant", [](const bfd::SpecView &s, const std::string &group,
                            const std::string &name) {
            int64_t value = 0;
            if (!s.constant(group, name, &value)) return nb::none();
            return nb::cast(value);
        }, "group"_a, "name"_a);

    nb::class_<bfd::Spec, bfd::SpecView>(m, "ArchBFDSpec")
        .def_static("load", [](const std::string &path, int32_t format, uint32_t flags) {
            return bfd::Spec::load(path, static_cast<DsymFormat>(format), flags);
        }, "path"_a, "format"_a = int32_t(DSYM_FORMAT_AUTO), "flags"_a = 0)
        .def_static("load_buffer",
                    [](nb::handle data, int32_t format, const std::string &path, uint32_t flags) {
                        if (nb::isinstance<nb::str>(data)) {
                            auto text = nb::cast<std::string>(data);
                            return bfd::Spec::load_buffer(text, static_cast<DsymFormat>(format),
                                                         path, flags);
                        }
                        if (nb::isinstance<nb::bytes>(data)) {
                            auto bytes = nb::borrow<nb::bytes>(data);
                            return bfd::Spec::load_buffer(
                                std::string_view(bytes.c_str(), bytes.size()),
                                static_cast<DsymFormat>(format), path, flags);
                        }
                        throw nb::type_error("data must be str or bytes");
                    },
                    "data"_a, "format"_a, "path"_a = "<memory>", "flags"_a = 0)
        .def("close", &bfd::Spec::close)
        .def("__enter__", [](bfd::Spec &s) -> bfd::Spec & {
            (void)s.record_count(); // Reject a closed context manager.
            return s;
        }, nb::rv_policy::reference_internal)
        .def("__exit__", [](bfd::Spec &s, nb::handle, nb::handle, nb::handle) { s.close(); },
             "exc_type"_a.none(), "exc"_a.none(), "traceback"_a.none())
        .def_prop_ro("diagnostics", [](const bfd::Spec &s) { return s.diagnostics(); })
        .def("write", [](const bfd::Spec &s, int32_t format, uint32_t flags) {
            return s.write(static_cast<DsymFormat>(format), flags);
        }, "format"_a, "flags"_a = 0);

    nb::class_<bfd::Catalogue>(m, "ArchBFDCatalogue")
        .def_static("open", &bfd::Catalogue::open, "path"_a)
        .def_static("scan", &bfd::Catalogue::scan, "dir"_a)
        .def("close", &bfd::Catalogue::close)
        .def("__enter__", [](bfd::Catalogue &c) -> bfd::Catalogue & {
            (void)c.count(); // Reject a closed context manager.
            return c;
        }, nb::rv_policy::reference_internal)
        .def("__exit__", [](bfd::Catalogue &c, nb::handle, nb::handle, nb::handle) { c.close(); },
             "exc_type"_a.none(), "exc"_a.none(), "traceback"_a.none())
        .def_prop_ro("count", &bfd::Catalogue::count)
        .def("entry_name", &bfd::Catalogue::entry_name, "index"_a)
        .def("entry_path", &bfd::Catalogue::entry_path, "index"_a)
        .def("entry_format", &bfd::Catalogue::entry_format, "index"_a)
        .def("entry_target_vector", &bfd::Catalogue::entry_target_vector, "index"_a)
        .def("load", &bfd::Catalogue::load, "name"_a, "flags"_a = 0);

    nb::class_<bfd::Object>(m, "ArchBFDObject")
        .def_static("open", &bfd::Object::open, "path"_a, "spec"_a)
        .def_static("open_buffer", &bfd::Object::open_buffer, "data"_a, "spec"_a)
        .def("close", &bfd::Object::close)
        .def("__enter__", [](bfd::Object &o) -> bfd::Object & {
            (void)o.section_count(); // Reject a closed context manager.
            return o;
        }, nb::rv_policy::reference_internal)
        .def("__exit__", [](bfd::Object &o, nb::handle, nb::handle, nb::handle) { o.close(); },
             "exc_type"_a.none(), "exc"_a.none(), "traceback"_a.none())
        .def_prop_ro("kind", &bfd::Object::kind)
        .def_prop_ro("machine", &bfd::Object::machine)
        .def_prop_ro("type", &bfd::Object::type)
        .def_prop_ro("section_count", &bfd::Object::section_count)
        .def_prop_ro("symbol_count", &bfd::Object::symbol_count)
        .def_prop_ro("relocation_count", &bfd::Object::relocation_count)
        .def("section", &bfd::Object::section, "index"_a)
        .def("symbol", &bfd::Object::symbol, "index"_a)
        .def("relocation", &bfd::Object::relocation, "index"_a);
}
