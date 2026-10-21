# nodecall.q — node C API primitives.
@module nodecall
@version 1.0
@param PREFIX nodecall
@include <node_api.h>
@guard

@fn new {
static inline napi_status ${PREFIX}_new(napi_env e, const char *name, napi_callback fn, void *data, napi_value *out) {
    return napi_create_function(e, name, NAPI_AUTO_LENGTH, fn, data, out);
}
}

@fn args {
static inline napi_status ${PREFIX}_args(napi_env e, napi_callback_info info, size_t *argc, napi_value *argv, napi_value *self, void **data) {
    return napi_get_cb_info(e, info, argc, argv, self, data);
}
}

@fn invoke {
static inline napi_status ${PREFIX}_invoke(napi_env e, napi_value self, napi_value fn, size_t argc, const napi_value *argv, napi_value *out) {
    return napi_call_function(e, self, fn, argc, argv, out);
}
}

@fn construct {
static inline napi_status ${PREFIX}_construct(napi_env e, napi_value fn, size_t argc, const napi_value *argv, napi_value *out) {
    return napi_new_instance(e, fn, argc, argv, out);
}
}

