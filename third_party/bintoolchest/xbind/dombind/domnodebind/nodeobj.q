# nodeobj.q — node C API primitives.
@module nodeobj
@version 1.0
@param PREFIX nodeobj
@include <node_api.h>
@guard

@fn new {
static inline napi_status ${PREFIX}_new(napi_env e, napi_value *out) {
    return napi_create_object(e, out);
}
}

@fn get {
static inline napi_status ${PREFIX}_get(napi_env e, napi_value obj, const char *key, napi_value *out) {
    return napi_get_named_property(e, obj, key, out);
}
}

@fn set {
static inline napi_status ${PREFIX}_set(napi_env e, napi_value obj, const char *key, napi_value v) {
    return napi_set_named_property(e, obj, key, v);
}
}

@fn type {
static inline napi_status ${PREFIX}_type(napi_env e, napi_value v, napi_valuetype *out) {
    return napi_typeof(e, v, out);
}
}

@fn wrap {
static inline napi_status ${PREFIX}_wrap(napi_env e, napi_value obj, void *data, napi_finalize finalizer, void *hint, napi_ref *ref) {
    return napi_wrap(e, obj, data, finalizer, hint, ref);
}
}

@fn unwrap {
static inline napi_status ${PREFIX}_unwrap(napi_env e, napi_value obj, void **data) {
    return napi_unwrap(e, obj, data);
}
}

