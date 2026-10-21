#pragma once
#include <domrubybind/capi.h>
#include <memory>
#include <stdexcept>
#include <utility>

namespace domrubybind {
// Root addresses must not move. The slot is allocated independently of this handle.
// Construct/destroy with the GVL held, before ruby_cleanup.
class object {
    std::unique_ptr<VALUE> slot_;
public:
    explicit object(VALUE v = Qnil) : slot_(new VALUE(v)) { rubyobj_retain(slot_.get()); }
    object(const object &other) : object(other.get()) {}
    object(object &&other) noexcept : slot_(std::move(other.slot_)) {}
    object &operator=(object other) noexcept { slot_.swap(other.slot_); return *this; }
    ~object() { if (slot_) rubyobj_release(slot_.get()); }
    VALUE get() const noexcept { return slot_ ? *slot_ : Qnil; }
};
class error : public std::runtime_error {
    object exception_;
public:
    explicit error(VALUE exception) : std::runtime_error("Ruby exception"), exception_(exception) {}
    VALUE exception() const noexcept { return exception_.get(); }
};
// fn must contain no C++ automatic objects needing destruction across a Ruby jump.
inline object protect(VALUE (*fn)(VALUE), VALUE data) {
    int status = 0;
    VALUE result = rubycall_protect(fn, data, &status);
    if (status) {
        error failure(rubycall_error()); rubycall_clear_error(); throw failure;
    }
    return object(result);
}
inline object eval(const char *code) {
    int status = 0;
    VALUE result = rubycall_eval(code, &status);
    if (status) {
        error failure(rubycall_error()); rubycall_clear_error(); throw failure;
    }
    return object(result);
}
} // namespace domrubybind
