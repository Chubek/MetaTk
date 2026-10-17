#pragma once

// Shared, locale-independent text primitives and the DSLtk dependency boundary.
#if defined(METACHAIN_DSLTK_HEADER)
#include METACHAIN_DSLTK_HEADER
#elif __has_include(<DSLtk.hpp>)
#include <DSLtk.hpp>
#elif __has_include("../../DSLtk/DSLtk.hpp")
#include "../../DSLtk/DSLtk.hpp"
#else
#error "MetaChain requires DSLtk.hpp; add its directory to the include path"
#endif

#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace metachain::text {
inline bool space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
inline bool alpha(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
inline bool digit(char c) noexcept { return c >= '0' && c <= '9'; }
inline bool identifier(std::string_view s) noexcept {
    if (s.empty() || !(alpha(s.front()) || s.front() == '.' || s.front() == '$')) return false;
    for (char c : s) if (!(alpha(c) || digit(c) || c == '.' || c == '$')) return false;
    return true;
}
inline std::string_view trim(std::string_view s) noexcept {
    while (!s.empty() && space(s.front())) s.remove_prefix(1);
    while (!s.empty() && space(s.back())) s.remove_suffix(1);
    return s;
}
inline std::string lower(std::string_view s) {
    std::string r(s);
    for (char& c : r) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return r;
}
struct Integer {
    std::uint64_t magnitude = 0;
    bool negative = false;
    std::uint64_t bits() const noexcept { return negative ? 0 - magnitude : magnitude; }
    bool fits(unsigned width) const noexcept {
        if (width == 0 || width > 8) return false;
        if (negative) return magnitude <= (std::uint64_t{1} << (width * 8 - 1));
        return width == 8 || magnitude < (std::uint64_t{1} << (width * 8));
    }
};
inline std::optional<Integer> integer(std::string_view s) {
    s = trim(s);
    Integer r;
    if (!s.empty() && (s.front() == '+' || s.front() == '-')) {
        r.negative = s.front() == '-'; s.remove_prefix(1);
    }
    int base = 10;
    if (s.size() >= 2 && s[0] == '0') {
        switch (s[1]) {
        case 'x': case 'X': base = 16; break;
        case 'b': case 'B': base = 2; break;
        case 'o': case 'O': base = 8; break;
        default: break;
        }
        if (base != 10) s.remove_prefix(2);
    }
    if (s.empty()) return std::nullopt;
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), r.magnitude, base);
    if (ec != std::errc{} || p != s.data() + s.size()) return std::nullopt;
    return r;
}
inline std::optional<std::int64_t> signed_integer(std::string_view s) {
    auto v = integer(s);
    if (!v || v->magnitude > std::uint64_t{INT64_MAX} + (v->negative ? 1u : 0u)) return std::nullopt;
    if (v->negative && v->magnitude == (std::uint64_t{1} << 63)) return INT64_MIN;
    return v->negative ? -static_cast<std::int64_t>(v->magnitude) : static_cast<std::int64_t>(v->magnitude);
}
inline std::string_view uncomment(std::string_view s) {
    char quote = 0; bool escape = false;
    for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (quote) {
            if (escape) escape = false;
            else if (c == '\\') escape = true;
            else if (c == quote) quote = 0;
        } else if (c == '\'' || c == '"') quote = c;
        else if (c == ';' || c == '#') return s.substr(0, i);
    }
    return s;
}
inline std::vector<std::string_view> operands(std::string_view s) {
    std::vector<std::string_view> r;
    char quote = 0; bool escape = false; std::size_t start = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (quote) {
            if (escape) escape = false;
            else if (c == '\\') escape = true;
            else if (c == quote) quote = 0;
        } else if (c == '\'' || c == '"') quote = c;
        else if (c == ',') { r.push_back(trim(s.substr(start, i - start))); start = i + 1; }
    }
    if (start || !trim(s).empty()) r.push_back(trim(s.substr(start)));
    return r;
}
inline std::optional<std::string> unquote(std::string_view s) {
    s = trim(s);
    if (s.size() < 2 || (s[0] != '\'' && s[0] != '"') || s.back() != s[0]) return std::nullopt;
    std::string r;
    for (std::size_t i = 1; i < s.size() - 1; ++i) {
        char c = s[i];
        if (c == s[0]) return std::nullopt;
        if (c == '\\') {
            if (++i >= s.size() - 1) return std::nullopt;
            switch (s[i]) {
            case 'n': c = '\n'; break; case 'r': c = '\r'; break;
            case 't': c = '\t'; break; case '0': c = '\0'; break;
            case '\\': c = '\\'; break; case '\'': c = '\''; break; case '"': c = '"'; break;
            case 'x': {
                if (i + 2 >= s.size() - 1) return std::nullopt;
                unsigned v = 0;
                auto [p, ec] = std::from_chars(s.data() + i + 1, s.data() + i + 3, v, 16);
                if (ec != std::errc{} || p != s.data() + i + 3) return std::nullopt;
                c = static_cast<char>(v); i += 2; break;
            }
            default: return std::nullopt;
            }
        }
        r.push_back(c);
    }
    return r;
}
} // namespace metachain::text
