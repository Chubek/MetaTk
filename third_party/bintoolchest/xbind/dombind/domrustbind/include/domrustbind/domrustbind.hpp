#pragma once
#include <domrustbind/capi.h>
#include <new>
#include <stdexcept>
#include <utility>
namespace domrustbind {
class buffer {
    rustbuf_t value_ = {nullptr, 0};
public:
    buffer() = default;
    buffer(const void *data, size_t size) {
        int status = rustbuf_copy(&value_, data, size);
        if (status == -1) throw std::invalid_argument("invalid buffer source");
        if (status) throw std::bad_alloc();
    }
    buffer(const buffer &other) : buffer(other.data(), other.size()) {}
    buffer(buffer &&other) noexcept : value_(other.release()) {}
    buffer &operator=(buffer other) noexcept { std::swap(value_, other.value_); return *this; }
    ~buffer() { rustbuf_free(&value_); }
    unsigned char *data() noexcept { return value_.data; }
    const unsigned char *data() const noexcept { return value_.data; }
    size_t size() const noexcept { return value_.size; }
    rustbuf_t release() noexcept { auto old = value_; value_ = {nullptr, 0}; return old; }
};
} // namespace domrustbind
