#pragma once
#include <domjavabind/capi.h>
#include <stdexcept>
#include <utility>

namespace domjavabind {
class error : public std::runtime_error { using std::runtime_error::runtime_error; };
inline void check(JNIEnv *env) {
    if (javaerr_pending(env)) throw error("Java exception pending");
}
// Thread-bound RAII reference. JVM and JNIEnv must outlive every reference.
class global_ref {
    JNIEnv *env_ = nullptr; jobject value_ = nullptr;
public:
    global_ref() = default;
    global_ref(JNIEnv *env, jobject value) : env_(env), value_(javaref_retain(env, value)) {
        if (value && !value_) { check(env); throw std::bad_alloc(); }
    }
    global_ref(const global_ref &other) : global_ref(other.env_, other.value_, 0) {}
    global_ref(global_ref &&other) noexcept : env_(other.env_), value_(std::exchange(other.value_, nullptr)) {}
    global_ref &operator=(global_ref other) noexcept { swap(other); return *this; }
    ~global_ref() { if (value_) javaref_release(env_, value_); }
    void swap(global_ref &other) noexcept { std::swap(env_, other.env_); std::swap(value_, other.value_); }
    jobject get() const noexcept { return value_; }
private:
    global_ref(JNIEnv *env, jobject value, int) : env_(env) {
        if (value) { value_ = javaref_retain(env, value); if (!value_) { check(env); throw std::bad_alloc(); } }
    }
};
class local_frame {
    JNIEnv *env_;
public:
    explicit local_frame(JNIEnv *env, jint capacity = 16) : env_(env) {
        if (javaref_frame_push(env_, capacity) != JNI_OK) throw error("JNI local frame allocation failed");
    }
    ~local_frame() { (void)javaref_frame_pop(env_, nullptr); }
    local_frame(const local_frame &) = delete;
    local_frame &operator=(const local_frame &) = delete;
};
} // namespace domjavabind
