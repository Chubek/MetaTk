// MetaNum - shared catalog vocabulary.
//
// Every per-category catalog header (isl_*.hpp, gsl_*.hpp, ppl_*.hpp) includes
// this file instead of declaring its own types. Declaring `operation` once here
// is what lets any combination of catalog headers be included in a single
// translation unit.
//
// The catalogs *describe* the native ISL / GSL / PPL APIs listed in
// MANIFEST.yaml. They do not reimplement or link against those libraries.
#ifndef METANUM_CATALOG_CORE_HPP
#define METANUM_CATALOG_CORE_HPP

#if defined(_MSVC_LANG)
#  define METANUM_CPLUSPLUS _MSVC_LANG
#else
#  define METANUM_CPLUSPLUS __cplusplus
#endif
#if METANUM_CPLUSPLUS < 201703L
#  error "MetaNum requires C++17 or later."
#endif
#undef METANUM_CPLUSPLUS

#include <array>
#include <cstddef>
#include <string_view>

namespace metanum::catalog {

/// Upstream library that provides an operation.
enum class library : unsigned char { isl, gsl, ppl };

/// Short identifier as used in MANIFEST.yaml ("isl", "gsl", "ppl").
[[nodiscard]] constexpr std::string_view to_string(library lib) noexcept {
    switch (lib) {
        case library::isl: return "isl";
        case library::gsl: return "gsl";
        case library::ppl: return "ppl";
    }
    return {};
}

/// Human-readable library name as given in MANIFEST.yaml.
[[nodiscard]] constexpr std::string_view full_name(library lib) noexcept {
    switch (lib) {
        case library::isl: return "Integer Set Library";
        case library::gsl: return "GNU Scientific Library";
        case library::ppl: return "Parma Polyhedra Library";
    }
    return {};
}

/// Homepage URL as given in MANIFEST.yaml.
[[nodiscard]] constexpr std::string_view homepage(library lib) noexcept {
    switch (lib) {
        case library::isl: return "https://libisl.sourceforge.io/";
        case library::gsl: return "https://www.gnu.org/software/gsl/";
        case library::ppl: return "https://www.bugseng.com/ppl";
    }
    return {};
}

/// One manifest operation. `symbols` preserves the manifest's native symbols;
/// repeated symbols may be deduplicated by the generator. Views must outlive use;
/// use symbol_count()/symbol_at()/references_symbol() to work with the
/// individual native symbols it lists.
struct operation {
    std::string_view name;
    std::string_view symbols;
    std::string_view description;
};

/// Non-owning view of one manifest category (e.g. "gsl.fft").
struct category {
    std::string_view id{};
    library lib{};
    std::string_view description{};
    const operation* ops = nullptr;
    std::size_t count = 0;

    [[nodiscard]] constexpr const operation* begin() const noexcept { return ops; }
    [[nodiscard]] constexpr const operation* end() const noexcept { return count == 0 ? ops : ops + count; }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return count; }
    [[nodiscard]] constexpr bool empty() const noexcept { return count == 0; }
};

namespace detail {

constexpr bool is_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

constexpr std::string_view trim(std::string_view s) noexcept {
    while (!s.empty() && is_space(s.front())) s.remove_prefix(1);
    while (!s.empty() && is_space(s.back())) s.remove_suffix(1);
    return s;
}

/// Non-empty, no surrounding whitespace, printable ASCII only.
constexpr bool is_well_formed(std::string_view s) noexcept {
    if (s.empty() || is_space(s.front()) || is_space(s.back())) return false;
    for (const char c : s) {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x20 || u > 0x7e) return false;
    }
    return true;
}

/// Parentheses in a symbol list must balance.
constexpr bool parens_balanced(std::string_view s) noexcept {
    std::size_t depth = 0;
    for (const char c : s) {
        if (c == '(') ++depth;
        else if (c == ')') {
            if (depth == 0) return false;
            --depth;
        }
    }
    return depth == 0;
}

