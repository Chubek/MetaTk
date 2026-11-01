#include "Graphics.hpp"
#include <algorithm>
#include <stdexcept>
namespace metavm { namespace machines {
Graphics::Graphics(uint32_t w,uint32_t h):width_(w),height_(h) {
    if(!w||!h||uint64_t(w)*h>16777216)throw std::runtime_error("DRMVM surface dimensions exceed limit");
    pixels_.resize(size_t(w)*h);
}
void Graphics::clear(uint32_t rgb) {std::fill(pixels_.begin(),pixels_.end(),rgb&0xffffff);}
void Graphics::rectangle(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint32_t rgb) {
    if(x>=width_||y>=height_)return;
    w=std::min(w,width_-x);h=std::min(h,height_-y);
    for(uint32_t row=y;row<y+h;++row)
        std::fill_n(pixels_.begin()+size_t(row)*width_+x,w,rgb&0xffffff);
}
mvm_status Graphics::clear_host(mvm_vm*,const mvm_value*a,size_t n,mvm_value*r,void*u,mvm_error*) {
    if(n!=1||a[0].kind!=MVM_INT)return MVM_TYPE;
    static_cast<Graphics*>(u)->clear(uint32_t(a[0].as.integer));*r=mvm_nil();return MVM_OK;
}
mvm_status Graphics::rect_host(mvm_vm*,const mvm_value*a,size_t n,mvm_value*r,void*u,mvm_error*) {
    if(n!=5)return MVM_INVALID;
    for(size_t k=0;k<n;++k)if(a[k].kind!=MVM_INT)return MVM_TYPE;
    for(size_t k=0;k<4;++k)if(a[k].as.integer<0||uint64_t(a[k].as.integer)>UINT32_MAX)return MVM_BOUNDS;
    static_cast<Graphics*>(u)->rectangle(uint32_t(a[0].as.integer),uint32_t(a[1].as.integer),uint32_t(a[2].as.integer),uint32_t(a[3].as.integer),uint32_t(a[4].as.integer));
    *r=mvm_nil();return MVM_OK;
}
mvm_status Graphics::bind(mvm_vm*v,mvm_error*e) {
    const mvm_native_binding bindings[]={
        {"drm_clear",1,clear_host,this},{"drm_rect",5,rect_host,this}};
    return mvm_register_natives(v,bindings,2,e);
}
}}
#ifdef METAVM_HAVE_LIBDRM
#include <xf86drm.h>
#include <drm_mode.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
namespace metavm { namespace machines {
DumbBuffer::DumbBuffer(int fd,uint32_t w,uint32_t h):width_(w),height_(h) {
    if(!w||!h||uint64_t(w)*h>16777216)throw std::runtime_error("DRMVM buffer dimensions");
    fd_=dup(fd);if(fd_<0)throw std::runtime_error("DRMVM dup fd failed");
    drm_mode_create_dumb create{};create.width=w;create.height=h;create.bpp=32;
    if(drmIoctl(fd_,DRM_IOCTL_MODE_CREATE_DUMB,&create)!=0){close(fd_);fd_=-1;throw std::runtime_error("DRMVM create dumb failed");}
    handle_=create.handle;pitch_=create.pitch;size_=create.size;
    drm_mode_map_dumb mapping{};mapping.handle=handle_;
    bool valid=pitch_>=uint64_t(w)*4 && size_>=uint64_t(pitch_)*h && size_<=SIZE_MAX;
    if(valid && drmIoctl(fd_,DRM_IOCTL_MODE_MAP_DUMB,&mapping)==0)
        map_=mmap(nullptr,size_t(size_),PROT_READ|PROT_WRITE,MAP_SHARED,fd_,mapping.offset);
    if(!map_||map_==MAP_FAILED) {
        drm_mode_destroy_dumb destroy{};destroy.handle=handle_;drmIoctl(fd_,DRM_IOCTL_MODE_DESTROY_DUMB,&destroy);close(fd_);fd_=-1;map_=nullptr;
        throw std::runtime_error("DRMVM map dumb failed");
    }
}
DumbBuffer::~DumbBuffer() {
    if(map_)munmap(map_,size_t(size_));
    if(fd_>=0){drm_mode_destroy_dumb destroy{};destroy.handle=handle_;drmIoctl(fd_,DRM_IOCTL_MODE_DESTROY_DUMB,&destroy);close(fd_);}
}
void DumbBuffer::upload(const Graphics&s) {
    if(s.width()!=width_||s.height()!=height_)throw std::runtime_error("DRMVM surface size mismatch");
    for(uint32_t y=0;y<height_;++y)std::memcpy(static_cast<unsigned char*>(map_)+size_t(y)*pitch_,s.pixels().data()+size_t(y)*width_,size_t(width_)*4);
}
}}
#endif
