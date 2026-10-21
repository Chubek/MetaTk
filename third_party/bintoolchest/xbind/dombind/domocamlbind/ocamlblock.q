# ocamlblock.q — ocaml C API primitives.
@module ocamlblock
@version 1.0
@param PREFIX ocamlblock
@include <caml/mlvalues.h>
@include <caml/memory.h>
@include <caml/alloc.h>
@include <caml/callback.h>
@include <caml/fail.h>
@include <caml/threads.h>
@guard

@fn pair {
static inline value ${PREFIX}_pair(value a, value b) {
    CAMLparam2(a, b);
    CAMLlocal1(pair);
    pair = caml_alloc_tuple(2);
    Store_field(pair, 0, a); Store_field(pair, 1, b);
    CAMLreturn(pair);
}
}

@fn some {
static inline value ${PREFIX}_some(value v) {
    CAMLparam1(v);
    CAMLlocal1(result);
    result = caml_alloc(1, 0); Store_field(result, 0, v);
    CAMLreturn(result);
}
}

@fn none {
static inline value ${PREFIX}_none(void) {
    return Val_int(0);
}
}

@fn get {
static inline value ${PREFIX}_get(value block, mlsize_t i) {
    return Field(block, i);
}
}

@fn set {
static inline void ${PREFIX}_set(value block, mlsize_t i, value v) {
    caml_modify(&Field(block, i), v);
}
}

