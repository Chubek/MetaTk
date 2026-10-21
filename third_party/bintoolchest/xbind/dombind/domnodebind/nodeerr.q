# nodeerr.q — node C API primitives.
@module nodeerr
@version 1.0
@param PREFIX nodeerr
@include <node_api.h>
@guard

@fn throw {
static inline napi_status ${PREFIX}_throw(napi_env e, const char *code, const char *message) {
    return napi_throw_error(e, code, message);
}
}

@fn pending {
static inline napi_status ${PREFIX}_pending(napi_env e, bool *out) {
    return napi_is_exception_pending(e, out);
}
}

@fn take {
static inline napi_status ${PREFIX}_take(napi_env e, napi_value *out) {
    return napi_get_and_clear_last_exception(e, out);
}
}