/// Symbol lists separate native symbols with ',' or '/'. Separators inside
/// parentheses (argument lists such as "maximize(expr, num, den, closed)")
/// do not split. Calls f(token) for each trimmed token; stops early if f
/// returns false. Returns the number of tokens visited.
template <class F>
constexpr std::size_t for_each_symbol(std::string_view list, F&& f) {
    if (trim(list).empty()) return 0;
    std::size_t visited = 0;
    std::size_t depth = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= list.size(); ++i) {
        const bool at_end = (i == list.size());
        const char c = at_end ? '\0' : list[i];
        if (c == '(') { ++depth; continue; }
        if (c == ')') { if (depth > 0) --depth; continue; }
        if (at_end || (depth == 0 && (c == ',' || c == '/'))) {
            ++visited;
            if (!f(trim(list.substr(start, i - start)))) return visited;
            if (at_end) return visited;
            start = i + 1;
        }
    }
    return visited;
}

}  // namespace detail

/// Number of individual native symbols an operation lists.
[[nodiscard]] constexpr std::size_t symbol_count(const operation& op) noexcept {
    return detail::for_each_symbol(op.symbols, [](std::string_view) { return true; });
}

/// i-th native symbol (0-based); empty view if out of range.
[[nodiscard]] constexpr std::string_view symbol_at(const operation& op, std::size_t i) noexcept {
    std::string_view out{};
    std::size_t k = 0;
    detail::for_each_symbol(op.symbols, [&](std::string_view s) {
        if (k++ == i) { out = s; return false; }
        return true;
    });
    return out;
}

/// True if `symbol` is one of the operation's listed native symbols (exact match).
[[nodiscard]] constexpr bool references_symbol(const operation& op, std::string_view symbol) noexcept {
    if (symbol.empty()) return false;
    bool hit = false;
    detail::for_each_symbol(op.symbols, [&](std::string_view s) {
        hit = (s == symbol);
        return !hit;
    });
    return hit;
}

/// Look up an operation by manifest name; nullptr if absent.
template <std::size_t N>
[[nodiscard]] constexpr const operation* find(const std::array<operation, N>& ops,
                                              std::string_view name) noexcept {
    for (const operation& op : ops)
        if (op.name == name) return &op;
    return nullptr;
}

/// Look up an operation by manifest name within a category; nullptr if absent.
[[nodiscard]] constexpr const operation* find(const category& cat, std::string_view name) noexcept {
    for (const operation& op : cat)
        if (op.name == name) return &op;
    return nullptr;
}

namespace detail {

/// Manifest identifiers use lowercase ASCII letters, digits and underscores.
constexpr bool identifier_well_formed(std::string_view s) noexcept {
    if (s.empty() || s.front() < 'a' || s.front() > 'z') return false;
    for (char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    return true;
}

constexpr bool category_metadata_well_formed(const category& cat) noexcept {
    const auto prefix = to_string(cat.lib);
    return !prefix.empty() && cat.id.size() > prefix.size() + 1
        && cat.id.substr(0, prefix.size()) == prefix && cat.id[prefix.size()] == '.'
        && identifier_well_formed(cat.id.substr(prefix.size() + 1))
        && is_well_formed(cat.description);
}

/// Non-owning categories require live storage for count operations.
constexpr bool category_well_formed(const category& cat) noexcept {
    return category_metadata_well_formed(cat) && (cat.count == 0 || cat.ops != nullptr);
}

/// Compile-time checks applied to every generated catalog via static_assert.
template <std::size_t N>
constexpr bool fields_well_formed(const std::array<operation, N>& ops) noexcept {
    for (const operation& op : ops)
        if (!is_well_formed(op.name) || !is_well_formed(op.symbols) || !is_well_formed(op.description))
            return false;
    return true;
}

template <std::size_t N>
constexpr bool names_unique(const std::array<operation, N>& ops) noexcept {
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = i + 1; j < N; ++j)
            if (ops[i].name == ops[j].name) return false;
    return true;
}

template <std::size_t N>
constexpr bool symbols_well_formed(const std::array<operation, N>& ops) noexcept {
    for (const operation& op : ops) {
        if (!is_well_formed(op.symbols) || !parens_balanced(op.symbols)) return false;
        bool ok = true;
        for_each_symbol(op.symbols, [&](std::string_view s) { ok = !s.empty(); return ok; });
        if (!ok) return false;
    }
    return true;
}

template <std::size_t N>
constexpr bool symbols_unique_per_operation(const std::array<operation, N>& ops) noexcept {
    for (const operation& op : ops) {
        const std::size_t n = symbol_count(op);
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n; ++j)
                if (symbol_at(op, i) == symbol_at(op, j)) return false;
    }
    return true;
}

}  // namespace detail
}  // namespace metanum::catalog

#endif  // METANUM_CATALOG_CORE_HPP
