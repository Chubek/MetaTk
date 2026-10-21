#pragma once
#include <domocamlbind/capi.h>
#include <memory>
#include <stdexcept>

namespace domocamlbind {
// Heap-allocated root slots survive C++ moves and OCaml moving collections.
// All operations require the runtime lock; destroy roots before caml_shutdown.
class root {
    std::unique_ptr<value> slot_;
public:
    explicit root(value v = Val_unit) : slot_(new value(v)) { ocamlroot_retain(slot_.get()); }
    root(const root &other) : root(other.get()) {}
    root(root &&other) noexcept : slot_(std::move(other.slot_)) {}
    root &operator=(root other) noexcept { slot_.swap(other.slot_); return *this; }
    ~root() { if (slot_) ocamlroot_release(slot_.get()); }
    value get() const noexcept { return slot_ ? *slot_ : Val_unit; }
    void set(value v) {
        if (!slot_) throw std::logic_error("empty OCaml root");
        ocamlroot_set(slot_.get(), v);
    }
};
class error : public std::runtime_error {
    root exception_;
public:
    explicit error(value exception) : std::runtime_error("OCaml exception"), exception_(exception) {}
    value exception() const noexcept { return exception_.get(); }
};
inline root call(value fn, value arg) {
    value result = ocamlcall_call(fn, arg);
    if (ocamlcall_is_exception(result)) throw error(ocamlcall_exception(result));
    return root(result);
}
} // namespace domocamlbind
