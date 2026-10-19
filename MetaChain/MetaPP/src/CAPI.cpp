#include "MetaPP-C.h"
#include "MetaPP.h"

#include <cstdio>
#include <utility>

struct metapp_context {
    metapp::Preprocessor pp;
    std::string output;
    char error[2048]{};
    explicit metapp_context(metapp::Options options) : pp(std::move(options)) {}
};

namespace {
template<class F> int call(metapp_context* context, F&& fn) noexcept {
    if (!context) return 1;
    context->error[0] = 0;
    try { fn(); return 0; }
    catch (const std::exception& e) { std::snprintf(context->error, sizeof(context->error), "%s", e.what()); }
    catch (...) { std::snprintf(context->error, sizeof(context->error), "%s", "unknown preprocessing error"); }
    return 1;
}
void require(const char* value) { if (!value) throw std::invalid_argument("NULL string argument"); }
} // namespace

extern "C" {
unsigned metapp_abi_version(void) { return METAPP_ABI_VERSION; }

metapp_context* metapp_create(const char* const* include_paths, size_t count) {
    try {
        if (count && !include_paths) return nullptr;
        metapp::Options options;
        for (size_t i = 0; i < count; ++i) { require(include_paths[i]); options.include_paths.emplace_back(include_paths[i]); }
        return new metapp_context(std::move(options));
    } catch (...) { return nullptr; }
}
void metapp_destroy(metapp_context* context) { delete context; }

int metapp_define(metapp_context* context, const char* definition) {
    return call(context, [&] { require(definition); context->pp.define(definition); });
}
int metapp_undefine(metapp_context* context, const char* name) {
    return call(context, [&] { require(name); context->pp.undefine(name); });
}
int metapp_process(metapp_context* context, const char* source, const char* filename) {
    return call(context, [&] {
        context->output.clear();
        require(source);
        context->output = context->pp.process(source, filename ? filename : "<input>");
    });
}
int metapp_process_file(metapp_context* context, const char* path) {
    return call(context, [&] {
        context->output.clear();
        require(path);
        context->output = context->pp.process_file(path);
    });
}
int metapp_register_kernel(metapp_context* context, const char* name, metapp_kernel_fn callback, void* userdata) {
    return call(context, [&] {
        require(name);
        if (!callback) throw std::invalid_argument("NULL kernel callback");
        context->pp.register_kernel(name, [callback, userdata](const metapp::Tokens& args, const metapp::Location& where) {
            std::string text = metapp::render(args);
            const char* result = callback(text.c_str(), where.file.c_str(), where.line, userdata);
            if (!result) throw metapp::Error(where, "foreign kernel callback failed");
            return metapp::tokenize(result, where);
        });
    });
}
const char* metapp_output(const metapp_context* context) { return context ? context->output.c_str() : ""; }
const char* metapp_last_error(const metapp_context* context) { return context ? context->error : "NULL MetaPP context"; }
size_t metapp_warning_count(const metapp_context* context) { return context ? context->pp.warnings().size() : 0; }
}
