#pragma once
#include <juniper/Juniper.hpp>
#include <stdexcept>
#include <utility>

// Application helpers used by the manual, not additions to Juniper's API.
namespace example {
template<class T>
T take(juniper::Result<T> result) {
    if (!result)
        throw std::runtime_error(juniper::format(*result.error()));
    return std::move(*result.get());
}

inline void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
} // namespace example
