# ocamlconv.q — ocaml C API primitives.
@module ocamlconv
@version 1.0
@param PREFIX ocamlconv
@include <caml/mlvalues.h>
@include <caml/memory.h>
@include <caml/alloc.h>
@include <caml/callback.h>
@include <caml/fail.h>
@include <caml/threads.h>
@guard

@fn from_int {
static inline value ${PREFIX}_from_int(intnat v) {
    return Val_long(v);
}
}

@fn to_int {
static inline intnat ${PREFIX}_to_int(value v) {
    return Long_val(v);
}
}

@fn from_int64 {
static inline value ${PREFIX}_from_int64(int64_t v) {
    return caml_copy_int64(v);
}
}

@fn to_int64 {
static inline int64_t ${PREFIX}_to_int64(value v) {
    return Int64_val(v);
}
}

@fn from_double {
static inline value ${PREFIX}_from_double(double v) {
    return caml_copy_double(v);
}
}

@fn to_double {
static inline double ${PREFIX}_to_double(value v) {
    return Double_val(v);
}
}

@fn string {
static inline value ${PREFIX}_string(const char *s, mlsize_t n) {
    return caml_alloc_initialized_string(n, s);
}
}

@fn string_size {
static inline mlsize_t ${PREFIX}_string_size(value v) {
    return caml_string_length(v);
}
}

@fn string_data {
static inline const char * ${PREFIX}_string_data(value v) {
    return String_val(v);
}
}

