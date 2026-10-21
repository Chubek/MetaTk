#include <domnodebind/domnodebind.hpp>
static napi_value init(napi_env env, napi_value exports) {
    return domnodebind::callback(env, [&]() {
        napi_value answer;
        domnodebind::check(nodeconv_from_int64(env, 42, &answer));
        domnodebind::check(nodeobj_set(env, exports, "answer", answer));
        return exports;
    });
}
nodemodule_define(answer, init)
