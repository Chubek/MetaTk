// Standalone C++17 RAII API over ABI v1; does not require DSLtk or MetaArch.hpp.
#ifndef METAARCH_API_HPP
#define METAARCH_API_HPP
#include "MetaArchABI.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace metaarch { namespace api {
inline std::string copy_string(const char *s) { return s ? s : ""; }
inline void check_string(const std::string &s) {
    if (s.find('\0') != std::string::npos) throw std::invalid_argument("embedded NUL in name/path");
}
struct Diagnostic {
    int32_t severity;
    uint32_t line, column;
    std::string file, path, message;
};
class Error : public std::runtime_error {
    std::vector<Diagnostic> diagnostics_;
public:
    explicit Error(std::vector<Diagnostic> d)
        : std::runtime_error(d.empty() ? "MetaArch operation failed" : d.back().message),
          diagnostics_(std::move(d)) {}
    const std::vector<Diagnostic> &diagnostics() const noexcept { return diagnostics_; }
};
class Diagnostics {
    std::unique_ptr<MaDiagnostics, decltype(&metaarch_diagnostics_free)> handle_;
public:
    Diagnostics() : handle_(metaarch_diagnostics_create(), metaarch_diagnostics_free) {
        if (!handle_) throw std::bad_alloc();
    }
    MaDiagnostics *get() const noexcept { return handle_.get(); }
    std::vector<Diagnostic> copy() const {
        std::vector<Diagnostic> result;
        for (size_t i = 0; i < metaarch_diagnostics_count(get()); ++i) {
            MaDiagnosticView v{};
            if (metaarch_diagnostics_get(get(), i, &v) != MA_OK) throw std::logic_error("diagnostic index");
            result.push_back({v.severity, v.line, v.column, copy_string(v.file),
                              copy_string(v.path), copy_string(v.message)});
        }
        return result;
    }
};
struct Target {
    std::string arch, subarch, object_format, abi;
    int64_t address_width;
    uint32_t byte_order;
};
struct Register { std::string name, reg_class; int64_t width, encoding; };
struct Form { std::string mnemonic, encoder; size_t operand_count, visible_operand_count; };
struct Binding { std::string name; int64_t value; };
struct Relocation {
    std::string name;
    uint32_t offset, size, bit_offset, bit_width, byte_order, symbol_parameter;
    int64_t symbol_value;
};
struct Encoded { std::vector<uint8_t> bytes; std::vector<Relocation> relocations; };

