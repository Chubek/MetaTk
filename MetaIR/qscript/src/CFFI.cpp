#include "qscript/src/FFIInternal.h"
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#ifdef QSCRIPT_HAS_LIBFFI
#include <ffi.h>
#endif

namespace qscript::detail {
struct NativeFunction {
    std::shared_ptr<SharedLibrary> library;
    void *address=nullptr;
    int result_type=QS_CFFI_VOID;
    std::vector<int> types;
#ifdef QSCRIPT_HAS_LIBFFI
    ffi_cif cif{};
    std::vector<ffi_type*> ffi_types;
#endif
};
#ifdef QSCRIPT_HAS_LIBFFI
ffi_type *native_type(int type) {
    switch (type) {
    case QS_CFFI_VOID: return &ffi_type_void;
    case QS_CFFI_I8: return &ffi_type_sint8;
    case QS_CFFI_U8: return &ffi_type_uint8;
    case QS_CFFI_I16: return &ffi_type_sint16;
    case QS_CFFI_U16: return &ffi_type_uint16;
    case QS_CFFI_I32: return &ffi_type_sint32;
    case QS_CFFI_U32: return &ffi_type_uint32;
    case QS_CFFI_I64: return &ffi_type_sint64;
    case QS_CFFI_U64: return &ffi_type_uint64;
    case QS_CFFI_FLOAT: return &ffi_type_float;
    case QS_CFFI_DOUBLE: return &ffi_type_double;
    case QS_CFFI_POINTER: return &ffi_type_pointer;
    default: throw std::invalid_argument("unknown cffi type");
    }
}
union Slot {
    ffi_arg word;
    ffi_sarg signed_word;
    std::int8_t i8; std::uint8_t u8;
    std::int16_t i16; std::uint16_t u16;
    std::int32_t i32; std::uint32_t u32;
    std::int64_t i64; std::uint64_t u64;
    float f32; double f64; void *pointer;
    Slot():word(0) {}
};
template<class T, class V> T checked_integer(V value) {
    if (value<std::numeric_limits<T>::min() || value>std::numeric_limits<T>::max())
        throw std::invalid_argument("cffi integer argument out of range");
    return static_cast<T>(value);
}
#endif
std::shared_ptr<NativeFunction> make_native(std::shared_ptr<SharedLibrary> lib,
    const char *symbol,int result_type,std::span<const int> types) {
#ifdef QSCRIPT_HAS_LIBFFI
    if (types.size()>16) throw std::invalid_argument("cffi supports at most 16 arguments");
    auto f=std::make_shared<NativeFunction>();
    f->library=std::move(lib); f->result_type=result_type;
    auto *result=native_type(result_type);
    if (!types.empty()) f->types.assign(types.begin(),types.end());
    for (auto t:types) {
        if (t==QS_CFFI_VOID) throw std::invalid_argument("void is not an argument type");
        f->ffi_types.push_back(native_type(t));
    }
    f->address=library_symbol(*f->library,symbol);
    if (!f->address) throw std::runtime_error("null native function address");
    if (ffi_prep_cif(&f->cif,FFI_DEFAULT_ABI,static_cast<unsigned>(types.size()),result,f->ffi_types.data())!=FFI_OK)
        throw std::runtime_error("libffi rejected signature");
    return f;
#else
    (void)lib; (void)symbol; (void)result_type; (void)types;
    throw std::runtime_error("QScript was built without libffi");
#endif
}
qs_cffi_value invoke_native(NativeFunction& f,std::span<const qs_cffi_value> values) {
#ifdef QSCRIPT_HAS_LIBFFI
    if (values.size()!=f.types.size()) throw std::invalid_argument("cffi argument count mismatch");
    std::array<Slot,16> storage{};
    std::array<void*,16> args{};
    for (std::size_t n=0;n<values.size();++n) {
        const auto& v=values[n]; auto& s=storage[n]; args[n]=&s;
        if (v.type!=f.types[n]) throw std::invalid_argument("cffi argument type mismatch");
        switch (v.type) {
        case QS_CFFI_I8: s.i8=checked_integer<std::int8_t>(v.data.i64); break;
        case QS_CFFI_U8: s.u8=checked_integer<std::uint8_t>(v.data.u64); break;
        case QS_CFFI_I16: s.i16=checked_integer<std::int16_t>(v.data.i64); break;
        case QS_CFFI_U16: s.u16=checked_integer<std::uint16_t>(v.data.u64); break;
        case QS_CFFI_I32: s.i32=checked_integer<std::int32_t>(v.data.i64); break;
        case QS_CFFI_U32: s.u32=checked_integer<std::uint32_t>(v.data.u64); break;
        case QS_CFFI_I64: s.i64=v.data.i64; break;
        case QS_CFFI_U64: s.u64=v.data.u64; break;
        case QS_CFFI_FLOAT: s.f32=static_cast<float>(v.data.f64); break;
        case QS_CFFI_DOUBLE: s.f64=v.data.f64; break;
        case QS_CFFI_POINTER: s.pointer=v.data.pointer; break;
        default: throw std::invalid_argument("invalid cffi argument type");
        }
    }
    Slot result;
    ffi_call(&f.cif,FFI_FN(f.address),&result,args.data());
    qs_cffi_value out{}; out.type=f.result_type;
    switch (f.result_type) {
    // libffi widens sub-word integral return values to ffi_arg.
    case QS_CFFI_I8: out.data.i64=static_cast<std::int8_t>(result.signed_word); break;
    case QS_CFFI_U8: out.data.u64=static_cast<std::uint8_t>(result.word); break;
    case QS_CFFI_I16: out.data.i64=static_cast<std::int16_t>(result.signed_word); break;
    case QS_CFFI_U16: out.data.u64=static_cast<std::uint16_t>(result.word); break;
    case QS_CFFI_I32: out.data.i64=sizeof(ffi_arg)>4 ? static_cast<std::int32_t>(result.signed_word) : result.i32; break;
    case QS_CFFI_U32: out.data.u64=sizeof(ffi_arg)>4 ? static_cast<std::uint32_t>(result.word) : result.u32; break;
    case QS_CFFI_I64: out.data.i64=result.i64; break;
    case QS_CFFI_U64: out.data.u64=result.u64; break;
    case QS_CFFI_FLOAT: out.data.f64=result.f32; break;
    case QS_CFFI_DOUBLE: out.data.f64=result.f64; break;
    case QS_CFFI_POINTER: out.data.pointer=result.pointer; break;
    default: break;
    }
    return out;
#else
    (void)f; (void)values;
    throw std::runtime_error("QScript was built without libffi");
#endif
}
std::int64_t invoke_integer(NativeFunction& f,std::span<const std::int64_t> args) {
    if (args.size()!=f.types.size()) throw std::invalid_argument("cffi argument count mismatch");
    static_assert(sizeof(double)==8 && sizeof(float)==4 && sizeof(std::uintptr_t)<=8);
    std::array<qs_cffi_value,16> values{};
    for (std::size_t n=0;n<args.size();++n) {
        auto& v=values[n]; v.type=f.types[n];
        switch (v.type) {
        case QS_CFFI_FLOAT: v.data.f64=std::bit_cast<float>(static_cast<std::uint32_t>(args[n])); break;
        case QS_CFFI_DOUBLE: v.data.f64=std::bit_cast<double>(args[n]); break;
        case QS_CFFI_POINTER: v.data.pointer=reinterpret_cast<void*>(static_cast<std::uintptr_t>(args[n])); break;
        case QS_CFFI_U8: case QS_CFFI_U16: case QS_CFFI_U32: case QS_CFFI_U64:
            v.data.u64=static_cast<std::uint64_t>(args[n]); break;
        default: v.data.i64=args[n]; break;
        }
    }
    const auto v=invoke_native(f,{values.data(),args.size()});
    switch (v.type) {
    case QS_CFFI_VOID: return 0;
    case QS_CFFI_FLOAT: return std::bit_cast<std::uint32_t>(static_cast<float>(v.data.f64));
    case QS_CFFI_DOUBLE: return std::bit_cast<std::int64_t>(v.data.f64);
    case QS_CFFI_POINTER: return std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(v.data.pointer)));
    case QS_CFFI_U8: case QS_CFFI_U16: case QS_CFFI_U32: case QS_CFFI_U64: return std::bit_cast<std::int64_t>(v.data.u64);
    default: return v.data.i64;
    }
}

