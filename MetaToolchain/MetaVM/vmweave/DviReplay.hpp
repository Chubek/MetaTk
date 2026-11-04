#pragma once
#include "Builder.hpp"
#include "../VMLibrary/DVI/DviMachine.hpp"
#include <cstdio>
#include <functional>
#include <limits>
#include <new>
#include <utility>

namespace vmweave {
// The module contains indices, not event payloads. Keep this snapshot alive at
// a stable address until all bound VMs have been destroyed. Sink effects are
// not transactional; synchronize a sink shared by concurrently running VMs.
class DviReplay {
    using Dvi=metavm::machines::DviMachine;
    Dvi::Result document_;
    std::function<void(const Dvi::Event&)> sink_;
    static mvm_status fail(mvm_error*error,mvm_status status,const char*message) noexcept {
        mvm_error_clear(error);
        if(error){error->status=status;std::snprintf(error->message,sizeof error->message,"%s",message);}
        return status;
    }
    static mvm_status event(mvm_vm*,const mvm_value*args,size_t count,
                            mvm_value*out,void*context,mvm_error*error) noexcept {
        if(!context||!args||count!=1||!out||args[0].kind!=MVM_INT)
            return fail(error,MVM_INVALID,"DVI replay requires an integer event index");
        auto&self=*static_cast<DviReplay*>(context);
        auto index=args[0].as.integer;
        if(index<0||uint64_t(index)>=self.document_.events.size())
            return fail(error,MVM_INVALID,"DVI replay event index out of range");
        try {
            self.sink_(self.document_.events[size_t(index)]);
            *out=mvm_nil();return MVM_OK;
        } catch(const std::bad_alloc&) {
            return fail(error,MVM_OOM,"DVI replay sink allocation failed");
        } catch(const std::exception&e) {
            return fail(error,MVM_HOST_ERROR,e.what());
        } catch(...) {
            return fail(error,MVM_HOST_ERROR,"DVI replay sink threw unknown exception");
        }
    }
public:
    explicit DviReplay(Dvi::Result document,std::function<void(const Dvi::Event&)> sink)
        :document_(std::move(document)),sink_(std::move(sink)) {
        if(!sink_)throw std::runtime_error("DVI replay requires a sink");
        if(document_.events.size()>uint64_t(std::numeric_limits<int64_t>::max()))
            throw std::runtime_error("DVI replay event count exceeds i64");
    }
    DviReplay(const DviReplay&)=delete;
    DviReplay&operator=(const DviReplay&)=delete;
    DviReplay(DviReplay&&)=delete;
    DviReplay&operator=(DviReplay&&)=delete;
    mvm_status bind(mvm_vm*vm,mvm_error*error=nullptr) {
        return mvm_register_native(vm,"dvi_event",1,&event,this,error);
    }
    metavm::Module module(size_t max_source_bytes=16*1024*1024) const {
        Builder out(max_source_bytes);
        out.emit(".import dvi_event 1");out.function("main",0,2);
        for(size_t i=0;i<document_.events.size();++i) {
            out.constant(0,int64_t(i));out.emit("host r1 dvi_event r0");
        }
        out.constant(0,int64_t(document_.events.size()));out.ret(0);out.end();
        return out.finish();
    }
};
}
