#include "qscript/src/FFIInternal.h"
#include "qscript/src/Runtime.h"
#include <atomic>
#include <bit>
#include <cstring>
#include <limits>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

namespace qscript::detail {
void register_host(qs_context *c, std::string name, std::size_t arity,
                   std::function<std::int64_t(std::span<const std::int64_t>)> fn) {
    if (name.empty() || arity > 16 || !fn) throw std::invalid_argument("invalid host binding");
    for (std::size_t start=0;;) {
        auto end=name.find("::", start);
        auto part=name.substr(start, end==std::string::npos ? end : end-start);
        if (part.empty() || !metachain::text::alpha(part.front())) throw std::invalid_argument("invalid host name");
        for (char ch:part) if (!metachain::text::alpha(ch) && !metachain::text::digit(ch)) throw std::invalid_argument("invalid host name");
        if (end==std::string::npos) break;
        start=end+2;
    }
    if (c->options.hosts.count(name)) throw std::invalid_argument("host function already registered: " + name);
    static std::atomic<std::uint64_t> next{1};
    const auto id=next.fetch_add(1);
    if (id>INT64_MAX) throw std::runtime_error("host binding ID limit exceeded");
    c->options.hosts.emplace(std::move(name), HostFunction{static_cast<std::int64_t>(id),arity,std::move(fn)});
}
Options runtime_options(Options o, Session *session) {
    if (o.hosts.empty() && !session) return o;
    std::map<std::int64_t,HostFunction> bindings;
    for (const auto& [name, f]:o.hosts) {
        (void)name;
        if (f.id<0 || f.arity>16 || !f.invoke || !bindings.emplace(f.id,f).second) throw std::invalid_argument("invalid host bindings");
    }
    o.toolchain.vm.host_call=[bindings=std::move(bindings), session](std::int64_t id, std::span<const std::int64_t> regs) -> std::int64_t {
        if (session && is_builtin_id(id)) return session->dispatch(id, regs);
        const auto f=bindings.find(id);
        if (f==bindings.end()) throw std::runtime_error("unbound host function");
        return f->second.invoke(regs.first(f->second.arity));
    };
    return o;
}
SharedLibrary::~SharedLibrary() {
#if defined(_WIN32)
    if (handle) FreeLibrary(static_cast<HMODULE>(handle));
#elif defined(__unix__) || defined(__APPLE__)
    if (handle) dlclose(handle);
#endif
}
std::shared_ptr<SharedLibrary> open_library(const char *path) {
    auto lib=std::make_shared<SharedLibrary>();
#if defined(_WIN32)
    if (!path) throw std::invalid_argument("a library path is required on Windows");
    lib->handle=LoadLibraryA(path);
    if (!lib->handle) throw std::runtime_error("cannot load shared library");
#elif defined(__unix__) || defined(__APPLE__)
    lib->handle=dlopen(path,RTLD_NOW|RTLD_LOCAL);
    if (!lib->handle) { const char *e=dlerror(); throw std::runtime_error(e ? e : "cannot load shared library"); }
#else
    (void)path;
    throw std::runtime_error("shared library loading is unavailable on this platform");
#endif
    return lib;
}
void *library_symbol(const SharedLibrary& lib, const char *name) {
    if (!name || !*name) throw std::invalid_argument("empty symbol name");
#if defined(_WIN32)
    auto address=reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(lib.handle),name));
    if (!address) throw std::runtime_error("shared library symbol not found");
    return address;
#elif defined(__unix__) || defined(__APPLE__)
    dlerror();
    void *address=dlsym(lib.handle,name);
    if (const char *e=dlerror()) throw std::runtime_error(e);
    return address;
#else
    (void)lib;
    throw std::runtime_error("shared library symbols are unavailable on this platform");
#endif
}
}
using namespace qscript::detail;
extern "C" {
qs_status qs_register_function(qs_context *c,const char *name,size_t arity,qs_host_fn fn,void *user) {
    return protect(c,[&] {
        if (!name || !fn || arity>16) { c->error="invalid host binding"; return QS_INVALID_ARGUMENT; }
        register_host(c,name,arity,[fn,user](std::span<const std::int64_t> args) {
            std::int64_t result=0;
            if (fn(args.data(),args.size(),&result,user)!=QS_OK) throw std::runtime_error("host callback failed");
            return result;
        });
        return QS_OK;
    });
}
qs_status qs_unregister_function(qs_context *c,const char *name) {
    return protect(c,[&] {
        if (!name) return QS_INVALID_ARGUMENT;
        if (c->cffi && std::string_view(name).starts_with("_cffi_")) {
            c->error="cannot unregister an active cffi primitive"; return QS_INVALID_ARGUMENT;
        }
        if (!c->options.hosts.erase(name)) { c->error="host function not registered"; return QS_ERROR; }
        return QS_OK;
    });
}
size_t qs_registered_function_count(const qs_context *c) { return c ? c->options.hosts.size() : 0; }
qs_shared_library *qs_shared_library_open(qs_context *c,const char *path) {
    qs_shared_library *result=nullptr;
    protect(c,[&] { result=new qs_shared_library{open_library(path)}; return QS_OK; });
    return result;
}
void qs_shared_library_close(qs_shared_library *lib) { delete lib; }
qs_status qs_shared_library_symbol(qs_context *c,const qs_shared_library *lib,const char *name,void **address) {
    if (address) *address=nullptr;
    return protect(c,[&] {
        if (!lib || !name || !address) return QS_INVALID_ARGUMENT;
        *address=library_symbol(*lib->library,name); return QS_OK;
    });
}
qs_status qs_values_encode(const int64_t *values,size_t count,unsigned char *out,size_t capacity,size_t *required) {
    if (required) *required=0;
    if (!required || (!values && count) || count>UINT32_MAX || count>(SIZE_MAX-8)/8) return QS_INVALID_ARGUMENT;
    *required=8+count*8;
    if (!out) return capacity==0 ? QS_OK : QS_INVALID_ARGUMENT;
    if (capacity<*required) return QS_INVALID_ARGUMENT;
    std::memcpy(out,"QSV1",4);
    for (unsigned n=0;n<4;++n) out[4+n]=static_cast<unsigned char>(count>>(8*n));
    for (size_t i=0;i<count;++i) for (unsigned n=0;n<8;++n)
        out[8+i*8+n]=static_cast<unsigned char>(std::bit_cast<std::uint64_t>(values[i])>>(8*n));
    return QS_OK;
}
qs_status qs_values_decode(const unsigned char *data,size_t length,int64_t *out,size_t capacity,size_t *required) {
    if (required) *required=0;
    if (!required || !data || length<8 || std::memcmp(data,"QSV1",4)) return QS_INVALID_ARGUMENT;
    std::uint64_t count=0;
    for (unsigned n=0;n<4;++n) count|=std::uint64_t(data[4+n])<<(8*n);
    if (count>(SIZE_MAX-8)/8 || length!=8+count*8) return QS_INVALID_ARGUMENT;
    *required=static_cast<size_t>(count);
    if (!out) return capacity==0 ? QS_OK : QS_INVALID_ARGUMENT;
    if (capacity<count) return QS_INVALID_ARGUMENT;
    for (size_t i=0;i<count;++i) {
        std::uint64_t bits=0;
        for (unsigned n=0;n<8;++n) bits|=std::uint64_t(data[8+i*8+n])<<(8*n);
        out[i]=std::bit_cast<std::int64_t>(bits);
    }
    return QS_OK;
}
}
