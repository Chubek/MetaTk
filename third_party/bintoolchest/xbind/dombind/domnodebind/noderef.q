# noderef.q — node C API primitives.
@module noderef
@version 1.0
@param PREFIX noderef
@include <node_api.h>
@guard

@fn retain {
static inline napi_status ${PREFIX}_retain(napi_env e, napi_value obj, napi_ref *out) {
    return napi_create_reference(e, obj, 1, out);
}
}

@fn get {
static inline napi_status ${PREFIX}_get(napi_env e, napi_ref ref, napi_value *out) {
    return napi_get_reference_value(e, ref, out);
}
}

@fn release {
static inline napi_status ${PREFIX}_release(napi_env e, napi_ref ref) {
    return napi_delete_reference(e, ref);
}
}

@fn scope_open {
static inline napi_status ${PREFIX}_scope_open(napi_env e, napi_handle_scope *out) {
    return napi_open_handle_scope(e, out);
}
}

@fn scope_close {
static inline napi_status ${PREFIX}_scope_close(napi_env e, napi_handle_scope scope) {
    return napi_close_handle_scope(e, scope);
}
}