// Script-facing resources have monotonically increasing, typed handles.
// A closed library remains mapped while any native function retains it.
struct CFFIState {
    std::int64_t next=1;
    std::size_t bytes=0;
    static constexpr std::size_t max_bytes=64u*1024u*1024u, max_handles=4096;
    std::map<std::int64_t,std::vector<unsigned char>> buffers;
    std::map<std::int64_t,std::shared_ptr<SharedLibrary>> libraries;
    std::map<std::int64_t,std::shared_ptr<NativeFunction>> functions;
    void release(std::int64_t handle) {
        if (auto it=buffers.find(handle); it!=buffers.end()) { bytes-=it->second.size(); buffers.erase(it); }
        else if (!libraries.erase(handle) && !functions.erase(handle)) throw std::invalid_argument("invalid or released cffi handle");
    }
    std::int64_t id() {
        if (buffers.size()+libraries.size()+functions.size()>=max_handles || next==INT64_MAX)
            throw std::runtime_error("cffi handle limit exceeded");
        return next++;
    }
    template<class Map> static auto& get(Map& map,std::int64_t id) {
        auto it=map.find(id);
        if (it==map.end()) throw std::invalid_argument("invalid or released cffi handle");
        return it->second;
    }
    std::string string(std::int64_t handle) {
        auto& b=get(buffers,handle);
        auto end=std::find(b.begin(),b.end(),0);
        if (end==b.end()) throw std::invalid_argument("cffi string is not NUL terminated");
        return std::string(b.begin(),end);
    }
    static std::size_t offset(std::int64_t offset,std::size_t width,std::size_t size) {
        if (offset<0 || static_cast<std::uint64_t>(offset)>size || width>size-static_cast<std::size_t>(offset))
            throw std::out_of_range("cffi buffer access out of bounds");
        return static_cast<std::size_t>(offset);
    }
};
}
struct qs_cffi_function { std::shared_ptr<qscript::detail::NativeFunction> function; };
using namespace qscript::detail;
extern "C" {
int qs_cffi_available(void) {
#ifdef QSCRIPT_HAS_LIBFFI
    return 1;
#else
    return 0;
#endif
}
qs_cffi_function *qs_cffi_function_create(qs_context *c,const qs_shared_library *lib,
    const char *symbol,int result_type,const int *types,size_t count) {
    qs_cffi_function *result=nullptr;
    protect(c,[&] {
        if (!lib || !symbol || (!types && count) || count>16) { c->error="invalid cffi function argument"; return QS_INVALID_ARGUMENT; }
        result=new qs_cffi_function{make_native(lib->library,symbol,result_type,{types,count})};
        return QS_OK;
    });
    return result;
}
void qs_cffi_function_destroy(qs_cffi_function *f) { delete f; }
qs_status qs_cffi_call(qs_context *c,const qs_cffi_function *f,const qs_cffi_value *args,size_t count,qs_cffi_value *result) {
    return protect(c,[&] {
        if (!f || !result || (!args && count) || count>16) return QS_INVALID_ARGUMENT;
        auto value=invoke_native(*f->function,{args,count});
        *result=value; return QS_OK;
    });
}
qs_status qs_cffi_bind(qs_context *c,const char *name,const qs_cffi_function *f) {
    return protect(c,[&] {
        if (!name || !f) return QS_INVALID_ARGUMENT;
        register_host(c,name,f->function->types.size(),[fn=f->function](auto args) { return invoke_integer(*fn,args); });
        return QS_OK;
    });
}
qs_status qs_cffi_enable(qs_context *c) {
    return protect(c,[&] {
        if (!qs_cffi_available()) { c->error="QScript was built without libffi"; return QS_ERROR; }
        if (c->cffi) return QS_OK;
        auto state=std::make_shared<CFFIState>();
        // Install transactionally; a conflicting user binding leaves options intact.
        qs_context staging; staging.options.hosts=c->options.hosts;
        auto add=[&](const char *name,size_t arity,auto fn) {
            register_host(&staging,name,arity,[state,fn](auto a) { return fn(*state,a); });
        };
        add("_cffi_alloc",1,[](auto& s,auto a)->std::int64_t {
            if (a[0]<0 || static_cast<std::uint64_t>(a[0])>s.max_bytes-s.bytes) throw std::runtime_error("cffi buffer limit exceeded");
            const auto id=s.id(); auto size=static_cast<std::size_t>(a[0]);
            s.buffers.emplace(id,std::vector<unsigned char>(size)); s.bytes+=size; return id;
        });
        add("_cffi_release",1,[](auto& s,auto a)->std::int64_t {
            s.release(a[0]);
            return 0;
        });
        add("_cffi_size",1,[](auto& s,auto a)->std::int64_t { return s.get(s.buffers,a[0]).size(); });
        add("_cffi_address",1,[](auto& s,auto a)->std::int64_t {
            return std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(s.get(s.buffers,a[0]).data())));
        });
        add("_cffi_read",3,[](auto& s,auto a)->std::int64_t {
            auto& b=s.get(s.buffers,a[0]);
            if (a[2]!=1 && a[2]!=2 && a[2]!=4 && a[2]!=8) throw std::invalid_argument("cffi width must be 1, 2, 4 or 8");
            auto offset=s.offset(a[1],static_cast<size_t>(a[2]),b.size());
            std::uint64_t value=0;
            for (int n=0;n<a[2];++n) value|=std::uint64_t(b[offset+n])<<(8*n);
            return std::bit_cast<std::int64_t>(value);
        });
        add("_cffi_write",4,[](auto& s,auto a)->std::int64_t {
            auto& b=s.get(s.buffers,a[0]);
            if (a[2]!=1 && a[2]!=2 && a[2]!=4 && a[2]!=8) throw std::invalid_argument("cffi width must be 1, 2, 4 or 8");
            auto offset=s.offset(a[1],static_cast<size_t>(a[2]),b.size());
            for (int n=0;n<a[2];++n) b[offset+n]=static_cast<unsigned char>(static_cast<std::uint64_t>(a[3])>>(8*n));
            return a[3];
        });
        add("_cffi_open",1,[](auto& s,auto a)->std::int64_t {
            auto path=a[0] ? s.string(a[0]) : std::string();
            auto lib=open_library(a[0] ? path.c_str() : nullptr);
            auto id=s.id(); s.libraries.emplace(id,std::move(lib)); return id;
        });
        add("_cffi_symbol",2,[](auto& s,auto a)->std::int64_t {
            auto name=s.string(a[1]);
            auto p=library_symbol(*s.get(s.libraries,a[0]),name.c_str());
            return std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p)));
        });
        add("_cffi_function",5,[](auto& s,auto a)->std::int64_t {
            if (a[2]<0 || a[2]>QS_CFFI_POINTER || a[4]<0 || a[4]>16) throw std::invalid_argument("invalid cffi signature");
            auto name=s.string(a[1]); std::vector<int> types;
            auto packed=static_cast<std::uint64_t>(a[3]);
            for (int n=0;n<a[4];++n) { types.push_back(packed&15); packed>>=4; }
            if (packed) throw std::invalid_argument("extra cffi argument types");
            auto f=make_native(s.get(s.libraries,a[0]),name.c_str(),static_cast<int>(a[2]),types);
            auto id=s.id(); s.functions.emplace(id,std::move(f)); return id;
        });
        for (unsigned n=0;n<=8;++n) {
            auto name="_cffi_call"+std::to_string(n);
            add(name.c_str(),n+1,[](auto& s,auto a)->std::int64_t {
                auto f=s.get(s.functions,a[0]); return invoke_integer(*f,a.subspan(1));
            });
        }
        c->options.hosts.swap(staging.options.hosts); c->cffi=std::move(state);
        return QS_OK;
    });
}
qs_status qs_cffi_buffer_create(qs_context *c,const void *data,size_t length,int64_t *handle) {
    if (handle) *handle=0;
    return protect(c,[&] {
        if (!handle) return QS_INVALID_ARGUMENT;
        if (!c->cffi) { c->error="enable cffi before creating buffers"; return QS_ERROR; }
        auto& s=*c->cffi;
        if (length>s.max_bytes-s.bytes) { c->error="cffi buffer limit exceeded"; return QS_ERROR; }
        auto id=s.id(); std::vector<unsigned char> bytes(length);
        if (data && length) std::memcpy(bytes.data(),data,length);
        s.buffers.emplace(id,std::move(bytes)); s.bytes+=length;
        *handle=id; return QS_OK;
    });
}
qs_status qs_cffi_buffer_read(qs_context *c,int64_t handle,size_t offset,void *out,size_t length) {
    return protect(c,[&] {
        if (!out && length) return QS_INVALID_ARGUMENT;
        if (!c->cffi) { c->error="cffi is not enabled"; return QS_ERROR; }
        const auto& b=CFFIState::get(c->cffi->buffers,handle);
        if (offset>b.size() || length>b.size()-offset) { c->error="cffi buffer access out of bounds"; return QS_INVALID_ARGUMENT; }
        if (length) std::memmove(out,b.data()+offset,length);
        return QS_OK;
    });
}
qs_status qs_cffi_buffer_write(qs_context *c,int64_t handle,size_t offset,const void *data,size_t length) {
    return protect(c,[&] {
        if (!data && length) return QS_INVALID_ARGUMENT;
        if (!c->cffi) { c->error="cffi is not enabled"; return QS_ERROR; }
        auto& b=CFFIState::get(c->cffi->buffers,handle);
        if (offset>b.size() || length>b.size()-offset) { c->error="cffi buffer access out of bounds"; return QS_INVALID_ARGUMENT; }
        if (length) std::memmove(b.data()+offset,data,length);
        return QS_OK;
    });
}
qs_status qs_cffi_release(qs_context *c,int64_t handle) {
    return protect(c,[&] {
        if (!c->cffi) { c->error="cffi is not enabled"; return QS_ERROR; }
        c->cffi->release(handle); return QS_OK;
    });
}
}
