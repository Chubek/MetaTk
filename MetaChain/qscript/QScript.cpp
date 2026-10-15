#include "QScript.h"
#include <new>

struct qs_context {
    qscript::Options options;
    std::string error;
    std::vector<std::uint8_t> buffer;
    qs_output_fn output=nullptr;
    void* user=nullptr;
};
struct qs_program { metail::Program ir; };
namespace {
template<class Function> qs_status protect(qs_context* c,Function fn) noexcept {
    if (!c) return QS_INVALID_ARGUMENT;
    c->error.clear(); c->buffer.clear();
    try { return fn(); }
    catch (const std::exception& e) { try { c->error=e.what(); } catch (...) {} }
    catch (...) { try { c->error="unexpected QScript failure"; } catch (...) {} }
    return QS_ERROR;
}
void errors(qs_context* c,const std::vector<qscript::Diagnostic>& diagnostics) {
    for (const auto& d:diagnostics) { if (!c->error.empty()) c->error+='\n'; c->error+=metachain::format(d); }
}
}
extern "C" {
qs_context* qs_create(void) { try { return new qs_context; } catch (...) { return nullptr; } }
void qs_destroy(qs_context* c) { delete c; }
const char* qs_error(const qs_context* c) { return c?c->error.c_str():"null QScript context"; }
void qs_set_output(qs_context* c,qs_output_fn fn,void* user) { if (c) { c->output=fn; c->user=user; } }
void qs_set_step_limit(qs_context* c,size_t limit) { if (c) c->options.toolchain.vm.max_steps=limit; }
qs_program* qs_compile(qs_context* c,const char* source,size_t length) {
    qs_program* p=nullptr;
    protect(c,[&] {
        if (!source&&length) { c->error="null source"; return QS_INVALID_ARGUMENT; }
        auto compiled=qscript::compile(std::string_view(source?source:"",length),c->options);
        if (!compiled) { errors(c,compiled.diagnostics); return QS_ERROR; }
        p=new qs_program{std::move(*compiled)}; return QS_OK;
    });
    return p;
}
void qs_program_destroy(qs_program* p) { delete p; }
qs_status qs_run(qs_context* c,const qs_program* p,int64_t* result) {
    if (result) *result=0;
    return protect(c,[&] {
        if (!p||!result) { c->error="null program or result"; return QS_INVALID_ARGUMENT; }
        auto executed=metachain::Toolchain(c->options.toolchain).run(p->ir);
        if (executed.value&&c->output) for (auto value:executed->output) c->output(value,c->user);
        if (!executed) { errors(c,executed.diagnostics); return QS_ERROR; }
        *result=executed->value; return QS_OK;
    });
}
qs_status qs_emit(qs_context* c,const qs_program* p,int format,const unsigned char** data,size_t* length) {
    if (data) *data=nullptr;
    if (length) *length=0;
    return protect(c,[&] {
        if (!p||!data||!length) { c->error="null emit argument"; return QS_INVALID_ARGUMENT; }
        if (format==QS_IL||format==QS_NATIVE) {
            auto text=format==QS_IL?qscript::emit(p->ir,c->options):qscript::select(p->ir,c->options);
            if (!text) { errors(c,text.diagnostics); return QS_ERROR; }
            c->buffer.assign(text->begin(),text->end());
        } else if (format==QS_BYTECODE) c->buffer=metail::encode(p->ir);
        else if (format==QS_OBJECT||format==QS_EXECUTABLE) {
            auto source=qscript::select(p->ir,c->options);
            if (!source) { errors(c,source.diagnostics); return QS_ERROR; }
            auto object=qscript::assemble(*source,true,c->options);
            if (!object) { errors(c,object.diagnostics); return QS_ERROR; }
            auto bytes=format==QS_OBJECT?std::move(object):qscript::link({*object},c->options);
            if (!bytes) { errors(c,bytes.diagnostics); return QS_ERROR; }
            c->buffer=std::move(*bytes);
        } else { c->error="invalid output format"; return QS_INVALID_ARGUMENT; }
        *data=c->buffer.data(); *length=c->buffer.size(); return QS_OK;
    });
}
} // extern C
