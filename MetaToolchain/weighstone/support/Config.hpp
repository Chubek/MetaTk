#pragma once
#include <stdexcept>
#include <string_view>

namespace weighstone::support {
inline constexpr std::string_view version = "0.1.0";
inline void require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
}
