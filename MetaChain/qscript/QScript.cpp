#include "QScript.h"
#include "qscript/include/QScript-Backend.h"
#include "qscript/include/QScript-Diagnostics.h"
#include "qscript/include/QScript-Grammar.h"
#include "qscript/include/QScript-Library.h"
#include "qscript/include/QScript-Module.h"
#include "qscript/include/QScript-Runtime.h"
#include "qscript/src/Context.h"
#include "qscript/src/Internal.h"
#include <cstring>
#include <fstream>
#include <new>
#include <sstream>

struct qs_module { qscript::module::Module module; };

namespace qscript::detail {
Options effective_options(const qs_context* c) {
    Options o = c->options;
    if (c->resolver) {
        auto fn = c->resolver; auto user = c->resolver_user;
        auto previous = o.resolver;
        o.resolver = [fn, user, previous](std::string_view name) -> std::optional<std::string> {
            if (previous) if (auto text = previous(name)) return text;
            std::string key(name);
            size_t length = 0;
            const char* text = fn(key.c_str(), &length, user);
            if (!text) return std::nullopt;
            return std::string(text, length);
        };
    }
    return o;
}
} // namespace qscript::detail

using qscript::detail::protect;
using qscript::detail::report;
using qscript::detail::text_out;

extern "C" {
qs_context* qs_create(void) { try { return new qs_context; } catch (...) { return nullptr; } }
void qs_destroy(qs_context* c) { delete c; }
const char* qs_error(const qs_context* c) { return c ? c->error.c_str() : "null QScript context"; }
void qs_set_output(qs_context* c, qs_output_fn fn, void* user) { if (c) { c->output = fn; c->user = user; } }
void qs_set_step_limit(qs_context* c, size_t limit) { if (c) c->options.toolchain.vm.max_steps = limit; }
qs_program* qs_compile(qs_context* c, const char* source, size_t length) {
    qs_program* p = nullptr;
    protect(c, [&] {
        if (!source && length) { c->error = "null source"; return QS_INVALID_ARGUMENT; }
        auto compiled = qscript::compile_unit(std::string_view(source ? source : "", length), qscript::detail::effective_options(c));
        if (!compiled) { report(c, compiled.diagnostics); return QS_ERROR; }
        p = new qs_program{std::move(*compiled)}; return QS_OK;
    });
    return p;
}
void qs_program_destroy(qs_program* p) { delete p; }

static qs_status execute(qs_context* c, const qs_program* p, qs_run_info* info) {
    if (info) *info = qs_run_info{0, 0, 0, QS_RUN_TRAPPED};
    if (!p) { c->error = "null program"; return QS_INVALID_ARGUMENT; }
    metachain::Outcome<metail::Execution> executed;
    if (c->engine == QS_ENGINE_JIT) {
        auto jitted = metachain::Toolchain(c->options.toolchain).jit(p->unit.program);
        executed.diagnostics = std::move(jitted.diagnostics);
        if (jitted.value) executed.value = static_cast<metail::Execution>(std::move(*jitted.value));
    } else executed = qscript::run(p->unit.program, c->options);
    if (executed.value) {
        if (c->output) for (auto value : executed->output) c->output(value, c->user);
        if (info) {
            info->steps = executed->steps; info->outputs = executed->output.size(); info->result = executed->value;
            info->status = executed->status == metavm::Status::Halted ? QS_RUN_HALTED :
                           executed->status == metavm::Status::StepLimit ? QS_RUN_STEP_LIMIT : QS_RUN_TRAPPED;
        }
    }
    if (!executed) { report(c, executed.diagnostics); return QS_ERROR; }
    return QS_OK;
}
qs_status qs_run(qs_context* c, const qs_program* p, int64_t* result) {
    if (result) *result = 0;
    return protect(c, [&] {
        if (!p || !result) { c->error = "null program or result"; return QS_INVALID_ARGUMENT; }
        qs_run_info info;
        auto status = execute(c, p, &info);
        if (status == QS_OK) *result = info.result;
        return status;
    });
}
qs_status qs_emit(qs_context* c, const qs_program* p, int format, const unsigned char** data, size_t* length) {
    if (data) *data = nullptr;
    if (length) *length = 0;
    return protect(c, [&] {
        if (!p || !data || !length) { c->error = "null emit argument"; return QS_INVALID_ARGUMENT; }
        const auto& ir = p->unit.program;
        if (format == QS_IL || format == QS_NATIVE) {
            auto text = format == QS_IL ? qscript::emit(ir, c->options) : qscript::select(ir, c->options);
            if (!text) { report(c, text.diagnostics); return QS_ERROR; }
            c->buffer.assign(text->begin(), text->end());
        } else if (format == QS_BYTECODE) c->buffer = metail::encode(ir);
        else if (format == QS_OBJECT || format == QS_EXECUTABLE) {
            auto source = qscript::select(ir, c->options);
            if (!source) { report(c, source.diagnostics); return QS_ERROR; }
            auto object = qscript::assemble(*source, true, c->options);
            if (!object) { report(c, object.diagnostics); return QS_ERROR; }
            auto bytes = format == QS_OBJECT ? std::move(object) : qscript::link({*object}, c->options);
            if (!bytes) { report(c, bytes.diagnostics); return QS_ERROR; }
            c->buffer = std::move(*bytes);
        } else { c->error = "invalid output format"; return QS_INVALID_ARGUMENT; }
        *data = c->buffer.data(); *length = c->buffer.size(); return QS_OK;
    });
}

/* ---- QScript-Backend.h -------------------------------------------------- */
qs_status qs_set_backend(qs_context* c, const qs_backend_options* o) {
    if (!c || !o) return QS_INVALID_ARGUMENT;
    qscript::Scheduler scheduler;
    switch (o->scheduler) {
    case QS_SCHED_LIST: scheduler = qscript::Scheduler::List; break;
    case QS_SCHED_CYCLE: scheduler = qscript::Scheduler::Cycle; break;
    case QS_SCHED_STATIC: scheduler = qscript::Scheduler::Static; break;
    default: return QS_INVALID_ARGUMENT;
    }
    if (o->issue_width != 0 && o->issue_width > 1048576) return QS_INVALID_ARGUMENT;
    if (o->optimize >= 0) c->options.toolchain.optimize = o->optimize != 0;
    c->options.backend.scheduler = scheduler;
    if (o->issue_width != 0) c->options.backend.issue_width = o->issue_width;
    return QS_OK;
}
qs_status qs_get_backend(const qs_context* c, qs_backend_options* o) {
    if (!c || !o) return QS_INVALID_ARGUMENT;
    o->scheduler = c->options.backend.scheduler == qscript::Scheduler::Cycle ? QS_SCHED_CYCLE :
                   c->options.backend.scheduler == qscript::Scheduler::Static ? QS_SCHED_STATIC : QS_SCHED_LIST;
    o->issue_width = c->options.backend.issue_width;
    o->optimize = c->options.toolchain.optimize ? 1 : 0;
    return QS_OK;
}
qs_status qs_set_scheduler_name(qs_context* c, const char* name) {
    if (!c || !name) return QS_INVALID_ARGUMENT;
    auto s = qscript::backend::scheduler_from_name(name);
    if (!s) return QS_INVALID_ARGUMENT;
    c->options.backend.scheduler = *s;
    return QS_OK;
}
const char* qs_scheduler_name(int scheduler) {
    switch (scheduler) {
    case QS_SCHED_LIST: return "list";
    case QS_SCHED_CYCLE: return "cycle";
    case QS_SCHED_STATIC: return "static";
    default: return nullptr;
    }
}
qs_status qs_set_optimize(qs_context* c, int enable) {
    if (!c) return QS_INVALID_ARGUMENT;
    c->options.toolchain.optimize = enable != 0;
    return QS_OK;
}
qs_status qs_backend_describe(const qs_context* c, char* text, size_t capacity, size_t* length) {
    const std::string description = c ? qscript::backend::describe(c->options) : std::string("no context");
    if (length) *length = description.size();
    if (!c) return QS_INVALID_ARGUMENT;
    if (text && capacity) {
        const auto copy = std::min(capacity - 1, description.size());
        std::memcpy(text, description.data(), copy);
        text[copy] = '\0';
    } else if (text) return QS_INVALID_ARGUMENT;
    return QS_OK;
}
qs_status qs_backend_report(qs_context* c, const qs_program* p, const char** text, size_t* length) {
    if (text) *text = nullptr;
    if (length) *length = 0;
    return protect(c, [&] {
        if (!p || !text || !length) { c->error = "null report argument"; return QS_INVALID_ARGUMENT; }
        std::ostringstream out;
        const auto blocks = qscript::backend::report(p->unit.program, c->options);
        std::size_t total = 0;
        out << qscript::backend::describe(c->options) << "\n";
        out << "instructions " << p->unit.program.size() << ", blocks " << blocks.size() << "\n";
        for (const auto& b : blocks) {
            out << "  block [" << b.begin << ", " << b.end << ")";
            if (b.scheduled) { out << " critical-path " << b.critical_path << " makespan " << b.makespan; total += b.makespan; }
            else out << " (not scheduled: block longer than 256 instructions)";
            out << "\n";
        }
        out << "total makespan " << total << "\n";
        return text_out(c, out.str(), text, length);
    });
}

/* ---- QScript-Module.h --------------------------------------------------- */
qs_module* qs_module_create(void) { try { return new qs_module; } catch (...) { return nullptr; } }
void qs_module_destroy(qs_module* m) { delete m; }
qs_status qs_module_add_source(qs_module* m, const char* text, size_t length, const char* file) {
    if (!m) return QS_INVALID_ARGUMENT;
    if (!text && length) return QS_INVALID_ARGUMENT;
    try {
        qscript::module::Source s{std::string(text ? text : "", length), file ? file : ""};
        if (s.text.size() > 64u * 1024u * 1024u) return QS_ERROR;
        m->module.sources.push_back(std::move(s));
    } catch (...) { return QS_ERROR; }
    return QS_OK;
}
qs_status qs_module_add_file(qs_module* m, const char* path) {
    if (!m || !path) return QS_INVALID_ARGUMENT;
    try {
        std::ifstream in(path, std::ios::binary);
        if (!in) return QS_ERROR;
        std::ostringstream text; text << in.rdbuf();
        if (!in.good() && !in.eof()) return QS_ERROR;
        auto s = text.str();
        return qs_module_add_source(m, s.data(), s.size(), path);
    } catch (...) { return QS_ERROR; }
}
qs_status qs_module_add_library(qs_module* m, const char* name, const char* text, size_t length) {
    if (!m || !name || (!text && length)) return QS_INVALID_ARGUMENT;
    try { m->module.libraries[name] = std::string(text ? text : "", length); }
    catch (...) { return QS_ERROR; }
    return QS_OK;
}
size_t qs_module_sources(const qs_module* m) { return m ? m->module.sources.size() : 0; }
size_t qs_module_library_count(const qs_module* m) { return m ? m->module.libraries.size() : 0; }
void qs_module_clear(qs_module* m) { if (m) { m->module.sources.clear(); m->module.libraries.clear(); } }
qs_status qs_module_compile(qs_context* c, const qs_module* m, qs_program** out) {
    if (out) *out = nullptr;
    return protect(c, [&] {
        if (!m || !out) { c->error = "null module argument"; return QS_INVALID_ARGUMENT; }
        auto compiled = qscript::module::compile_module_unit(m->module, qscript::detail::effective_options(c));
        if (!compiled) { report(c, compiled.diagnostics); return QS_ERROR; }
        qs_program* p = nullptr;
        try { p = new qs_program{std::move(*compiled)}; }
        catch (...) { c->error = "out of memory"; return QS_ERROR; }
        *out = p; return QS_OK;
    });
}

/* ---- QScript-Library.h -------------------------------------------------- */
size_t qs_stdlib_count(void) {
    size_t n = 0; qscript::detail::embedded_libraries(&n); return n;
}
const char* qs_stdlib_name(size_t index) {
    size_t n = 0; const auto* libs = qscript::detail::embedded_libraries(&n);
    return index < n ? libs[index].name : nullptr;
}
const char* qs_stdlib_source(const char* name, size_t* length) {
    if (length) *length = 0;
    if (!name) return nullptr;
    size_t n = 0; const auto* libs = qscript::detail::embedded_libraries(&n);
    for (size_t i = 0; i < n; ++i)
        if (!std::strcmp(libs[i].name, name)) { if (length) *length = std::strlen(libs[i].source); return libs[i].source; }
    return nullptr;
}
qs_status qs_add_library(qs_context* c, const char* name, const char* source, size_t length) {
    if (!c || !name || !*name || (!source && length)) return QS_INVALID_ARGUMENT;
    try { c->options.libraries[name] = std::string(source ? source : "", length); }
    catch (...) { return QS_ERROR; }
    return QS_OK;
}
qs_status qs_add_library_path(qs_context* c, const char* directory) {
    if (!c || !directory) return QS_INVALID_ARGUMENT;
    try { c->options.library_paths.emplace_back(directory); }
    catch (...) { return QS_ERROR; }
    return QS_OK;
}
qs_status qs_set_stdlib(qs_context* c, int enable) {
    if (!c) return QS_INVALID_ARGUMENT;
    c->options.stdlib = enable != 0;
    return QS_OK;
}
qs_status qs_set_library_resolver(qs_context* c, qs_library_resolver fn, void* user) {
    if (!c) return QS_INVALID_ARGUMENT;
    c->resolver = fn; c->resolver_user = user;
    return QS_OK;
}
qs_status qs_clear_libraries(qs_context* c) {
    if (!c) return QS_INVALID_ARGUMENT;
    c->options.libraries.clear(); c->options.library_paths.clear();
    c->options.stdlib = true; c->resolver = nullptr; c->resolver_user = nullptr;
    return QS_OK;
}

/* ---- QScript-Runtime.h -------------------------------------------------- */
qs_status qs_get_limits(const qs_context* c, qs_limits* l) {
    if (!c || !l) return QS_INVALID_ARGUMENT;
    l->max_steps = c->options.toolchain.vm.max_steps;
    l->max_stack = c->options.toolchain.vm.max_stack_values;
    l->max_instructions = c->options.toolchain.il.max_instructions;
    l->max_depth = c->options.max_depth;
    l->max_nodes = c->options.max_nodes;
    l->max_source_bytes = c->options.toolchain.il.max_source_bytes;
    return QS_OK;
}
qs_status qs_set_limits(qs_context* c, const qs_limits* l) {
    if (!c || !l) return QS_INVALID_ARGUMENT;
    if (l->max_depth > 256) return QS_INVALID_ARGUMENT;
    if (l->max_steps) c->options.toolchain.vm.max_steps = l->max_steps;
    if (l->max_stack) c->options.toolchain.vm.max_stack_values = l->max_stack;
    if (l->max_instructions) c->options.toolchain.il.max_instructions = l->max_instructions;
    if (l->max_depth) c->options.max_depth = l->max_depth;
    if (l->max_nodes) c->options.max_nodes = l->max_nodes;
    if (l->max_source_bytes) c->options.toolchain.il.max_source_bytes = l->max_source_bytes;
    return QS_OK;
}
qs_status qs_set_engine(qs_context* c, int engine) {
    if (!c || (engine != QS_ENGINE_INTERPRETER && engine != QS_ENGINE_JIT)) return QS_INVALID_ARGUMENT;
    c->engine = engine;
    return QS_OK;
}
int qs_get_engine(const qs_context* c) { return c ? c->engine : QS_ENGINE_INTERPRETER; }
qs_status qs_run_ex(qs_context* c, const qs_program* p, qs_run_info* info) {
    if (info) *info = qs_run_info{0, 0, 0, QS_RUN_TRAPPED};
    return protect(c, [&] { return execute(c, p, info); });
}
qs_status qs_call(qs_context* c, const qs_program* p, const char* function, const int64_t* args, size_t count, int64_t* result) {
    if (result) *result = 0;
    return protect(c, [&] {
        if (!p || !function || !result || (!args && count)) { c->error = "null call argument"; return QS_INVALID_ARGUMENT; }
        std::vector<std::int64_t> values(args, args + count);
        auto executed = qscript::call(p->unit, function, values, c->options);
        if (executed.value && c->output) for (auto v : executed->output) c->output(v, c->user);
        if (!executed) { report(c, executed.diagnostics); return QS_ERROR; }
        *result = executed->value;
        return QS_OK;
    });
}
size_t qs_program_size(const qs_program* p) { return p ? p->unit.program.size() : 0; }
size_t qs_program_function_count(const qs_program* p) { return p ? p->unit.functions.size() : 0; }
const char* qs_program_function_name(const qs_program* p, size_t i) {
    return p && i < p->unit.functions.size() ? p->unit.functions[i].name.c_str() : nullptr;
}
size_t qs_program_function_arity(const qs_program* p, size_t i) {
    return p && i < p->unit.functions.size() ? p->unit.functions[i].arity : 0;
}
size_t qs_program_library_count(const qs_program* p) { return p ? p->unit.libraries.size() : 0; }
const char* qs_program_library_name(const qs_program* p, size_t i) {
    return p && i < p->unit.libraries.size() ? p->unit.libraries[i].c_str() : nullptr;
}

/* ---- QScript-Diagnostics.h ---------------------------------------------- */
size_t qs_diagnostic_count(const qs_context* c) { return c ? c->diagnostics.size() : 0; }
qs_status qs_diagnostic_get(const qs_context* c, size_t i, qs_diagnostic* d) {
    if (!c || !d || i >= c->diagnostics.size()) return QS_INVALID_ARGUMENT;
    const auto& x = c->diagnostics[i];
    d->stage = static_cast<int>(x.stage);
    d->message = x.message.c_str();
    d->file = x.file.c_str();
    d->line = x.line; d->column = x.column; d->offset = x.offset;
    return QS_OK;
}
const char* qs_stage_name(int stage) {
    static const char* names[] = {"parse", "verify", "optimize", "execute", "assemble", "link", "lower", "rewrite", "io"};
    return stage >= 0 && stage < 9 ? names[stage] : "unknown";
}
qs_status qs_check(qs_context* c, const char* source, size_t length) {
    return protect(c, [&] {
        if (!source && length) { c->error = "null source"; return QS_INVALID_ARGUMENT; }
        auto o = qscript::detail::effective_options(c);
        auto ast = qscript::parse(std::string_view(source ? source : "", length), o);
        if (!ast) { report(c, ast.diagnostics); return QS_ERROR; }
        auto diagnostics = qscript::sema_check(*ast, o);
        if (!diagnostics.empty()) { report(c, diagnostics); return QS_ERROR; }
        return QS_OK;
    });
}
qs_status qs_ast_dump(qs_context* c, const char* source, size_t length, const char** text, size_t* text_length) {
    if (text) *text = nullptr;
    if (text_length) *text_length = 0;
    return protect(c, [&] {
        if ((!source && length) || !text || !text_length) { c->error = "null argument"; return QS_INVALID_ARGUMENT; }
        auto ast = qscript::parse(std::string_view(source ? source : "", length), c->options);
        if (!ast) { report(c, ast.diagnostics); return QS_ERROR; }
        return text_out(c, ast->root.dump(), text, text_length);
    });
}
qs_status qs_match_tree_dump(qs_context* c, const char* source, size_t length, const char** text, size_t* text_length) {
    if (text) *text = nullptr;
    if (text_length) *text_length = 0;
    return protect(c, [&] {
        if ((!source && length) || !text || !text_length) { c->error = "null argument"; return QS_INVALID_ARGUMENT; }
        auto parsed = qscript::language_grammar().parse(std::string_view(source ? source : "", length));
        if (!parsed) {
            report(c, {{metachain::Stage::Parse, parsed.error, {}, parsed.line, parsed.column, parsed.offset}});
            return QS_ERROR;
        }
        return text_out(c, parsed.match->dump(), text, text_length);
    });
}
} // extern C
