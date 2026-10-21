// Python owns copies of every view/result. No borrowed C pointer escapes.
#include "MetaArchAPI.hpp"
#include "archbfd_native.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

namespace nb = nanobind;
namespace api = metaarch::api;
using namespace nb::literals;

static int64_t operand_value(nb::handle value) {
    if (!nb::isinstance<nb::int_>(value) || nb::isinstance<nb::bool_>(value))
        throw nb::type_error("operand values must be signed 64-bit integers");
    int overflow = 0;
    auto result = PyLong_AsLongLongAndOverflow(value.ptr(), &overflow);
    if (overflow) throw std::overflow_error("operand value is outside int64 range");
    if (PyErr_Occurred()) throw nb::python_error();
    return static_cast<int64_t>(result);
}

NB_MODULE(_native, m) {
    m.doc() = "MetaArch ABI v1 bindings, implemented with nanobind.";
    m.def("abi_version", &metaarch_abi_version);

    nb::class_<api::Diagnostic>(m, "Diagnostic")
        .def_ro("severity", &api::Diagnostic::severity)
        .def_ro("line", &api::Diagnostic::line)
        .def_ro("column", &api::Diagnostic::column)
        .def_ro("file", &api::Diagnostic::file)
        .def_ro("path", &api::Diagnostic::path)
        .def_ro("message", &api::Diagnostic::message);
    nb::exception<api::Error> error(m, "Error", PyExc_RuntimeError);
    nb::register_exception_translator([](const std::exception_ptr &p, void *payload) {
        try {
            std::rethrow_exception(p);
        } catch (const api::Error &e) {
            auto type = static_cast<PyObject *>(payload);
            nb::object instance = nb::borrow<nb::object>(type)(nb::str(e.what()));
            instance.attr("diagnostics") = nb::cast(e.diagnostics());
            PyErr_SetObject(type, instance.ptr());
        }
    }, error.ptr());

    nb::class_<api::Target>(m, "Target")
        .def_ro("arch", &api::Target::arch)
        .def_ro("subarch", &api::Target::subarch)
        .def_ro("object_format", &api::Target::object_format)
        .def_ro("abi", &api::Target::abi)
        .def_ro("address_width", &api::Target::address_width)
        .def_ro("byte_order", &api::Target::byte_order);
    nb::class_<api::Register>(m, "Register")
        .def_ro("name", &api::Register::name)
        .def_ro("reg_class", &api::Register::reg_class)
        .def_ro("width", &api::Register::width)
        .def_ro("encoding", &api::Register::encoding);
    nb::class_<api::Form>(m, "Form")
        .def_ro("mnemonic", &api::Form::mnemonic)
        .def_ro("encoder", &api::Form::encoder)
        .def_ro("operand_count", &api::Form::operand_count)
        .def_ro("visible_operand_count", &api::Form::visible_operand_count);
    nb::class_<api::Relocation>(m, "Relocation")
        .def_ro("name", &api::Relocation::name)
        .def_ro("offset", &api::Relocation::offset)
        .def_ro("size", &api::Relocation::size)
        .def_ro("bit_offset", &api::Relocation::bit_offset)
        .def_ro("bit_width", &api::Relocation::bit_width)
        .def_ro("byte_order", &api::Relocation::byte_order)
        .def_ro("symbol_parameter", &api::Relocation::symbol_parameter)
        .def_ro("symbol_value", &api::Relocation::symbol_value);
    nb::class_<api::Encoded>(m, "Encoded")
        .def_prop_ro("data", [](const api::Encoded &e) {
            return nb::bytes(e.bytes.empty() ? "" : reinterpret_cast<const char *>(e.bytes.data()),
                             e.bytes.size());
        })
        .def_prop_ro("relocations", [](const api::Encoded &e) { return e.relocations; });

    nb::class_<api::Spec>(m, "Spec")
        .def_static("load", &api::Spec::load, "path"_a, "format"_a = int32_t(MA_AUTO), "flags"_a = 0)
        .def_static("parse", [](nb::handle data, MaFormat format, const std::string &path, uint32_t flags) {
            if (nb::isinstance<nb::str>(data)) {
                auto text = nb::cast<std::string>(data);
                return api::Spec::parse(text, format, path, flags);
            }
            if (nb::isinstance<nb::bytes>(data)) {
                auto bytes = nb::borrow<nb::bytes>(data);
                return api::Spec::parse(std::string_view(bytes.c_str(), bytes.size()), format, path, flags);
            }
            throw nb::type_error("data must be str or bytes");
        }, "data"_a, "format"_a, "path"_a = "<memory>", "flags"_a = 0)
        .def("close", &api::Spec::close)
        .def("__enter__", [](api::Spec &s) -> api::Spec & {
            (void) s.target_count(); // Reject a closed context manager.
            return s;
        }, nb::rv_policy::reference_internal)
        .def("__exit__", [](api::Spec &s, nb::handle, nb::handle, nb::handle) { s.close(); },
             "exc_type"_a.none(), "exc"_a.none(), "traceback"_a.none())
        .def_prop_ro("diagnostics", [](const api::Spec &s) { return s.diagnostics(); })
        .def_prop_ro("target_count", &api::Spec::target_count)
        .def_prop_ro("register_count", &api::Spec::register_count)
        .def_prop_ro("encoder_count", &api::Spec::encoder_count)
        .def_prop_ro("form_count", &api::Spec::form_count)
        .def("target", &api::Spec::target, "index"_a)
        .def("register", nb::overload_cast<size_t>(&api::Spec::reg, nb::const_), "index"_a)
        .def("register", nb::overload_cast<const std::string &>(&api::Spec::reg, nb::const_), "name"_a)
        .def("form", &api::Spec::form, "index"_a)
        .def("encoder_name", &api::Spec::encoder_name, "index"_a)
        .def("find_encoder", &api::Spec::find_encoder, "name"_a)
        .def("find_register", [](const api::Spec &s, const std::string &name) -> int64_t {
            api::check_string(name);
            for (size_t i = 0; i < s.register_count(); ++i)
                if (s.reg(i).name == name) return static_cast<int64_t>(i);
            return -1;
        }, "name"_a)
        .def("load_backend", &api::Spec::load_backend)
        .def("write", &api::Spec::write, "format"_a, "flags"_a = 0)
        .def("encode", [](const api::Spec &s, const std::string &encoder, nb::dict operands, size_t target) {
            std::vector<api::Binding> bindings;
            for (auto item : operands) {
                if (!nb::isinstance<nb::str>(item.first))
                    throw nb::type_error("operand names must be strings");
                bindings.push_back({nb::cast<std::string>(item.first), operand_value(item.second)});
            }
            return s.encode(encoder, bindings, target);
        }, "encoder"_a, "operands"_a, "target"_a = 0)
        .def("encode_form", [](const api::Spec &s, size_t form, nb::sequence values, size_t target) {
            std::vector<int64_t> operands;
            for (auto value : values) operands.push_back(operand_value(value));
            return s.encode_form(form, operands, target);
        }, "form"_a, "values"_a, "target"_a = 0);

    // The object-format catalogue registers its own classes.
    archbfd_register(m);
}
