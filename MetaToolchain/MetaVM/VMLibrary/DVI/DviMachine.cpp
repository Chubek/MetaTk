#include "DviMachine.hpp"
#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>

namespace metavm { namespace machines {
namespace {
using D=DviMachine;
[[noreturn]] void fail(size_t offset,const std::string& reason) {
    throw std::runtime_error("DVI byte "+std::to_string(offset)+": "+reason);
}
struct Reader {
    const std::vector<uint8_t>& bytes;
    size_t pc=0;
    uint32_t u(unsigned n) {
        if(n>bytes.size()-pc)fail(pc,"truncated operand");
        uint32_t value=0;
        while(n--){value=(value<<8)|bytes[pc++];}
        return value;
    }
    int64_t s(unsigned n) {
        uint32_t value=u(n);
        return (uint64_t(value)&(uint64_t(1)<<(n*8-1)))?
            int64_t(value)-int64_t(uint64_t(1)<<(n*8)):int64_t(value);
    }
    int64_t index(unsigned n){return n==4?s(n):int64_t(u(n));}
    std::string text(size_t n) {
        if(n>bytes.size()-pc)fail(pc,"truncated text");
        std::string value(bytes.begin()+pc,bytes.begin()+pc+n);pc+=n;return value;
    }
};
struct Instruction {
    uint8_t op;
    size_t offset;
    int64_t a=0,b=0;
    size_t payload=0,length=0;
};
struct FontState {size_t index;bool body=false,post=false,used=false;};
struct Decoded {
    D::Result result;
    std::vector<Instruction> code;
    std::map<int64_t,FontState> fonts;
};
bool same(const D::Font&a,const D::Font&b) {
    return a.number==b.number&&a.checksum==b.checksum&&a.scale==b.scale&&
        a.design_size==b.design_size&&a.area==b.area&&a.name==b.name;
}
Decoded decode(const std::vector<uint8_t>&bytes,const D::Limits&limits) {
    if(bytes.size()>limits.max_input_bytes)fail(0,"input byte limit");
    Reader r{bytes};Decoded d;auto&out=d.result;
    auto positive=[&](const char*name) {
        int64_t value=r.s(4);if(value<=0)fail(r.pc-4,std::string(name)+" must be positive");
        return uint32_t(value);
    };
    auto count=[&](size_t offset) {
        if(out.instructions>=limits.max_instructions)fail(offset,"instruction limit");
        ++out.instructions;
    };
    if(r.u(1)!=247)fail(0,"missing preamble");
    count(0);
    if(r.u(1)!=2)fail(1,"unsupported format version (expected 2)");
    out.header.numerator=positive("numerator");
    out.header.denominator=positive("denominator");
    out.header.magnification=positive("magnification");
    auto comment_size=r.u(1);out.header.comment=r.text(comment_size);
    bool page=false,post=false,done=false;
    int64_t last_bop=-1;size_t post_offset=0,depth=0,special_bytes=0;
    while(r.pc<bytes.size()) {
        size_t offset=r.pc;auto op=uint8_t(r.u(1));count(offset);
        Instruction i{op,offset};
        if(op==138)continue;
        if(op>=243&&op<=246) {
            D::Font font;font.number=r.index(op-242);font.checksum=r.u(4);
            font.scale=int32_t(positive("font scale"));
            if(font.scale>=(int32_t(1)<<27))fail(offset,"font scale must be less than 2^27");
            font.design_size=int32_t(positive("font design size"));
            auto a=r.u(1),n=r.u(1);font.area=r.text(a);font.name=r.text(n);
            auto found=d.fonts.find(font.number);
            if(found==d.fonts.end()) {
                if(out.fonts.size()>=limits.max_fonts)fail(offset,"font limit");
                FontState state{out.fonts.size(),!post,post,false};
                d.fonts.emplace(font.number,state);out.fonts.push_back(std::move(font));
            } else {
                auto&state=found->second;
                if((post?state.post:state.body))fail(offset,"duplicate font definition");
                if(!same(out.fonts[state.index],font))fail(offset,"inconsistent font definition");
                if(post)state.post=true;else state.body=true;
            }
            continue;
        }
        if(op==139) {
            if(page||post)fail(offset,"BOP outside page sequence");
            if(out.pages.size()>=limits.max_pages)fail(offset,"page limit");
            D::Page p;p.begin_offset=offset;
            for(auto&c:p.counts)c=int32_t(r.s(4));
            if(r.s(4)!=last_bop)fail(offset,"invalid previous-page pointer");
            if(offset>INT32_MAX)fail(offset,"page pointer exceeds signed 32-bit range");
            last_bop=int64_t(offset);page=true;depth=0;
            i.a=int64_t(out.pages.size());out.pages.push_back(p);
        } else if(op==140) {
            if(!page)fail(offset,"EOP outside page");
            if(depth)fail(offset,"nonempty stack at EOP");
            out.pages.back().end_offset=offset;page=false;
        } else if(op==248) {
            if(page||post)fail(offset,"POST outside page sequence");
            if(r.s(4)!=last_bop)fail(offset,"invalid final-page pointer");
            if(r.u(4)!=out.header.numerator)fail(offset,"postamble numerator mismatch");
            if(r.u(4)!=out.header.denominator)fail(offset,"postamble denominator mismatch");
            if(r.u(4)!=out.header.magnification)fail(offset,"postamble magnification mismatch");
            int64_t h=r.s(4),w=r.s(4);
            if(h<0||w<0)fail(offset,"negative postamble page dimensions");
            out.header.max_height=int32_t(h);out.header.max_width=int32_t(w);
            out.header.declared_stack=uint16_t(r.u(2));
            if(out.header.declared_stack<out.peak_stack)fail(offset,"postamble understates stack depth");
            if(r.u(2)!=out.pages.size())fail(offset,"postamble page count mismatch");
            post=true;post_offset=offset;continue;
        } else if(op==249) {
            if(!post)fail(offset,"POST_POST before POST");
            if(r.s(4)!=int64_t(post_offset))fail(offset,"invalid postamble pointer");
            if(r.u(1)!=2)fail(offset,"trailer format version mismatch");
            if(bytes.size()-r.pc<4)fail(r.pc,"fewer than four trailer bytes");
            while(r.pc<bytes.size())if(r.u(1)!=223)fail(r.pc-1,"invalid trailer byte");
            done=true;break;
        } else {
            if(op>=247)fail(offset,"undefined or misplaced opcode "+std::to_string(op));
            if(!page||post)fail(offset,"page instruction outside page");
            if(op<128)i.a=op;
            else if(op<=131)i.a=r.index(op-127);
            else if(op==132||op==137){i.a=r.s(4);i.b=r.s(4);}
            else if(op<=136)i.a=r.index(op-132);
            else if(op==141) {
                if(depth>=limits.max_stack)fail(offset,"stack limit");
                ++depth;out.peak_stack=std::max(out.peak_stack,depth);
            } else if(op==142) {
                if(!depth)fail(offset,"stack underflow");
                --depth;
            } else if(op<=146)i.a=r.s(op-142);
            else if(op<=151){if(op!=147)i.a=r.s(op-147);}
            else if(op<=156){if(op!=152)i.a=r.s(op-152);}
            else if(op<=160)i.a=r.s(op-156);
            else if(op<=165){if(op!=161)i.a=r.s(op-161);}
            else if(op<=170){if(op!=166)i.a=r.s(op-166);}
            else if(op<=234)i.a=op-171;
            else if(op<=238)i.a=r.index(op-234);
            else {
                auto length=r.u(op-238);
                if(length>bytes.size()-r.pc)fail(offset,"truncated special");
                if(length>limits.max_special_bytes-special_bytes)fail(offset,"special byte limit");
                special_bytes+=length;i.payload=r.pc;i.length=length;r.pc+=length;
            }
            if(op>=171&&op<=238) {
                auto f=d.fonts.find(i.a);
                if(f==d.fonts.end()||!f->second.body)fail(offset,"undefined font selection");
                f->second.used=true;
            }
        }
        d.code.push_back(i);
    }
    if(!done)fail(r.pc,"missing postamble or trailer");
    for(const auto&entry:d.fonts)
        if(entry.second.used&&!entry.second.post)fail(post_offset,"used font missing from postamble");
    return d;
}
void move(int32_t&position,int64_t amount,size_t offset) {
    int64_t next=int64_t(position)+amount;
    if(next<INT32_MIN||next>INT32_MAX)fail(offset,"coordinate overflow");
    position=int32_t(next);
}
}
DviMachine::Result DviMachine::run(const std::vector<uint8_t>&bytes,const Metrics&metrics) {
    return run(bytes,metrics,Limits{});
}
DviMachine::Result DviMachine::run(const std::vector<uint8_t>&bytes,const Metrics&metrics,const Limits&limits) {
    auto d=decode(bytes,limits);auto&out=d.result;
    Position position;std::vector<Position> stack;size_t page=0;
    int64_t font=0;bool has_font=false;
    for(const auto&i:d.code) {
        auto op=i.op;
        auto emit=[&](Kind kind)->Event& {
            if(out.events.size()>=limits.max_events)fail(i.offset,"event limit");
            Event e;e.kind=kind;e.source_offset=i.offset;e.page=page;
            e.position=position;e.font=font;out.events.push_back(std::move(e));
            return out.events.back();
        };
        if(op==139) {
            position=Position{};stack.clear();has_font=false;font=0;page=size_t(i.a);
            emit(Kind::begin_page);
        } else if(op==140)emit(Kind::end_page);
        else if(op<=131||(op>=133&&op<=136)) {
            if(!has_font)fail(i.offset,"character without a selected font");
            bool advances=op<=131;int32_t width=0;
            if(advances) {
                if(!metrics)fail(i.offset,"SET character requires font metrics");
                width=metrics(out.fonts[d.fonts.at(font).index],i.a);
            }
            auto&e=emit(Kind::glyph);e.character=i.a;e.advances=advances;e.width=width;
            if(advances)move(position.h,width,i.offset);
        } else if(op==132||op==137) {
            if(i.a>0&&i.b>0) {
                auto&e=emit(Kind::rule);e.height=int32_t(i.a);e.width=int32_t(i.b);e.advances=op==132;
            }
            if(op==132)move(position.h,i.b,i.offset);
        } else if(op==141)stack.push_back(position);
        else if(op==142){position=stack.back();stack.pop_back();}
        else if(op<=146)move(position.h,i.a,i.offset);
        else if(op<=151) {
            if(op!=147)position.w=int32_t(i.a);
            move(position.h,position.w,i.offset);
        } else if(op<=156) {
            if(op!=152)position.x=int32_t(i.a);
            move(position.h,position.x,i.offset);
        } else if(op<=160)move(position.v,i.a,i.offset);
        else if(op<=165) {
            if(op!=161)position.y=int32_t(i.a);
            move(position.v,position.y,i.offset);
        } else if(op<=170) {
            if(op!=166)position.z=int32_t(i.a);
            move(position.v,position.z,i.offset);
        } else if(op<=238){font=i.a;has_font=true;}
        else {
            auto&e=emit(Kind::special);
            e.bytes=std::string(bytes.begin()+i.payload,bytes.begin()+i.payload+i.length);
        }
    }
    return std::move(out);
}
}}
