#ifndef METATOOLCHAIN_METAAS_DSL_HPP
#define METATOOLCHAIN_METAAS_DSL_HPP

// Native C++20 program construction. Link MetaAS::dsl; DSLtk is header-only.
#include "MetaAS.hpp"
#include <DSLtk.hpp>
#include <charconv>
#include <concepts>
#include <initializer_list>
#include <limits>
#include <set>
#include <variant>

namespace metaas::dsl {
namespace detail {
constexpr bool identifier(std::string_view s) {
    auto first = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.';
    };
    if (s.empty() || !first(s.front())) return false;
    for (char c : s) if (!first(c) && !(c >= '0' && c <= '9') && c != '$') return false;
    return true;
}
inline std::string name(std::string_view s) {
    if (!identifier(s)) throw std::invalid_argument("invalid assembly identifier");
    return std::string(s);
}
// to_chars is independent of the process locale.
template<std::integral T> std::string number(T value) {
    char buffer[32];
    auto [end, error] = std::to_chars(buffer, buffer + sizeof buffer, value);
    if (error != std::errc{}) throw std::length_error("integer formatting failed");
    return {buffer, end};
}
} // namespace detail

/// Owned, validated physical register name; the target resolves it at compile time.
class Register {
    std::string name_;
public:
    explicit Register(std::string_view name) : name_(detail::name(name)) {}
    const std::string &name() const noexcept { return name_; }
};
/// Owned relocation symbol and signed addend; only encode() accepts these.
class Symbol {
    std::string name_;
    int64_t addend_;
public:
    explicit Symbol(std::string_view name, int64_t addend = 0)
        : name_(detail::name(name)), addend_(addend) {}
    const std::string &name() const noexcept { return name_; }
    int64_t addend() const noexcept { return addend_; }
};
/// Checked conversion avoids silent unsigned-to-signed narrowing at API boundaries.
class Immediate {
    int64_t value_;
public:
    template<std::integral T> requires (!std::same_as<T, bool>)
    explicit Immediate(T value) {
        if constexpr (std::numeric_limits<T>::digits > std::numeric_limits<int64_t>::digits) {
            if (value > static_cast<T>(INT64_MAX))
                throw std::out_of_range("immediate exceeds signed 64-bit range");
            if constexpr (std::is_signed_v<T>)
                if (value < static_cast<T>(INT64_MIN))
                    throw std::out_of_range("immediate exceeds signed 64-bit range");
        }
        value_ = static_cast<int64_t>(value);
    }
    int64_t value() const noexcept { return value_; }
};
inline Register reg(std::string_view name) { return Register(name); }
inline Symbol symbol(std::string_view name, int64_t addend = 0) { return Symbol(name, addend); }
template<std::integral T> requires (!std::same_as<T, bool>)
Immediate imm(T value) { return Immediate(value); }
using Operand = std::variant<Immediate, Register, Symbol>;
struct Binding { std::string name; Operand value; };
inline Binding arg(std::string_view name, Operand value) {
    return {detail::name(name), std::move(value)};
}
/// DSLtk string NTTPs catch malformed literal parameter names at compile time.
template<::dsl::FixedString Name> Binding arg(Operand value) {
    static_assert(detail::identifier(Name.view()), "invalid encoder parameter name");
    return arg(Name.view(), std::move(value));
}

struct Limits {
    size_t source_bytes = 16 * 1024 * 1024;
    size_t statements = 1000000;
};
enum class Width { byte = 1, word = 2, long_ = 4, quad = 8 };

/** Owns source and label names; copies are independent. No target is retained.
 * Mutators validate before append and leave the program unchanged on failure.
 * Builder errors throw invalid_argument, out_of_range or length_error; allocation
 * may throw bad_alloc. Const reads can run concurrently; mutations need exclusion.
 * One generated line is one statement. Source views borrow until mutation/destruction.
 */
class Program : public ::dsl::DSL<Program, ::dsl::Pipeline> {
    Limits limits_;
    std::string source_;
    size_t statements_ = 0;
    std::set<std::string> labels_;

    void room(size_t size) const {
        if (size > limits_.source_bytes - source_.size()) throw std::length_error("DSL source byte limit exceeded");
    }
    void extend(std::string &line, std::string_view text) const {
        room(line.size());
        if (text.size() > limits_.source_bytes - source_.size() - line.size())
            throw std::length_error("DSL source byte limit exceeded");
        line += text;
    }
    Program &append(std::string line) {
        if (statements_ >= limits_.statements) throw std::length_error("DSL statement limit exceeded");
        extend(line, "\n");
        source_ += line;
        ++statements_;
        return *this;
    }
    Program &encoded(std::string line, const std::vector<Binding> &bindings, bool form) {
        std::set<std::string> names;
        for (const auto &b : bindings) {
            detail::name(b.name); // Binding is intentionally an aggregate; revalidate it.
            if (!names.insert(b.name).second) throw std::invalid_argument("duplicate encoder parameter: " + b.name);
            extend(line, ", "); extend(line, b.name); extend(line, "=");
            std::visit([&](const auto &value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::same_as<T, Immediate>) extend(line, detail::number(value.value()));
                else if constexpr (std::same_as<T, Register>) {
                    detail::name(value.name());
                    extend(line, value.name());
                } else {
                    if (form) throw std::invalid_argument("symbolic inputs require encode, not form");
                    detail::name(value.name());
                    extend(line, "@"); extend(line, value.name());
                    if (value.addend() > 0) extend(line, "+");
                    if (value.addend()) extend(line, detail::number(value.addend()));
                }
            }, b.value);
        }
        return append(std::move(line));
    }
