# ocamlcall.q — ocaml C API primitives.
@module ocamlcall
@version 1.0
@param PREFIX ocamlcall
@include <caml/mlvalues.h>
@include <caml/memory.h>
@include <caml/alloc.h>
@include <caml/callback.h>
@include <caml/fail.h>
@include <caml/threads.h>
@guard

@fn named {
static inline const value * ${PREFIX}_named(const char *name) {
    return caml_named_value(name);
}
}

@fn call {
static inline value ${PREFIX}_call(value fn, value arg) {
    CAMLparam2(fn, arg);
    CAMLlocal1(result);
    result = caml_callback_exn(fn, arg);
    CAMLreturn(result);
}
}

@fn call2 {
static inline value ${PREFIX}_call2(value fn, value a, value b) {
    CAMLparam3(fn, a, b);
    CAMLlocal1(result);
    result = caml_callback2_exn(fn, a, b);
    CAMLreturn(result);
}
}

@fn is_exception {
static inline int ${PREFIX}_is_exception(value result) {
    return Is_exception_result(result);
}
}

@fn exception {
static inline value ${PREFIX}_exception(value result) {
    return Extract_exception(result);
}
}

