#pragma once
#include <domnodebind/capi.h>
#include <stdexcept>
#include <utility>

namespace domnodebind {
class error : public std::runtime_error {
    napi_status status_;
public:
    explicit error(napi_status status) : std::runtime_error("Node-API operation failed"), status_(status) {}
    napi_status status() const noexcept { return status_; }
};
inline void check(napi_status status) { if (status != napi_ok) throw error(status); }
// All instances are confined to their environment's JS thread and lifetime.
class scope {
    napi_env env_; napi_handle_scope scope_;
public:
    explicit scope(napi_env env) : env_(env) { check(noderef_scope_open(env_, &scope_)); }
    ~scope() { (void)noderef_scope_close(env_, scope_); }
    scope(const scope &) = delete;
    scope &operator=(const scope &) = delete;
};
class reference {
    napi_env env_ = nullptr; napi_ref ref_ = nullptr;
public:
    reference() = default;
    reference(napi_env env, napi_value value) : env_(env) { check(noderef_retain(env_, value, &ref_)); }
    reference(const reference &other) : env_(other.env_) {
        if (other.ref_) check(noderef_retain(env_, other.get(), &ref_));
    }
    reference(reference &&other) noexcept : env_(other.env_), ref_(std::exchange(other.ref_, nullptr)) {}
    reference &operator=(reference other) noexcept { swap(other); return *this; }
    ~reference() { if (ref_) (void)noderef_release(env_, ref_); }
    void swap(reference &other) noexcept { std::swap(env_, other.env_); std::swap(ref_, other.ref_); }
    napi_value get() const {
        if (!ref_) throw error(napi_invalid_arg);
        napi_value value; check(noderef_get(env_, ref_, &value)); return value;
    }
};
// Enclose the entire callback body so C++ exceptions never reach Node.
template<class F> napi_value callback(napi_env env, F &&fn) noexcept {
    try { return fn(); }
    catch (const std::exception &e) {
        bool pending = false;
        if (nodeerr_pending(env, &pending) == napi_ok && !pending) (void)nodeerr_throw(env, nullptr, e.what());
    } catch (...) { (void)nodeerr_throw(env, nullptr, "native callback failed"); }
    return nullptr;
}
} // namespace domnodebind