class Spec {
    using ResultPtr = std::unique_ptr<MaResult, decltype(&metaarch_result_free)>;
    std::unique_ptr<MaSpec, decltype(&metaarch_spec_free)> handle_;
    std::vector<Diagnostic> diagnostics_;
    Spec(MaSpec *s, const Diagnostics &d) : handle_(s, metaarch_spec_free), diagnostics_(d.copy()) {
        if (!handle_) throw Error(diagnostics_);
    }
    const MaSpec *require() const {
        if (!handle_) throw std::logic_error("MetaArch spec is closed or moved-from");
        return handle_.get();
    }
    static ResultPtr result(MaResult *r, const Diagnostics &d) {
        ResultPtr owned(r, metaarch_result_free);
        if (!owned) throw Error(d.copy());
        return owned;
    }
    static Encoded encoded(const ResultPtr &r) {
        Encoded out;
        const uint8_t *data = metaarch_result_data(r.get());
        size_t n = metaarch_result_size(r.get());
        if (n) out.bytes.assign(data, data + n);
        for (size_t i = 0; i < metaarch_result_relocation_count(r.get()); ++i) {
            MaRelocationView v{};
            if (metaarch_result_relocation(r.get(), i, &v) != MA_OK) throw std::logic_error("relocation index");
            out.relocations.push_back({copy_string(v.name), v.offset, v.size, v.bit_offset,
                                       v.bit_width, v.byte_order, v.symbol_parameter, v.symbol_value});
        }
        return out;
    }
public:
    Spec(Spec &&) noexcept = default;
    Spec &operator=(Spec &&) noexcept = default;
    Spec(const Spec &) = delete;
    Spec &operator=(const Spec &) = delete;
    static Spec load(const std::string &path, MaFormat format = MA_AUTO, uint32_t flags = 0) {
        check_string(path);
        Diagnostics d;
        return Spec(metaarch_spec_load(path.c_str(), format, flags, d.get()), d);
    }
    static Spec parse(std::string_view data, MaFormat format,
                      const std::string &path = "<memory>", uint32_t flags = 0) {
        check_string(path);
        Diagnostics d;
        return Spec(metaarch_spec_parse(data.data(), data.size(), format, path.c_str(), flags, d.get()), d);
    }
    void close() noexcept { handle_.reset(); }
    const std::vector<Diagnostic> &diagnostics() const noexcept { return diagnostics_; }
    size_t target_count() const { return metaarch_spec_target_count(require()); }
    size_t register_count() const { return metaarch_spec_register_count(require()); }
    size_t encoder_count() const { return metaarch_spec_encoder_count(require()); }
    size_t form_count() const { return metaarch_spec_form_count(require()); }
    Target target(size_t i) const {
        MaTargetView v{};
        if (metaarch_spec_target(require(), i, &v) != MA_OK) throw std::out_of_range("target index");
        return {copy_string(v.arch), copy_string(v.subarch), copy_string(v.object_format),
                copy_string(v.abi), v.address_width, v.byte_order};
    }
    Register reg(size_t i) const {
        MaRegisterView v{};
        if (metaarch_spec_register(require(), i, &v) != MA_OK) throw std::out_of_range("register index");
        return {copy_string(v.name), copy_string(v.reg_class), v.width, v.encoding};
    }
    Register reg(const std::string &name) const {
        check_string(name);
        int32_t i = metaarch_spec_find_register(require(), name.c_str());
        if (i < 0) throw std::out_of_range("register not found: " + name);
        return reg(static_cast<size_t>(i));
    }
    Form form(size_t i) const {
        MaFormView v{};
        if (metaarch_spec_form(require(), i, &v) != MA_OK) throw std::out_of_range("form index");
        return {copy_string(v.mnemonic), copy_string(v.encoder), v.operand_count, v.visible_operand_count};
    }
    std::string encoder_name(size_t i) const {
        auto name = metaarch_spec_encoder_name(require(), i);
        if (!name) throw std::out_of_range("encoder index");
        return name;
    }
    int32_t find_encoder(const std::string &name) const {
        check_string(name);
        return metaarch_spec_find_encoder(require(), name.c_str());
    }
    // Exclusive mutation: finish loading the backend before concurrent reads.
    void load_backend() {
        require();
        Diagnostics d;
        if (!metaarch_spec_load_backend(handle_.get(), d.get())) throw Error(d.copy());
    }
    std::string write(MaFormat format, uint32_t flags = 0) const {
        Diagnostics d;
        auto r = result(metaarch_spec_write(require(), format, flags, d.get()), d);
        return std::string(reinterpret_cast<const char *>(metaarch_result_data(r.get())),
                           metaarch_result_size(r.get()));
    }
    Encoded encode(const std::string &encoder, const std::vector<Binding> &bindings,
                   size_t target = 0) const {
        check_string(encoder);
        std::vector<const char *> names;
        std::vector<int64_t> values;
        for (const auto &b : bindings) { check_string(b.name); names.push_back(b.name.c_str()); values.push_back(b.value); }
        Diagnostics d;
        return encoded(result(metaarch_spec_encode(require(), encoder.c_str(), target,
                              names.data(), values.data(), names.size(), d.get()), d));
    }
    Encoded encode_form(size_t form_index, const std::vector<int64_t> &values, size_t target = 0) const {
        Diagnostics d;
        return encoded(result(metaarch_spec_encode_form(require(), form_index, target,
                              values.data(), values.size(), d.get()), d));
    }
};
}} // namespace metaarch::api
#endif
