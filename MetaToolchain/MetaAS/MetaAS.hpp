#ifndef METATOOLCHAIN_METAAS_HPP
#define METATOOLCHAIN_METAAS_HPP
#include "MetaAS.h"
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace metaas {
inline MetaASOptions options() { MetaASOptions o; metaas_options_init(&o); return o; }
struct Error : std::runtime_error {
    MetaASError detail;
    explicit Error(const MetaASError &e) : std::runtime_error(e.message), detail(e) {}
};
class Result {
    std::unique_ptr<MetaASResult, decltype(&metaas_result_free)> result_;
public:
    explicit Result(MetaASResult *r) : result_(r, metaas_result_free) {}
    std::span<const uint8_t> bytes() const { return {metaas_result_data(result_.get()), metaas_result_size(result_.get())}; }
    std::string text() const { auto b = bytes(); return b.empty() ? std::string{} : std::string(reinterpret_cast<const char *>(b.data()), b.size()); }
    std::string diagnostics() const { auto p = metaas_result_diagnostics(result_.get()); return p ? p : ""; }
    std::vector<std::string> dependencies() const {
        std::vector<std::string> out;
        for (size_t i = 0; i < metaas_result_dependency_count(result_.get()); ++i) out.emplace_back(metaas_result_dependency(result_.get(), i));
        return out;
    }
};
/* Move-only owner. Callbacks are owned by the wrapper and follow it on move.
 * Methods throw Error on C API failure, or std::bad_alloc. Views borrow Results.
 * Same exclusion/reentry rules as the C API; no executable is spawned for builtin. */
class Assembler {
public:
    using Directive = std::function<std::string(std::string_view, const ArchASInvocation &)>;
private:
    std::vector<std::unique_ptr<Directive>> callbacks_;
    std::unique_ptr<MetaAS, decltype(&metaas_free)> assembler_{nullptr, metaas_free};
    void check(int32_t rc) const {
        if (rc == METAAS_OK) return;
        if (!assembler_) throw std::logic_error("moved-from MetaAS assembler");
        throw Error(*metaas_error(assembler_.get()));
    }
    static int invoke(void *p, const ArchASInvocation *inv, const ArchASEmitter *out) noexcept {
        try {
            auto source = (*static_cast<Directive *>(p))({inv->arguments, inv->arguments_length}, *inv);
            return out->emit(out->context, source.data(), source.size());
        } catch (const std::exception &e) { out->error(out->context, e.what()); }
        catch (...) { out->error(out->context, "C++ directive threw an unknown exception"); }
        return -1;
    }
public:
    explicit Assembler(const MetaASOptions &o) {
        MetaASError error{}; assembler_.reset(metaas_create(&o, &error));
        if (!assembler_) throw Error(error);
    }
    Assembler(Assembler &&) noexcept = default;
    Assembler &operator=(Assembler &&) noexcept = default;
    Assembler &include(const std::string &p) { check(metaas_add_include(assembler_.get(), p.c_str())); return *this; }
    Assembler &lua(const std::string &p) { check(metaas_add_lua(assembler_.get(), p.c_str())); return *this; }
    Assembler &plugin(const std::string &p) { check(metaas_add_plugin(assembler_.get(), p.c_str())); return *this; }
    Assembler &define(const std::string &name, int64_t value) { check(metaas_define(assembler_.get(), name.c_str(), value)); return *this; }
    Assembler &directive(const std::string &name, Directive callback) {
        if (!callback) throw std::invalid_argument("empty directive callback");
        callbacks_.push_back(std::make_unique<Directive>(std::move(callback)));
        auto rc = metaas_register_directive(assembler_.get(), name.c_str(), invoke, callbacks_.back().get());
        if (rc != METAAS_OK) { callbacks_.pop_back(); check(rc); }
        return *this;
    }
    Result compile(std::string_view source, const std::string &file = "<memory>") {
        auto *r = metaas_compile(assembler_.get(), source.data(), source.size(), file.c_str());
        if (!r) check(METAAS_ERROR);
        return Result(r);
    }
};
}
#endif
