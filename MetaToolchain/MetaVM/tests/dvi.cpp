#include "VMLibrary/DVI/DviMachine.hpp"
#include "vmweave/DviReplay.hpp"
#include <iostream>
#include <stdexcept>

using D=metavm::machines::DviMachine;
#define CHECK(x) do{if(!(x))throw std::runtime_error("check failed: " #x);}while(0)
template<class F> void rejects(F f,const std::string&message) {
    try{f();}catch(const std::runtime_error&e){
        if(std::string(e.what()).find(message)==std::string::npos)
            throw std::runtime_error("expected '"+message+"', got '"+e.what()+"'");
        return;
    }
    throw std::runtime_error("expected rejection: "+message);
}
using Bytes=std::vector<uint8_t>;
static void word(Bytes&b,int64_t value,unsigned n=4) {
    auto bits=uint64_t(value);
    for(unsigned k=n;k>0;--k)b.push_back(uint8_t(bits>>((k-1)*8)));
}
struct Fixture {
    Bytes bytes,definitions;
    int64_t last=-1;uint16_t pages=0;
    size_t post=0;
    Fixture() {
        bytes={247,2};word(bytes,25400000);word(bytes,473628672);word(bytes,1000);bytes.push_back(0);
    }
    void font(int64_t number=0,unsigned n=1) {
        Bytes f{uint8_t(242+n)};word(f,number,n);word(f,0xabcdef01);word(f,655360);word(f,655360);
        f.insert(f.end(),{0,4,'t','e','s','t'});
        bytes.insert(bytes.end(),f.begin(),f.end());definitions.insert(definitions.end(),f.begin(),f.end());
    }
    void begin() {
        auto offset=bytes.size();bytes.push_back(139);
        for(int k=0;k<10;++k)word(bytes,k-5);
        word(bytes,last);last=int64_t(offset);++pages;
    }
    void end(){bytes.push_back(140);}
    Bytes finish(uint16_t stack=0) {
        post=bytes.size();bytes.push_back(248);word(bytes,last);
        word(bytes,25400000);word(bytes,473628672);word(bytes,1000);word(bytes,0);word(bytes,0);
        word(bytes,stack,2);word(bytes,pages,2);bytes.insert(bytes.end(),definitions.begin(),definitions.end());
        bytes.push_back(249);word(bytes,int64_t(post));bytes.insert(bytes.end(),{2,223,223,223,223});
        return bytes;
    }
};
static int32_t metric(const D::Font&font,int64_t) {
    CHECK(font.checksum==0xabcdef01);CHECK(font.scale==655360);CHECK(font.name=="test");return 7;
}
static void instructions() {
    Fixture f;f.font();f.begin();f.bytes.push_back(171);
    for(unsigned c=0;c<128;++c)f.bytes.push_back(uint8_t(c));
    for(unsigned n=1;n<=4;++n) {
        f.bytes.push_back(uint8_t(127+n));word(f.bytes,n==4?-1:((int64_t(1)<<(8*n))-1),n);
        f.bytes.push_back(uint8_t(132+n));word(f.bytes,n==4?INT32_MIN:42,n);
    }
    f.end();auto bytes=f.finish();auto result=D::run(bytes,metric);
    CHECK(result.events.size()==138);CHECK(result.pages[0].counts[0]==-5);
    CHECK(result.events.back().position.h==132*7);CHECK(result.fonts.size()==1);
    for(unsigned c=0;c<128;++c) {
        auto&e=result.events[c+1];CHECK(e.kind==D::Kind::glyph&&e.character==c&&e.position.h==int32_t(c*7));
    }
    CHECK(result.events[135].character==-1);CHECK(result.events[136].character==INT32_MIN);
    rejects([&]{D::run(bytes);},"requires font metrics");
    auto neg=D::run(bytes,[](const auto&,int64_t){return -3;});CHECK(neg.events.back().position.h==-396);
    auto zero=D::run(bytes,[](const auto&,int64_t){return 0;});CHECK(zero.events.back().position.h==0);
    // Every font-number opcode, including signed fnt4/def4 and unsigned fnt3.
    Fixture fonts;
    for(unsigned n=0;n<64;++n)fonts.font(n);
    for(unsigned n=1;n<=4;++n)fonts.font(n==4?-1:(int64_t(1)<<(n*8))-1,n);
    fonts.begin();
    for(unsigned n=0;n<64;++n){fonts.bytes.push_back(uint8_t(171+n));fonts.bytes.insert(fonts.bytes.end(),{133,42});}
    for(unsigned n=1;n<=4;++n) {
        fonts.bytes.push_back(uint8_t(234+n));word(fonts.bytes,n==4?-1:(int64_t(1)<<(n*8))-1,n);
        fonts.bytes.insert(fonts.bytes.end(),{133,42});
    }
    fonts.end();auto allfonts=D::run(fonts.finish());CHECK(allfonts.fonts.size()==68);
    CHECK(allfonts.events[68].font==-1);CHECK(allfonts.events[67].font==16777215);
    // All signed movement sizes at both extremes. Set/reuse each spacing register.
    for(unsigned n=1;n<=4;++n)for(unsigned group=0;group<6;++group) {
        Fixture p;p.begin();auto&b=p.bytes;
        unsigned base=group==0?142:group==1?147:group==2?152:group==3?156:group==4?161:166;
        int64_t min=-(int64_t(1)<<(n*8-1)),max=-min-1;
        b.push_back(uint8_t(base+n));word(b,min,n);b.push_back(uint8_t(base+n));word(b,max,n);
        if(group!=0&&group!=3)b.push_back(uint8_t(base));
        p.end();auto r=D::run(p.finish());auto pos=r.events.back().position;
        int32_t expected=int32_t((group==0||group==3)?-1:max-1);
        CHECK((group<3?pos.h:pos.v)==expected);
    }
    // Font selection survives POP; the six positioning registers do not.
    Fixture state;state.font(0);state.font(1);state.begin();auto&b=state.bytes;
    b.insert(b.end(),{171,148,3,153,4,162,5,167,6,141,148,10,153,11,162,12,167,13,172,142,147,152,161,166,133,9});
    state.end();auto s=D::run(state.finish(1));const auto&e=s.events[s.events.size()-2];
    CHECK(e.font==1&&e.position.h==14&&e.position.v==22);
    CHECK(e.position.w==3&&e.position.x==4&&e.position.y==5&&e.position.z==6);
    // SET_RULE advances even for non-positive dimensions; PUT_RULE never does.
    Fixture rules;rules.begin();
    auto rule=[&](unsigned op,int32_t h,int32_t w){rules.bytes.push_back(uint8_t(op));word(rules.bytes,h);word(rules.bytes,w);};
    rule(132,3,7);rule(137,5,9);rule(132,0,-4);rule(137,-1,99);rule(132,-1,2);
    rules.bytes.push_back(138);rules.end();auto r=D::run(rules.finish());
    CHECK(r.events.size()==4);CHECK(r.events[1].position.h==0&&r.events[1].advances);
    CHECK(r.events[2].position.h==7&&!r.events[2].advances);CHECK(r.events.back().position.h==5);
    // All special length widths, embedded NUL, and zero-length payloads.
    Fixture specials;specials.begin();
    for(unsigned n=1;n<=4;++n) {
        specials.bytes.push_back(uint8_t(238+n));word(specials.bytes,3,n);specials.bytes.insert(specials.bytes.end(),{0,255,42});
        specials.bytes.push_back(uint8_t(238+n));word(specials.bytes,0,n);
    }
    specials.end();auto sp=D::run(specials.finish());CHECK(sp.events.size()==10);
    for(size_t i=1;i<9;i+=2){CHECK(sp.events[i].bytes==std::string("\0\xff*",3));CHECK(sp.events[i+1].bytes.empty());}
    // Page transitions reset all positioning state and the current font.
    Fixture pages;pages.font();pages.begin();pages.bytes.insert(pages.bytes.end(),{171,148,42,65});pages.end();
    pages.begin();pages.bytes.insert(pages.bytes.end(),{147,152,161,166});pages.end();
    auto pg=D::run(pages.finish(),metric);CHECK(pg.pages.size()==2&&pg.events.back().position.h==0);
    CHECK(pg.events.back().position.v==0&&pg.events.back().page==1);
    Fixture nofont;nofont.font();nofont.begin();nofont.bytes.push_back(171);nofont.end();nofont.begin();nofont.bytes.push_back(65);nofont.end();
    rejects([&]{D::run(nofont.finish(),metric);},"without a selected font");
}
static void malformed() {
    Fixture f;f.font();f.begin();f.bytes.insert(f.bytes.end(),{171,141,65,142});f.end();auto good=f.finish(1);
    for(size_t n=0;n<good.size();++n) {
        // The final signature accepts any length >=4; this fixture has exactly 4.
        rejects([&]{D::run(Bytes(good.begin(),good.begin()+n),metric);},"DVI byte");
    }
    auto changed=[&](size_t offset,uint8_t value,const std::string&error) {
        auto b=good;b.at(offset)=value;rejects([&]{D::run(b,metric);},error);
    };
    changed(0,138,"preamble");changed(1,3,"version");changed(2,128,"positive");
    changed(35+41,0,"previous-page pointer"); // BOP follows 20-byte font definition.
    changed(f.post+1,1,"final-page pointer");changed(f.post+5,0,"numerator mismatch");
    changed(f.post+9,0,"denominator mismatch");changed(f.post+13,1,"magnification mismatch");
    changed(f.post+17,255,"negative postamble");changed(f.post+26,0,"understates stack");
    changed(f.post+28,2,"page count mismatch");
    // Replacing POST_POST with SET_CHAR_0 is a page instruction in the postamble.
    changed(good.size()-10,0,"page instruction outside page");
    changed(good.size()-9,127,"postamble pointer");changed(good.size()-5,3,"version mismatch");
    changed(good.size()-1,0,"trailer byte");
    for(unsigned op=250;op<=255;++op) {
        Fixture p;p.begin();p.bytes.push_back(uint8_t(op));p.end();
        rejects([&]{D::run(p.finish());},"undefined or misplaced opcode");
    }
    Fixture nested;nested.begin();nested.begin();nested.end();rejects([&]{D::run(nested.finish());},"BOP");
    Fixture outside;outside.bytes.push_back(140);rejects([&]{D::run(outside.finish());},"EOP");
    Fixture pop;pop.begin();pop.bytes.push_back(142);pop.end();rejects([&]{D::run(pop.finish());},"underflow");
    Fixture push;push.begin();push.bytes.push_back(141);push.end();rejects([&]{D::run(push.finish(1));},"nonempty stack");
    Fixture undef;undef.begin();undef.bytes.push_back(171);undef.end();rejects([&]{D::run(undef.finish());},"undefined font");
    Fixture special;special.begin();special.bytes.insert(special.bytes.end(),{242,255,255,255,255});
    rejects([&]{D::run(special.finish());},"truncated special");
    Fixture late;late.font();late.begin();late.bytes.push_back(171);late.end();late.definitions.clear();
    rejects([&]{D::run(late.finish());},"missing from postamble");
    Fixture dup;dup.font();dup.font();rejects([&]{D::run(dup.finish());},"duplicate font");
    Fixture mismatch;mismatch.font();mismatch.definitions[5]=2;
    rejects([&]{D::run(mismatch.finish());},"inconsistent font");
    Fixture scale;scale.font();scale.bytes[21]=8; // First scale byte: 2^27.
    rejects([&]{D::run(scale.finish());},"scale must");
    Fixture overflow;overflow.begin();overflow.bytes.push_back(146);word(overflow.bytes,INT32_MAX);
    overflow.bytes.insert(overflow.bytes.end(),{143,1});overflow.end();
    rejects([&]{D::run(overflow.finish());},"coordinate overflow");
    Fixture underflow;underflow.begin();underflow.bytes.push_back(160);word(underflow.bytes,INT32_MIN);
    underflow.bytes.insert(underflow.bytes.end(),{157,255});underflow.end();
    rejects([&]{D::run(underflow.finish());},"coordinate overflow");
    // A broken trailer must be diagnosed before any metric callback runs.
    auto broken=good;broken.back()=0;size_t calls=0;
    rejects([&]{D::run(broken,[&](const auto&,int64_t){++calls;return 0;});},"trailer byte");CHECK(calls==0);
    // Deterministic mutations of a complete file: all must return or reject
    // cleanly under ASan/UBSan, even when lengths/opcodes become hostile.
    uint32_t random=0x70d41;
    for(unsigned k=0;k<4096;++k) {
        auto b=good;random=random*1664525u+1013904223u;size_t offset=random%b.size();
        random=random*1664525u+1013904223u;b[offset]=uint8_t(random>>24);
        try{D::run(b,metric);}catch(const std::runtime_error&){}
    }
}
static void limits_and_replay() {
    Fixture f;f.font();f.begin();f.bytes.insert(f.bytes.end(),{171,141,65,142,239,3,0,1,2});f.end();auto bytes=f.finish(1);
    auto result=D::run(bytes,metric);D::Limits limit;
    limit.max_input_bytes=bytes.size();limit.max_instructions=result.instructions;limit.max_fonts=1;
    limit.max_pages=1;limit.max_stack=1;limit.max_events=result.events.size();limit.max_special_bytes=3;
    CHECK(D::run(bytes,metric,limit).events.size()==4);
    for(unsigned field=0;field<7;++field) {
        auto bound=limit;
        switch(field){case 0:--bound.max_input_bytes;break;case 1:--bound.max_instructions;break;
        case 2:--bound.max_fonts;break;case 3:--bound.max_pages;break;case 4:--bound.max_stack;break;
        case 5:--bound.max_events;break;case 6:--bound.max_special_bytes;break;}
        rejects([&]{D::run(bytes,metric,bound);},"limit");
    }
    Fixture empty;D::Limits zero;zero.max_events=zero.max_pages=zero.max_fonts=zero.max_stack=zero.max_special_bytes=0;
    CHECK(D::run(empty.finish(),{},zero).events.empty());
    for(auto engine:{MVM_ENGINE_BYTECODE,MVM_ENGINE_THREADED,MVM_ENGINE_XVM}) {
        size_t observed=0;
        vmweave::DviReplay replay(result,[&](const D::Event&e) {
            const auto&expected=result.events.at(observed++);
            CHECK(e.kind==expected.kind&&e.source_offset==expected.source_offset&&e.bytes==expected.bytes);
            CHECK(e.position.h==expected.position.h&&e.position.v==expected.position.v);
        });
        auto module=replay.module();metavm::VM vm;mvm_error error{};
        metavm::check(replay.bind(vm.get(),&error),error);
        CHECK(replay.bind(vm.get(),&error)!=MVM_OK); // Existing binding remains usable.
        auto value=vm.run(module,0,{},engine);CHECK(value.kind==MVM_INT&&value.as.integer==4&&observed==4);
        rejects([&]{replay.module(10);},"source limit");
        mvm_buffer binary{};
        metavm::check(mvm_serialize(module.get(),&binary,&error),error);
        mvm_module*copy=nullptr;
        auto status=mvm_deserialize(binary.data,binary.size,0,&copy,&error);
        mvm_buffer_dispose(&binary);metavm::check(status,error);
        observed=0;mvm_value replayed=mvm_nil();
        status=mvm_run(vm.get(),copy,0,nullptr,0,engine,&replayed,&error);
        mvm_module_destroy(copy);metavm::check(status,error);
        CHECK(observed==4&&replayed.kind==MVM_INT&&replayed.as.integer==4);
    }
    vmweave::DviReplay throwing(result,[](const D::Event&){throw std::runtime_error("sink failure");});
    auto module=throwing.module();metavm::VM vm;mvm_error error{};
    metavm::check(throwing.bind(vm.get(),&error),error);rejects([&]{vm.run(module);},"sink failure");
    vmweave::DviReplay nonstandard(result,[](const D::Event&){throw 42;});
    auto unknown=nonstandard.module();metavm::VM vm2;metavm::check(nonstandard.bind(vm2.get(),&error),error);
    rejects([&]{vm2.run(unknown);},"unknown exception");
    vmweave::DviReplay oom(result,[](const D::Event&){throw std::bad_alloc();});
    auto alloc=oom.module();metavm::VM vm3;metavm::check(oom.bind(vm3.get(),&error),error);
    try{vm3.run(alloc);throw std::runtime_error("expected sink allocation failure");}
    catch(const metavm::Error&e){
        // Machinery deliberately normalizes every failing host callback to
        // MVM_HOST_ERROR; its diagnostic retains the allocation failure.
        CHECK(e.status==MVM_HOST_ERROR);
        CHECK(std::string(e.what()).find("allocation failed")!=std::string::npos);
    }
    auto call=metavm::Module::compile(".import dvi_event 1\n.function main 0 2\nconst r0 -1\nhost r1 dvi_event r0\nret r1\n.end\n");
    rejects([&]{vm.run(call);},"out of range");
    auto wrong_type=metavm::Module::compile(".import dvi_event 1\n.function main 0 2\nnil r0\nhost r1 dvi_event r0\nret r1\n.end\n");
    rejects([&]{vm.run(wrong_type);},"integer event index");
}
int main() {
    try{instructions();malformed();limits_and_replay();std::cout<<"DVI opcode families, structure, limits, mutations and Machinery replay passed\n";}
    catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
