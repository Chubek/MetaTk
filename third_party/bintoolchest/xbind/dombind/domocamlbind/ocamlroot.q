# ocamlroot.q — ocaml C API primitives.
@module ocamlroot
@version 1.0
@param PREFIX ocamlroot
@include <caml/mlvalues.h>
@include <caml/memory.h>
@include <caml/alloc.h>
@include <caml/callback.h>
@include <caml/fail.h>
@include <caml/threads.h>
@guard

@fn retain {
static inline void ${PREFIX}_retain(value *slot) {
    caml_register_generational_global_root(slot);
}
}

@fn set {
static inline void ${PREFIX}_set(value *slot, value v) {
    caml_modify_generational_global_root(slot, v);
}
}

@fn release {
static inline void ${PREFIX}_release(value *slot) {
    caml_remove_generational_global_root(slot);
}
}

