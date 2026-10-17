#include "qscript/include/QScript-Generated.h"
#include "qscript/src/Context.h"
#include "qscript/src/Runtime.h"
#include "qscript/src/RuntimeConfig.h"
#include <cstring>

struct qs_runtime {
    qscript::Unit unit;
    std::unique_ptr<qscript::detail::Session> session;
    const qs_context* owner = nullptr;
    qs_output_fn output = nullptr;
    void* user = nullptr;
    size_t printed = 0;
};
extern "C" {
qs_runtime* qs_runtime_create(qs_context* c, const qs_runtime_config* config) {
    qs_runtime* runtime = nullptr;
    qscript::detail::protect(c, [&] {
        if (!config) { c->error = "null runtime table"; return QS_INVALID_ARGUMENT; }
        auto unit = qscript::detail::runtime_unit(*config, c->options);
        if (!unit) { qscript::detail::report(c, unit.diagnostics); return QS_ERROR; }
        auto pending = std::make_unique<qs_runtime>();
        pending->owner = c; pending->output = c->output; pending->user = c->user;
        pending->unit = std::move(*unit);
        auto options = c->options;
        options.max_heap_objects = config->max_heap_objects; options.max_heap_bytes = config->max_heap_bytes;
        if (c->text) {
            auto fn = c->text; auto user = c->text_user;
            options.text_output = [fn, user](std::string_view text) { fn(text.data(), text.size(), user); };
        }
        pending->session = std::make_unique<qscript::detail::Session>(pending->unit, options);
        runtime = pending.release();
        return QS_OK;
    });
    return runtime;
}
void qs_runtime_destroy(qs_runtime* runtime) { delete runtime; }
qs_status qs_runtime_dispatch(qs_context* c, qs_runtime* runtime, int64_t id,
                              const int64_t* registers, size_t count, int64_t* result) {
    if (result) *result = 0;
    return qscript::detail::protect(c, [&] {
        if (!runtime || runtime->owner != c || !registers || count != 32 || !result || !qscript::detail::is_builtin_id(id)) {
            c->error = "invalid runtime dispatch argument"; return QS_INVALID_ARGUMENT;
        }
        *result = runtime->session->dispatch(id, std::span(registers, count));
        const auto& printed = runtime->session->printed_integers();
        while (runtime->printed < printed.size()) {
            const auto value = printed[runtime->printed++];
            if (runtime->output) runtime->output(value, runtime->user);
        }
        return QS_OK;
    });
}
}
