# nodearray.q — node C API primitives.
@module nodearray
@version 1.0
@param PREFIX nodearray
@include <node_api.h>
@guard

@fn new {
static inline napi_status ${PREFIX}_new(napi_env e, size_t n, napi_value *out) {
    return napi_create_array_with_length(e, n, out);
}
}

@fn size {
static inline napi_status ${PREFIX}_size(napi_env e, napi_value a, uint32_t *out) {
    return napi_get_array_length(e, a, out);
}
}

@fn get {
static inline napi_status ${PREFIX}_get(napi_env e, napi_value a, uint32_t i, napi_value *out) {
    return napi_get_element(e, a, i, out);
}
}

@fn set {
static inline napi_status ${PREFIX}_set(napi_env e, napi_value a, uint32_t i, napi_value v) {
    return napi_set_element(e, a, i, v);
}
}

@fn buffer_copy {
static inline napi_status ${PREFIX}_buffer_copy(napi_env e, const void *data, size_t n, void **copy, napi_value *out) {
    return napi_create_buffer_copy(e, n, data, copy, out);
}
}

