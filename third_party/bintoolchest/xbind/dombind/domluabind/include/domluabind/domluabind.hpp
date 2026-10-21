#pragma once
#include <domluabind/capi.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace domluabind {
class error : public std::runtime_error { using std::runtime_error::runtime_error; };
class stack_guard {
    lua_State *state_; int top_;
public:
    explicit stack_guard(lua_State *s) : state_(s), top_(lua_gettop(s)) {}
    ~stack_guard() { lua_settop(state_, top_); }
    stack_guard(const stack_guard &) = delete;
    stack_guard &operator=(const stack_guard &) = delete;
};
class object;
class state {
    std::shared_ptr<lua_State> state_;
    friend class object;
public:
    explicit state(bool libraries = true) : state_(luainit_new(), luainit_close) {
        if (!state_) throw std::bad_alloc();
        if (libraries) luainit_openlibs(state_.get());
    }
    lua_State *get() const noexcept { return state_.get(); }
    // Leaves the requested results on the stack; restores it on failure.
    void eval(const std::string &code, int results = 0) const {
        int top = lua_gettop(get());
        if (luacall_eval(get(), code.data(), code.size(), results) != LUA_OK) {
            const char *message = lua_type(get(), -1) == LUA_TSTRING ? lua_tostring(get(), -1) : nullptr;
            std::string text = message ? message : "Lua evaluation failed";
            lua_settop(get(), top);
            throw error(text);
        }
    }
};
// Registry references keep the state alive and are copied by retaining the value.
class object {
    std::shared_ptr<lua_State> state_;
    int ref_ = LUA_NOREF;
public:
    object() = default;
    object(const state &s, int index) : state_(s.state_), ref_(luaref_retain(s.get(), index)) {}
    object(const object &other) : state_(other.state_) {
        if (state_) { other.push(); ref_ = luaL_ref(state_.get(), LUA_REGISTRYINDEX); }
    }
    object(object &&other) noexcept : state_(std::move(other.state_)), ref_(other.ref_) { other.ref_ = LUA_NOREF; }
    object &operator=(object other) noexcept { swap(other); return *this; }
    ~object() { if (state_) luaref_release(state_.get(), ref_); }
    void swap(object &other) noexcept { state_.swap(other.state_); std::swap(ref_, other.ref_); }
    void push() const {
        if (!state_) throw error("empty Lua object");
        if (ref_ == LUA_REFNIL) lua_pushnil(state_.get());
        else luaref_push(state_.get(), ref_);
    }
};
} // namespace domluabind
