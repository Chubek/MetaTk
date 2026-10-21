# nodemodule.q — node C API primitives.
@module nodemodule
@version 1.0
@param PREFIX nodemodule
@include <node_api.h>
@guard

@raw {
#define ${PREFIX}_define(name, init) NAPI_MODULE(name, init)
}

