#pragma once
#include "MetaVM.h"
#include <cstdint>
#include <vector>
namespace metavm { namespace machines {
/* Host-owned XRGB8888 surface. A compositor owns presentation and scheduling.
 * The surface must outlive any Machinery VM whose native imports refer to it. */
class Graphics {
    uint32_t width_,height_;
    std::vector<uint32_t> pixels_;
    static mvm_status clear_host(mvm_vm*,const mvm_value*,size_t,mvm_value*,void*,mvm_error*);
    static mvm_status rect_host(mvm_vm*,const mvm_value*,size_t,mvm_value*,void*,mvm_error*);
public:
    Graphics(uint32_t width,uint32_t height);
    uint32_t width()const{return width_;} uint32_t height()const{return height_;}
    const std::vector<uint32_t>& pixels()const{return pixels_;}
    void clear(uint32_t rgb);
    void rectangle(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint32_t rgb);
    mvm_status bind(mvm_vm*,mvm_error*);
};
#ifdef METAVM_HAVE_LIBDRM
/* Owns a duplicate of a caller-authorized DRM/KMS fd, a dumb GEM allocation,
 * and its mapping. No CRTC changes, device discovery, or DRM master acquisition.
 * Construct only with a KMS-capable fd; render nodes lack dumb-buffer support. */
class DumbBuffer {
    int fd_=-1; uint32_t handle_=0,pitch_=0,width_=0,height_=0;
    uint64_t size_=0; void* map_=nullptr;
public:
    DumbBuffer(int fd,uint32_t width,uint32_t height);
    ~DumbBuffer();
    DumbBuffer(const DumbBuffer&)=delete; DumbBuffer&operator=(const DumbBuffer&)=delete;
    uint32_t handle()const{return handle_;}uint32_t pitch()const{return pitch_;}
    void upload(const Graphics&);
};
#endif
}}
