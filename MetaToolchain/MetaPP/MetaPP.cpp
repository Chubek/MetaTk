#include "MetaPP.h"
#include "MetaPP-Internal.hpp"
#include <cstdlib>
#include <cstring>
#include <new>
struct metapp_context { metapp::Engine engine; bool oom = false; };
namespace {
template<class F> int invoke(metapp_context *ctx, F operation) noexcept {
    if (!ctx) return 0;
    ctx->oom = false; ctx->engine.diagnostics.clear();
    try { operation(); return 1; }
    catch (const std::bad_alloc &) { ctx->oom = true; }
    catch (const std::exception &e) {
        try { ctx->engine.diagnostics += e.what(); ctx->engine.diagnostics += '\n'; }
        catch (...) { ctx->oom = true; }
    } catch (...) { ctx->oom = true; }
    return 0;
}
void output(metapp_context *ctx, const std::string &source, const char *filename, char **out) {
    ctx->engine.steps = 0; ctx->engine.once.clear();
    ctx->engine.expansion_bytes = 0; ctx->engine.files_processed = 0;
    auto text = ctx->engine.process(source, filename);
    char *memory = static_cast<char *>(std::malloc(text.size() + 1));
    if (!memory) throw std::bad_alloc();
    std::memcpy(memory, text.c_str(), text.size() + 1); *out = memory;
}
std::string bounded_source(const char *text) {
    size_t length = 0;
    while (length <= metapp::max_text_bytes && text[length]) ++length;
    if (length > metapp::max_text_bytes) throw metapp::Error("input exceeds 64 MiB limit");
    return std::string(text, length);
}
}
extern "C" {
metapp_context *metapp_create(void) { try { return new metapp_context; } catch (...) { return nullptr; } }
void metapp_destroy(metapp_context *ctx) { delete ctx; }
void metapp_free(void *p) { std::free(p); }
const char *metapp_diagnostics(const metapp_context *ctx) {
    if (!ctx) return "MetaPP: invalid context";
    return ctx->oom ? "MetaPP: out of memory" : ctx->engine.diagnostics.c_str();
}
int metapp_define(metapp_context *ctx, const char *text) {
    return invoke(ctx, [&] {
        if (!text) throw metapp::Error("NULL definition");
        auto ts = metapp::lex(bounded_source(text), "<command-line>");
        for (const auto &t : ts) if (t.kind == metapp::Kind::newline) throw metapp::Error("definition must occupy one logical line");
        ctx->engine.define(ts);
    });
}
int metapp_undefine(metapp_context *ctx, const char *name) {
    return invoke(ctx, [&] {
        if (!name || !metapp::identifier(name)) throw metapp::Error("invalid macro name");
        if (ctx->engine.builtin(name)) throw metapp::Error("cannot undefine reserved macro");
        ctx->engine.macros.erase(name);
    });
}
int metapp_add_include_path(metapp_context *ctx, const char *path) {
    return invoke(ctx, [&] {
        if (!path) throw metapp::Error("NULL include path");
        ctx->engine.paths.emplace_back(path);
    });
}
int metapp_preprocess(metapp_context *ctx, const char *source, const char *filename, char **out) {
    if (out) *out = nullptr;
    return invoke(ctx, [&] {
        if (!source || !filename || !out) throw metapp::Error("NULL preprocessing argument");
        output(ctx, bounded_source(source), filename, out);
    });
}
int metapp_preprocess_n(metapp_context *ctx, const char *source, size_t length, const char *filename, char **out) {
    if (out) *out = nullptr;
    return invoke(ctx, [&] {
        if (!source || !filename || !out) throw metapp::Error("NULL preprocessing argument");
        if (length > metapp::max_text_bytes) throw metapp::Error("input exceeds 64 MiB limit");
        output(ctx, std::string(source, length), filename, out);
    });
}
int metapp_preprocess_file(metapp_context *ctx, const char *filename, char **out) {
    if (out) *out = nullptr;
    return invoke(ctx, [&] {
        if (!filename || !out) throw metapp::Error("NULL preprocessing argument");
        output(ctx, metapp::read_file(filename), filename, out);
    });
}
}
