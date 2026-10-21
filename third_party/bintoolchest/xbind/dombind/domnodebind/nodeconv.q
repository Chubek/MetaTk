# nodeconv.q — node C API primitives.
@module nodeconv
@version 1.0
@param PREFIX nodeconv
@include <node_api.h>
@guard

@fn from_int64 {
static inline napi_status ${PREFIX}_from_int64(napi_env e, int64_t v, napi_value *out) {
    return napi_create_int64(e, v, out);
}
}

@fn to_int64 {
static inline napi_status ${PREFIX}_to_int64(napi_env e, napi_value v, int64_t *out) {
    return napi_get_value_int64(e, v, out);
}
}

@fn from_double {
static inline napi_status ${PREFIX}_from_double(napi_env e, double v, napi_value *out) {
    return napi_create_double(e, v, out);
}
}

@fn to_double {
static inline napi_status ${PREFIX}_to_double(napi_env e, napi_value v, double *out) {
    return napi_get_value_double(e, v, out);
}
}

@fn from_boolean {
static inline napi_status ${PREFIX}_from_boolean(napi_env e, bool v, napi_value *out) {
    return napi_get_boolean(e, v, out);
}
}

@fn to_boolean {
static inline napi_status ${PREFIX}_to_boolean(napi_env e, napi_value v, bool *out) {
    return napi_get_value_bool(e, v, out);
}
}

@fn string {
static inline napi_status ${PREFIX}_string(napi_env e, const char *s, size_t n, napi_value *out) {
    return napi_create_string_utf8(e, s, n, out);
}
}

@fn string_copy {
static inline napi_status ${PREFIX}_string_copy(napi_env e, napi_value v, char *s, size_t cap, size_t *n) {
    return napi_get_value_string_utf8(e, v, s, cap, n);
}
}

@fn bigint {
static inline napi_status ${PREFIX}_bigint(napi_env e, int64_t v, napi_value *out) {
    return napi_create_bigint_int64(e, v, out);
}
}

@fn bigint_value {
static inline napi_status ${PREFIX}_bigint_value(napi_env e, napi_value v, int64_t *out, bool *lossless) {
    return napi_get_value_bigint_int64(e, v, out, lossless);
}
}

