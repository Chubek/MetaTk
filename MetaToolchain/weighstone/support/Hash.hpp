#pragma once
#include <cstddef>
#include <functional>
namespace weighstone::support {
inline void hash_combine(std::size_t& seed, std::size_t hash) noexcept {
    seed ^= hash + static_cast<std::size_t>(0x9e3779b97f4a7c15ULL) + (seed << 6) + (seed >> 2);
}
template<class T> inline void hash_value(std::size_t& seed, const T& x) {
    hash_combine(seed, std::hash<T>{}(x));
}
}
