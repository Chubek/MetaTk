#pragma once
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
namespace metatex::util {
inline std::size_t checkedSize(std::size_t count, std::size_t size) {
    if (size && count > std::numeric_limits<std::size_t>::max() / size)
        throw std::length_error("allocation size overflow");
    return count * size;
}
template<class F> class ScopeExit {
public:
    explicit ScopeExit(F action) : action_(std::move(action)) {}
    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;
    ~ScopeExit() noexcept { if (active_) action_(); }
    void release() noexcept { active_ = false; }
private:
    F action_;
    bool active_ = true;
};
template<class T> std::shared_ptr<const T> immutable(T value) {
    return std::make_shared<const T>(std::move(value));
}
}