public:
    explicit Program(Limits limits = {}) : limits_(limits) {}
    // Reset moved-from bookkeeping so the object remains a usable empty program.
    Program(const Program &) = default;
    Program(Program &&other) noexcept : Program(other.limits_) { swap(other); }
    Program &operator=(Program other) noexcept { swap(other); return *this; }
    void swap(Program &other) noexcept {
        std::swap(limits_, other.limits_); source_.swap(other.source_);
        std::swap(statements_, other.statements_); labels_.swap(other.labels_);
    }
    std::string_view source() const noexcept { return source_; }
    size_t size() const noexcept { return statements_; }
    bool empty() const noexcept { return statements_ == 0; }
    void clear() noexcept { source_.clear(); statements_ = 0; labels_.clear(); }
    /// Low-level encoder constraints; bindings use exact database parameter names.
    Program &encode(std::string_view name, const std::vector<Binding> &bindings = {}) {
        return encoded(".encode " + detail::name(name), bindings, false);
    }
    /// Checks declared form operand ranges/classes through the existing MetaAS compiler.
    Program &form(size_t index, const std::vector<Binding> &bindings = {}) {
        if (!std::in_range<int64_t>(index)) throw std::out_of_range("form index exceeds signed 64-bit range");
        return encoded(".form " + detail::number(index), bindings, true);
    }
    Program &section(std::string_view name) {
        auto checked = detail::name(name);
        if (checked.front() != '.') throw std::invalid_argument("section name must start with '.'");
        return append(".section " + checked);
    }
    Program &global(std::string_view name) { return append(".globl " + detail::name(name)); }
    Program &label(std::string_view name) {
        auto checked = detail::name(name);
        auto [it, inserted] = labels_.insert(checked);
        if (!inserted) throw std::invalid_argument("duplicate label: " + checked);
        try { return append(checked + ":"); }
        catch (...) { labels_.erase(it); throw; }
    }
    Program &align(unsigned bytes) {
        if (!bytes || bytes > 65536 || (bytes & (bytes - 1)))
            throw std::invalid_argument("alignment must be a power of two <= 65536");
        return append(".balign " + detail::number(bytes));
    }
    /// Values accept signed minima through unsigned maxima for the chosen width.
    /// Quad data uses signed int64_t; use bytes() for arbitrary 64-bit bit patterns.
    Program &data(Width width, std::initializer_list<int64_t> values) {
        unsigned bits;
        std::string line;
        switch (width) {
        case Width::byte: bits = 8; line = ".byte "; break;
        case Width::word: bits = 16; line = ".2byte "; break;
        case Width::long_: bits = 32; line = ".4byte "; break;
        case Width::quad: bits = 64; line = ".8byte "; break;
        default: throw std::invalid_argument("invalid data width");
        }
        if (!values.size()) throw std::invalid_argument("empty data list");
        bool first = true;
        for (auto value : values) {
            if (bits < 64 && (value < -(int64_t(1) << (bits - 1)) ||
                (value >= 0 && uint64_t(value) >= (uint64_t(1) << bits))))
                throw std::out_of_range("data value does not fit width");
            if (!first) extend(line, ", ");
            first = false; extend(line, detail::number(value));
        }
        return append(std::move(line));
    }
    /// Literal bytes, including NUL and template delimiters; empty input is a no-op.
    Program &bytes(std::span<const uint8_t> values) {
        if (values.empty()) return *this;
        room(values.size());
        std::string line = ".byte ";
        bool first = true;
        for (auto value : values) {
            if (!first) extend(line, ", ");
            first = false; extend(line, detail::number(value));
        }
        return append(std::move(line));
    }
    Program &ascii(std::string_view text) {
        return bytes({reinterpret_cast<const uint8_t *>(text.data()), text.size()});
    }
    /// Synchronous compilation. Result owns output; assembler is borrowed for this call.
    /// Backend errors throw metaas::Error with file and generated line/column.
    Result compile(Assembler &assembler, const std::string &file = "<MetaAS-DSL>") const {
        return assembler.compile(source_, file);
    }
};

// Owning, reusable DSLtk stages. An lvalue input is copied; an rvalue is moved.
inline auto encode(std::string name, std::vector<Binding> bindings = {}) {
    return ::dsl::pipe([name = std::move(name), bindings = std::move(bindings)](Program p) {
        p.encode(name, bindings); return p;
    });
}
inline auto form(size_t index, std::vector<Binding> bindings = {}) {
    return ::dsl::pipe([index, bindings = std::move(bindings)](Program p) {
        p.form(index, bindings); return p;
    });
}
inline auto section(std::string name) {
    return ::dsl::pipe([name = std::move(name)](Program p) { p.section(name); return p; });
}
inline auto label(std::string name) {
    return ::dsl::pipe([name = std::move(name)](Program p) { p.label(name); return p; });
}
inline auto global(std::string name) {
    return ::dsl::pipe([name = std::move(name)](Program p) { p.global(name); return p; });
}
/// Terminal stage borrows assembler, which must outlive every invocation of the stage.
inline auto compile(Assembler &assembler, std::string file = "<MetaAS-DSL>") {
    return ::dsl::pipe([&assembler, file = std::move(file)](const Program &p) {
        return p.compile(assembler, file);
    });
}
} // namespace metaas::dsl
#endif
