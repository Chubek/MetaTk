#pragma once
#include <cstdint>
#include <stdexcept>
#include <string_view>
namespace metatex::util {
inline std::uint32_t nextUTF8(std::string_view text, std::size_t& at) {
    if (at >= text.size()) throw std::out_of_range("UTF-8 cursor");
    auto c = static_cast<unsigned char>(text[at++]);
    if (c < 128) return c;
    int count; std::uint32_t value, minimum;
    if (c >= 0xc2 && c <= 0xdf) { count = 1; value = c & 31; minimum = 0x80; }
    else if (c >= 0xe0 && c <= 0xef) { count = 2; value = c & 15; minimum = 0x800; }
    else if (c >= 0xf0 && c <= 0xf4) { count = 3; value = c & 7; minimum = 0x10000; }
    else throw std::invalid_argument("invalid UTF-8 leading byte");
    while (count--) {
        if (at == text.size()) throw std::invalid_argument("truncated UTF-8");
        c = static_cast<unsigned char>(text[at++]);
        if ((c & 0xc0) != 0x80) throw std::invalid_argument("invalid UTF-8 continuation");
        value = (value << 6) | (c & 63);
    }
    if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
        throw std::invalid_argument("invalid UTF-8 scalar");
    return value;
}
inline void validateUTF8(std::string_view text) {
    std::size_t at = 0; while (at < text.size()) (void)nextUTF8(text, at);
}
}
