#include "MetaAS.h"
#include "MetaAS.hpp"
#include <exception>

struct mas_context {
    std::optional<metaas::Assembler> assembler;
    metaas::Options options;
    std::string error,text;
    std::vector<std::uint8_t> bytes;
};
struct mas_image { metaas::Image image; };
namespace {
template<class F> int protect(mas_context *c,F&& f) noexcept {
    if (!c) return MAS_INVALID_ARGUMENT;
    c->error.clear(); c->text.clear(); c->bytes.clear();
    try { return f(); }
    catch (const std::exception& e) { try { c->error=e.what(); } catch (...) {} }
    catch (...) { try { c->error="unexpected MetaAS failure"; } catch (...) {} }
    return MAS_ERROR;
}
int report(mas_context *c,const std::vector<metaas::Diagnostic>& diagnostics) {
    for (const auto& d:diagnostics) {
        if (!c->error.empty()) c->error+='\n';
        if (!d.file.empty()) c->error+=d.file+":";
        if (d.line) c->error+=std::to_string(d.line)+":"+std::to_string(d.column)+": ";
        c->error+=d.message;
    }
    return MAS_ERROR;
}
}
extern "C" {
mas_context *mas_create(void) { try { return new mas_context; } catch (...) { return nullptr; } }
void mas_destroy(mas_context *c) { delete c; }
const char *mas_error(const mas_context *c) { return c?c->error.c_str():"null MetaAS context"; }
int mas_load(mas_context *c,const char *path) {
    return protect(c,[&]() -> int {
        if (!path) return MAS_INVALID_ARGUMENT;
        auto loaded=metaas::Assembler::load(path);
        if (!loaded) return report(c,loaded.diagnostics);
        c->assembler=std::move(*loaded.value); return MAS_OK;
    });
}
int mas_set_target(mas_context *c,size_t target) {
    return protect(c,[&] { c->options.target=target; return MAS_OK; });
}
int mas_set_origin(mas_context *c,uint64_t origin) {
    return protect(c,[&] { if (origin>INT64_MAX) return MAS_INVALID_ARGUMENT; c->options.origin=origin; return MAS_OK; });
}
int mas_set_limits(mas_context *c,size_t source,size_t output) {
    return protect(c,[&] { c->options.max_source_bytes=source; c->options.max_output_bytes=output; return MAS_OK; });
}
int mas_bind(mas_context *c,const char *encoder,const char *const *parameters,size_t count) {
    return protect(c,[&] {
        if (!encoder || (!parameters && count) || count>64) return MAS_INVALID_ARGUMENT;
        std::vector<std::string> names;
        for (size_t n=0;n<count;++n) { if (!parameters[n]) return MAS_INVALID_ARGUMENT; names.emplace_back(parameters[n]); }
        c->options.bindings[encoder]=std::move(names); return MAS_OK;
    });
}
int mas_assemble(mas_context *c,const char *source,size_t length,const char *file,mas_image **out) {
    if (out) *out=nullptr;
    return protect(c,[&]() -> int {
        if ((!source && length) || !out) return MAS_INVALID_ARGUMENT;
        if (!c->assembler) { c->error="load an architecture before assembling"; return MAS_ERROR; }
        auto result=c->assembler->assemble(std::string_view(source?source:"",length),c->options,file?file:"<assembly>");
        if (!result) return report(c,result.diagnostics);
        *out=new mas_image{std::move(*result.value)}; return MAS_OK;
    });
}
void mas_image_destroy(mas_image *image) { delete image; }
size_t mas_image_section_count(const mas_image *image) { return image?image->image.sections.size():0; }
int mas_image_section(const mas_image *image,size_t index,const char **name,const unsigned char **data,size_t *size,uint64_t *address) {
    if (name) *name=nullptr;
    if (data) *data=nullptr;
    if (size) *size=0;
    if (address) *address=0;
    if (!image || index>=image->image.sections.size() || !name || !data || !size || !address) return MAS_INVALID_ARGUMENT;
    const auto& s=image->image.sections[index];
    *name=s.name.c_str(); *data=s.bytes.data(); *size=s.bytes.size(); *address=s.address; return MAS_OK;
}
int mas_image_binary(mas_context *c,const mas_image *image,const unsigned char **data,size_t *size) {
    if (data) *data=nullptr;
    if (size) *size=0;
    return protect(c,[&]() -> int {
        if (!image || !data || !size) return MAS_INVALID_ARGUMENT;
        auto binary=image->image.binary(c->options.max_output_bytes);
        if (!binary) return report(c,binary.diagnostics);
        c->bytes=std::move(*binary.value); *data=c->bytes.data(); *size=c->bytes.size(); return MAS_OK;
    });
}
int mas_image_json(mas_context *c,const mas_image *image,const char **data,size_t *size) {
    if (data) *data=nullptr;
    if (size) *size=0;
    return protect(c,[&] {
        if (!image || !data || !size) return MAS_INVALID_ARGUMENT;
        c->text=image->image.json(); *data=c->text.c_str(); *size=c->text.size(); return MAS_OK;
    });
}
}
