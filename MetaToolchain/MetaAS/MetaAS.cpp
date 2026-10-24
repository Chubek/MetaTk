#include "MetaAS.h"
#include "../ArchAS/ArchAS.hpp"
#include <cstdio>
#include <cstring>

struct MetaAS {
    mair::Target target;
    archas::Options options;
    std::map<std::string, archas::Directive> directives;
    MetaASError error{};
    bool busy = false;
    explicit MetaAS(mair::Target t) : target(std::move(t)) {}
};
struct MetaASResult {
    archas::Output output;
    std::vector<std::string> dependencies;
    explicit MetaASResult(archas::Output o) : output(std::move(o)) {
        for (const auto &p : output.dependencies) dependencies.push_back(p.string());
    }
};
namespace {
void fail(MetaASError *e, int32_t code, const char *message, const mair::Location &loc = {}) noexcept {
    if (!e) return;
    *e = {}; e->code = code; e->line = loc.line; e->column = loc.column;
    std::snprintf(e->message, sizeof e->message, "%s", message);
    std::snprintf(e->file, sizeof e->file, "%s", loc.file.c_str());
}
template<class F> int32_t protect(MetaASError *e, F fn) noexcept {
    if (e) *e = {};
    try { fn(); return METAAS_OK; }
    catch (const std::bad_alloc &) { fail(e, METAAS_NO_MEMORY, "out of memory"); }
    catch (const archas::IOError &ex) { fail(e, METAAS_IO_ERROR, ex.what()); }
    catch (const std::filesystem::filesystem_error &ex) { fail(e, METAAS_IO_ERROR, ex.what()); }
    catch (const archas::Error &ex) { fail(e, METAAS_ERROR, ex.what(), ex.location); }
    catch (const std::invalid_argument &ex) { fail(e, METAAS_INVALID, ex.what()); }
    catch (const std::exception &ex) { fail(e, METAAS_ERROR, ex.what()); }
    catch (...) { fail(e, METAAS_ERROR, "unknown exception in assembler callback"); }
    return e ? e->code : METAAS_ERROR;
}
template<class F> int32_t operation(MetaAS *a, F fn) noexcept {
    if (!a) return METAAS_INVALID;
    if (a->busy) { fail(&a->error, METAAS_INVALID, "assembler callback reentry"); return METAAS_INVALID; }
    a->busy = true;
    auto rc = protect(&a->error, fn);
    a->busy = false;
    if (rc == METAAS_OK) a->error = {};
    return rc;
}
const char *text(const char *s) { return s ? s : ""; }
void required(const char *s) { if (!s || !*s) throw std::invalid_argument("expected nonempty string"); }
struct Emission {
    std::string source;
    size_t limit;
    char error[1024]{};
    static void report(void *p, const char *message) noexcept {
        auto &s = *static_cast<Emission *>(p);
        if (!*s.error) std::snprintf(s.error, sizeof s.error, "%s", message && *message ? message : "directive failed");
    }
    static int emit(void *p, const char *data, size_t size) noexcept {
        auto &s = *static_cast<Emission *>(p);
        if (*s.error) return -1;
        if ((!data && size) || size > s.limit - s.source.size()) { report(p, "invalid or oversized directive output"); return -1; }
        try { s.source.append(data ? data : "", size); return 0; }
        catch (...) { report(p, "out of memory in directive output"); return -1; }
    }
};
}
extern "C" {
uint32_t metaas_abi_version(void) { return METAAS_ABI_VERSION; }
void metaas_options_init(MetaASOptions *o) {
    if (!o) return;
    *o = {}; o->abi_version = METAAS_ABI_VERSION; o->struct_size = sizeof *o;
    o->max_bytes = 16 * 1024 * 1024; o->max_statements = 1000000; o->max_depth = 64;
    o->lua_memory = 64 * 1024 * 1024; o->lua_instructions = 10000000;
}
MetaAS *metaas_create(const MetaASOptions *o, MetaASError *error) {
    MetaASError local{};
    if (!error) error = &local;
    std::unique_ptr<MetaAS> a;
    auto rc = protect(error, [&] {
        if (!o || o->abi_version != METAAS_ABI_VERSION || o->struct_size < sizeof *o)
            throw std::invalid_argument("incompatible MetaAS options ABI");
        required(o->target);
        if (o->syntax < METAAS_ATT || o->syntax > METAAS_INTEL || o->backend < METAAS_AUTO || o->backend > METAAS_BUILTIN ||
            o->format < METAAS_OBJECT || o->format > METAAS_JSON || !o->max_bytes || !o->max_statements || !o->max_depth)
            throw std::invalid_argument("invalid assembler option or resource limit");
        a = std::make_unique<MetaAS>(archas::load_target(o->target, text(o->database), o->target_index, text(o->specification)));
        auto &v = a->options;
        v.syntax = static_cast<archas::Syntax>(o->syntax); v.backend = static_cast<archas::Backend>(o->backend);
        v.format = static_cast<archas::Format>(o->format);
        v.assembler = text(o->assembler); v.triple = text(o->triple); v.bfd_root = text(o->bfd_database);
        v.limits = {o->max_bytes, o->max_statements, o->max_depth, o->lua_memory, o->lua_instructions};
        (void)archas::Assembler(a->target, v); // Validate combinations at creation.
    });
    return rc == METAAS_OK ? a.release() : nullptr;
}
void metaas_free(MetaAS *a) { delete a; }
const MetaASError *metaas_error(const MetaAS *a) { return a ? &a->error : nullptr; }
int32_t metaas_add_include(MetaAS *a, const char *p) { return operation(a, [&] { required(p); a->options.include_paths.emplace_back(p); }); }
int32_t metaas_add_lua(MetaAS *a, const char *p) { return operation(a, [&] { required(p); a->options.lua_files.emplace_back(p); }); }
int32_t metaas_add_plugin(MetaAS *a, const char *p) { return operation(a, [&] { required(p); a->options.plugins.emplace_back(p); }); }
int32_t metaas_add_assembler_argument(MetaAS *a, const char *p) { return operation(a, [&] { required(p); a->options.assembler_args.emplace_back(p); }); }
int32_t metaas_define(MetaAS *a, const char *name, int64_t n) {
    return operation(a, [&] {
        required(name);
        if (!mair::ident(name) || !a->options.defines.emplace(name, n).second) throw std::invalid_argument("invalid or duplicate constant");
    });
}
int32_t metaas_register_directive(MetaAS *a, const char *name, ArchASDirective cb, void *user) {
    return operation(a, [&] {
        required(name);
        if (!cb) throw std::invalid_argument("NULL directive callback");
        archas::Directive callback = [cb, user, limit = a->options.limits.bytes](std::string_view args, const archas::DirectiveContext &ctx) {
            Emission out{{}, limit, {}};
            std::string argument(args);
            ArchASInvocation inv{argument.c_str(), argument.size(), ctx.location.file.c_str(), ctx.location.line,
                ctx.location.column, ctx.target.name().c_str(), ctx.syntax == archas::Syntax::intel ? "intel" : "att"};
            ArchASEmitter emit{&out, Emission::emit, Emission::report};
            int rc = cb(user, &inv, &emit);
            if (rc || *out.error) throw archas::Error(*out.error ? out.error : "directive callback failed", ctx.location);
            return std::move(out.source);
        };
        archas::Assembler validator(a->target, a->options);
        for (const auto &[key, fn] : a->directives) validator.directive(key, fn);
        validator.directive(name, callback);
        a->directives.emplace(name, std::move(callback));
    });
}
MetaASResult *metaas_compile(MetaAS *a, const void *data, size_t size, const char *file) {
    std::unique_ptr<MetaASResult> result;
    auto rc = operation(a, [&] {
        if (!data && size) throw std::invalid_argument("NULL source with nonzero length");
        archas::Assembler as(a->target, a->options);
        for (const auto &[name, callback] : a->directives) as.directive(name, callback);
        result = std::make_unique<MetaASResult>(as.compile({data ? static_cast<const char *>(data) : "", size}, file ? file : "<memory>"));
    });
    return rc == METAAS_OK ? result.release() : nullptr;
}
void metaas_result_free(MetaASResult *r) { delete r; }
const uint8_t *metaas_result_data(const MetaASResult *r) { return r ? reinterpret_cast<const uint8_t *>(r->output.data.data()) : nullptr; }
size_t metaas_result_size(const MetaASResult *r) { return r ? r->output.data.size() : 0; }
const char *metaas_result_diagnostics(const MetaASResult *r) { return r ? r->output.diagnostics.c_str() : nullptr; }
size_t metaas_result_dependency_count(const MetaASResult *r) { return r ? r->dependencies.size() : 0; }
const char *metaas_result_dependency(const MetaASResult *r, size_t i) { return r && i < r->dependencies.size() ? r->dependencies[i].c_str() : nullptr; }
}
