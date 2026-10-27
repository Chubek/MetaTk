#pragma once
#include <cstdint>
#include <string_view>
namespace metatex::util {
// Stable FNV-1a for identifiers/cache keys, not cryptographic authentication.
constexpr std::uint64_t hash64(std::string_view bytes) noexcept {
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : bytes) { hash ^= c; hash *= 1099511628211ULL; }
    return hash;
}
constexpr std::uint32_t crc32(std::string_view bytes) noexcept {
    std::uint32_t crc = 0xffffffffU;
    for (unsigned char c : bytes) {
        crc ^= c;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
}
