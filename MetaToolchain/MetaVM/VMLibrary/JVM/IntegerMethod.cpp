#include "IntegerMethod.hpp"
#include <cstring>
#include <limits>
#include <stdexcept>
namespace metavm { namespace machines {
static int32_t bits32(uint32_t x){int32_t r;std::memcpy(&r,&x,4);return r;}
int32_t jvm_integer_method(const std::vector<uint8_t>&c,const std::vector<int32_t>&args,size_t locals,size_t limit,uint64_t fuel) {
    if(c.empty()||c.size()>65535||locals>65535||locals<args.size()||!limit||limit>65535)throw std::runtime_error("JVM Code limits");
    std::vector<bool> boundary(c.size(),false);
    for(size_t pc=0;pc<c.size();) {
        boundary[pc]=true;unsigned op=c[pc];size_t len=1;
        if(op==0x10||op==0x15||op==0x36)len=2;
        else if(op==0x11||op==0x84||(op>=0x99&&op<=0xa4)||op==0xa7)len=3;
        else if(!(op==0||op==0x57||op==0x59||op==0xac||(op>=2&&op<=8)||(op>=0x1a&&op<=0x1d)||(op>=0x3b&&op<=0x3e)||op==0x60||op==0x64||op==0x68||op==0x6c||op==0x70||op==0x74))throw std::runtime_error("unsupported JVM opcode");
        if(len>c.size()-pc)throw std::runtime_error("truncated JVM instruction");
        pc+=len;
    }
    for(size_t pos=0;pos<c.size();++pos) if(boundary[pos]) {
        unsigned op=c[pos];
        if((op>=0x99&&op<=0xa4)||op==0xa7) {
            unsigned raw=(unsigned(c[pos+1])<<8)|c[pos+2];
            int delta=raw<32768?int(raw):int(raw)-65536;
            int64_t target=int64_t(pos)+delta;
            if(target<0||uint64_t(target)>=c.size()||!boundary[size_t(target)])
                throw std::runtime_error("JVM invalid static branch target");
        }
    }
    std::vector<int32_t> l(locals),stack;std::vector<bool> initialized(locals,false);
    for(size_t j=0;j<args.size();++j){l[j]=args[j];initialized[j]=true;}
    auto pop=[&](){if(stack.empty())throw std::runtime_error("JVM stack underflow");auto v=stack.back();stack.pop_back();return v;};
    auto push=[&](int32_t v){if(stack.size()>=limit)throw std::runtime_error("JVM stack overflow");stack.push_back(v);};
    auto load=[&](size_t idx){if(idx>=locals||!initialized[idx])throw std::runtime_error("JVM uninitialized local");return l[idx];};
    auto store=[&](size_t idx,int32_t v){if(idx>=locals)throw std::runtime_error("JVM local bounds");l[idx]=v;initialized[idx]=true;};
    size_t pc=0;
    for(uint64_t steps=0;steps<fuel;++steps) {
        if(pc>=c.size()||!boundary[pc])throw std::runtime_error("JVM invalid pc");
        size_t start=pc;unsigned op=c[pc++];
        auto byte=[&](){return c[pc++];};
        auto offset=[&](){unsigned v=unsigned(byte())<<8;v|=byte();return v<32768?int(v):int(v)-65536;};
        auto jump=[&](int delta){int64_t target=int64_t(start)+delta;if(target<0||uint64_t(target)>=c.size()||!boundary[size_t(target)])throw std::runtime_error("JVM invalid branch");pc=size_t(target);};
        if(op>=2&&op<=8)push(int32_t(op)-3);
        else if(op>=0x1a&&op<=0x1d)push(load(op-0x1a));
        else if(op>=0x3b&&op<=0x3e)store(op-0x3b,pop());
        else switch(op) {
        case 0:break;
        case 0x10:{int v=byte();push(v<128?v:v-256);break;}
        case 0x11:push(offset());break;
        case 0x15:push(load(byte()));break;
        case 0x36:{auto idx=byte();store(idx,pop());break;}
        case 0x57:pop();break;
        case 0x59:{auto a=pop();push(a);push(a);break;}
        case 0x60:case 0x64:case 0x68:case 0x6c:case 0x70:{
            int32_t b=pop(),a=pop(),r;
            if((op==0x6c||op==0x70)&&!b)throw std::runtime_error("JVM division by zero");
            if(op==0x60)r=bits32(uint32_t(a)+uint32_t(b));else if(op==0x64)r=bits32(uint32_t(a)-uint32_t(b));
            else if(op==0x68)r=bits32(uint32_t(a)*uint32_t(b));
            else if(a==INT32_MIN&&b==-1)r=op==0x6c?INT32_MIN:0;
            else r=op==0x6c?a/b:a%b;
            push(r);break;
        }
        case 0x74:push(bits32(0u-uint32_t(pop())));break;
        case 0x84:{unsigned idx=byte();int d=byte();if(d>=128)d-=256;store(idx,bits32(uint32_t(load(idx))+uint32_t(d)));break;}
        case 0xa7:jump(offset());break;
        case 0xac:return pop();
        default:{
            int d=offset();int32_t b=0,a=pop();unsigned cond=op-0x99;
            if(op>=0x9f){b=a;a=pop();cond=op-0x9f;}
            bool yes=cond==0?a==b:cond==1?a!=b:cond==2?a<b:cond==3?a>=b:cond==4?a>b:a<=b;
            if(yes)jump(d);
            break;
        }
        }
    }
    throw std::runtime_error("JVM fuel exhausted");
}
}}
