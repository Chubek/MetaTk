#ifndef METATOOLCHAIN_METALD_HPP
#define METATOOLCHAIN_METALD_HPP
#include "MetaLD.h"
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace metald {
inline MetaLDOptions options() { MetaLDOptions o; metald_options_init(&o); return o; }
struct Error : std::runtime_error {
    int32_t code;
    explicit Error(const MetaLDError &e) : std::runtime_error(e.message), code(e.code) {}
};
struct Symbol { std::string name; uint64_t address, size; };
/* Move-only C++20 owner. Mutators throw Error/std::bad_alloc. Image is borrowed
 * until destruction; symbols() copies names. Same lifecycle/thread rules as C. */
class Linker {
public:
    using Resolver = std::function<std::optional<uint64_t>(std::string_view)>;
private:
    std::unique_ptr<Resolver> resolver_;
    std::unique_ptr<MetaLD, decltype(&metald_free)> linker_{nullptr, metald_free};
    void check(int32_t rc) const {
        if (rc == METALD_OK) return;
        if (!linker_) throw std::logic_error("moved-from MetaLD linker");
        throw Error(*metald_error(linker_.get()));
    }
    static int invoke(void *p, const char *name, uint64_t *address, char *message, size_t capacity) noexcept {
        try {
            auto value = (*static_cast<Resolver *>(p))(name);
            if (!value) return 0;
            *address = *value; return 1;
        } catch (const std::exception &e) { std::snprintf(message, capacity, "%s", e.what()); }
        catch (...) { std::snprintf(message, capacity, "%s", "C++ resolver threw an unknown exception"); }
        return -1;
    }
public:
    explicit Linker(const MetaLDOptions &o = options()) {
        MetaLDError error{}; linker_.reset(metald_create(&o, &error));
        if (!linker_) throw Error(error);
    }
    Linker(Linker &&) noexcept = default;
    Linker &operator=(Linker &&) noexcept = default;
    Linker &add(const std::string &path) { check(metald_add_file(linker_.get(), path.c_str())); return *this; }
    Linker &add(std::string name, std::span<const uint8_t> data) { check(metald_add_memory(linker_.get(), name.c_str(), data.data(), data.size())); return *this; }
    Linker &define(const std::string &name, uint64_t address) { check(metald_define(linker_.get(), name.c_str(), address)); return *this; }
    Linker &resolver(Resolver fn) {
        auto next = fn ? std::make_unique<Resolver>(std::move(fn)) : nullptr;
        check(metald_set_resolver(linker_.get(), next ? invoke : nullptr, next.get()));
        resolver_ = std::move(next); return *this;
    }
    Linker &link() { check(metald_link(linker_.get())); return *this; }
    int32_t state() const { return metald_state(linker_.get()); }
    std::span<const uint8_t> image() const { return {metald_image(linker_.get()), metald_image_size(linker_.get())}; }
    uint64_t base() const { return metald_image_base(linker_.get()); }
    std::optional<uint64_t> symbol(const std::string &name) const {
        uint64_t value;
        auto rc = metald_find_symbol(linker_.get(), name.c_str(), &value);
        if (rc == METALD_NOT_FOUND) return {};
        if (rc != METALD_OK) throw std::logic_error("symbol lookup requires a linked context");
        return value;
    }
    std::vector<Symbol> symbols() const {
        if (state() != METALD_LINKED) throw std::logic_error("symbol lookup requires a linked context");
        std::vector<Symbol> result;
        for (size_t i = 0; i < metald_symbol_count(linker_.get()); ++i) {
            MetaLDSymbol s{};
            if (metald_symbol_at(linker_.get(), i, &s) != METALD_OK) throw std::logic_error("invalid symbol index");
            result.push_back({s.name, s.address, s.size});
        }
        return result;
    }
    void write_executable(const std::string &path, const std::string &entry = "_start") { check(metald_write_executable(linker_.get(), path.c_str(), entry.c_str())); }
    void write_map(const std::string &path) { check(metald_write_map(linker_.get(), path.c_str())); }
};
}
#endif
