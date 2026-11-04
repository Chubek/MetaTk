#include "VMLibrary/Library.hpp"
#include "VMLibrary/CHIP-8/CHIP-8.hpp"
#include "VMLibrary/SSAVM/SSAVM.hpp"
#include "VMLibrary/DRMVM/DRMVM.hpp"
#include "VMLibrary/JVM/JVM.hpp"
#include "VMLibrary/FORTH/FORTH.hpp"
#include "MetaVM.hpp"
#include "vmweave.h"
#include "vmweave/Builder.hpp"
#include "vmspecs/BMachine/BMachine.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#define CHECK(x) do {if(!(x))throw std::runtime_error("check failed: " #x);}while(0)
using namespace metavm::machines;
static void traps(const std::function<void()>&f) {bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
static void test_catalog() {
    size_t native=0;
    for(const auto&p:metavm::library::catalog())if(std::string(p.coverage)!="catalog-only")++native;
    CHECK(native==8); // DVI format-2 execution added alongside the prior seven profiles.
    CHECK(std::string(metavm::library::find("DVI")->coverage)=="dvi2-interpreter");
    CHECK(std::string(metavm::library::find("SECD")->coverage)=="secd-unary-subset");
    CHECK(std::string(metavm::library::find("Krivine")->coverage)=="krivine-whnf-subset");
    CHECK(metavm::library::catalog().size()==49);
    CHECK(!metavm::library::find("absent"));
    mvm_module*missing=reinterpret_cast<mvm_module*>(uintptr_t(1));mvm_error missing_error{};
    CHECK(legacy_BMachine_example(&missing,&missing_error)==MVM_NOT_FOUND);
    CHECK(missing==nullptr&&missing_error.status==MVM_NOT_FOUND);
    CHECK(std::string(missing_error.message).find("BMachine")!=std::string::npos);
    CHECK(legacy_BMachine_example(nullptr,&missing_error)==MVM_INVALID);
    CHECK(missing_error.status==MVM_INVALID);
    for(const auto&p:metavm::library::catalog()) {
        mvm_module*m=nullptr;mvm_error e{};CHECK(p.example_module(&m,&e)==MVM_OK);
        CHECK(mvm_module_verify(m,&e)==MVM_OK);
        for(auto engine:{MVM_ENGINE_BYTECODE,MVM_ENGINE_THREADED}) {
            Graphics surface(4,4);metavm::VM vm;
            if(std::string(p.name)=="DRMVM")CHECK(surface.bind(vm.get(),&e)==MVM_OK);
            mvm_value result{};CHECK(mvm_run(vm.get(),m,0,nullptr,0,engine,&result,&e)==MVM_OK);
            CHECK(result.kind==MVM_INT&&result.as.integer==42);
            if(std::string(p.name)=="DRMVM")CHECK(surface.pixels()[5]==0xff0000);
        }
        mvm_module_destroy(m);
    }
}
static void test_chip8() {
    Chip8 m;CHECK(m.load({0x60,0xff,0x61,0x02,0x80,0x14,0xa3,0x00,0xf0,0x33}));
    for(int k=0;k<3;++k)CHECK(m.step()==Chip8::Status::running);
    CHECK(m.v[0]==1&&m.v[15]==1);
    CHECK(m.step()==Chip8::Status::running);CHECK(m.step()==Chip8::Status::running);
    CHECK(m.memory[0x300]==0&&m.memory[0x301]==0&&m.memory[0x302]==1);
    CHECK(m.load({0xf0,0x0a}));CHECK(m.step()==Chip8::Status::waiting);CHECK(m.pc==0x200);
    m.keys[3]=true;CHECK(m.step()==Chip8::Status::running);CHECK(m.v[0]==3);
    CHECK(m.load({0xd0,0x11,0xd0,0x11}));m.i=0x300;m.memory[m.i]=0x80;
    CHECK(m.step()==Chip8::Status::running);CHECK(m.display[0]==1);
    CHECK(m.step()==Chip8::Status::running);CHECK(m.display[0]==0&&m.v[15]==1);
    CHECK(m.load({0x22,0x04,0x12,0x02,0x00,0xee}));
    CHECK(m.step()==Chip8::Status::running);CHECK(m.pc==0x204&&m.sp==1);
    CHECK(m.step()==Chip8::Status::running);CHECK(m.pc==0x202&&m.sp==0);
    CHECK(m.load({0x00,0xee}));CHECK(m.step()==Chip8::Status::stack_underflow);
    CHECK(m.load({0xf0,0x65}));m.i=4096;CHECK(m.step()==Chip8::Status::bounds);
    m.delay=2;m.sound=1;m.tick_60hz();CHECK(m.delay==1&&m.sound==0);
    CHECK(!m.load(std::vector<uint8_t>(4097)));
    CHECK(m.load({0x80,0x06}));m.v[0]=3;CHECK(m.step()==Chip8::Status::running);CHECK(m.v[0]==1&&m.v[15]==1);
    m.quirks.shift_uses_y=true;CHECK(m.load({0x80,0x16}));m.v[0]=3;m.v[1]=8;
    CHECK(m.step()==Chip8::Status::running);CHECK(m.v[0]==4&&m.v[15]==0);
}
static void test_issa() {
    using O=ISSA::Op;using I=ISSA::Instruction;
    // Paper figure 5; phi at index 5 must read the previous index-4 value.
    std::vector<I> fib={{O::constant,{},0},{O::constant,{},1},{O::constant,{},10},{O::constant,{},2},
        {O::phi,{1,8}},{O::phi,{0,4}},{O::phi,{3,9}},{O::pfe,{}},
        {O::add,{4,5}},{O::add,{1,6}},{O::branch_le,{9,2},0,4,1},{O::ret,{8}}};
    CHECK(std::get<int64_t>(ISSA::run(fib).value)==55);
    traps([&]{ISSA::run(fib,{},5);});
    auto bad=fib;bad[4].operands={999};traps([&]{ISSA::run(bad);});
    bad=fib;bad[7].op=O::constant;traps([&]{ISSA::run(bad);});
    std::vector<I> arrays={{O::constant,{},2},{O::array,{0}},{O::constant,{},0},{O::constant,{},7},
        {O::set,{1,2,3}},{O::get,{1,2}},{O::get,{4,2}},{O::add,{5,6}},{O::ret,{7}}};
    CHECK(std::get<int64_t>(ISSA::run(arrays).value)==7);
    arrays[2].immediate=-1;traps([&]{ISSA::run(arrays);});
    traps([]{ISSA::run({{O::add,{0,0}},{O::ret,{0}}});});
}
static void test_jvm_forth() {
    CHECK(jvm_integer_method({0x10,6,0x10,7,0x68,0xac})==42);
    CHECK(jvm_integer_method({0x1a,0x04,0x60,0xac},{41})==42);
    // i=0; while (i<5) ++i; return i.
    CHECK(jvm_integer_method({0x03,0x3b,0x1a,0x08,0xa2,0,9,0x84,0,1,0xa7,0xff,0xf8,0x1a,0xac})==5);
    traps([]{jvm_integer_method({0x10});});
    traps([]{jvm_integer_method({0xa7,0,1});});
    traps([]{jvm_integer_method({0x04,0x03,0x6c,0xac});});
    traps([]{jvm_integer_method({0x1a,0xac});});
    traps([]{jvm_integer_method({0xa7,0,0},{},1,1,10);});
    CHECK(forth(": square dup * ; 6 square 6 +")==std::vector<int64_t>{42});
    CHECK(forth("1 2 swap over +")==std::vector<int64_t>({2,3}));
    CHECK(forth("2 3 <")==std::vector<int64_t>{-1});
    traps([]{forth("drop");});traps([]{forth("1 0 /");});traps([]{forth(": recursive recursive ; recursive",10);});
}
static void test_builder() {
    vmweave::Builder b;b.function("main",0,3);b.constant(0,6);b.constant(1,7);b.binary("mul",2,0,1);b.ret(2);b.end();
    auto m=b.finish();metavm::VM vm;CHECK(vm.run(m).as.integer==42);
    traps([&]{b.binary("anything",0,0,0);});
    traps([&]{b.constant(-1,0);});
    traps([]{vmweave::Builder b;b.function("main",0,1);b.finish();});
    traps([]{vmweave::Builder b(4);b.emit("1234");});
    traps([]{vmweave::Builder b;b.function("bad\nname",0,1);});
}
static void test_graphics_emitter() {
    Graphics g(4,3);g.clear(0xff112233);CHECK(g.pixels()[0]==0x112233);
    g.rectangle(3,2,UINT32_MAX,UINT32_MAX,0xff0000);CHECK(g.pixels()[11]==0xff0000&&g.pixels()[10]==0x112233);
    traps([]{Graphics g(0,1);});
    auto module=metavm::Module::compile(".function main 0 1\nconst r0 42\nret r0\n.end\n");
    mvm_error e{};FILE*f=tmpfile();CHECK(f);
    CHECK(vmwv_emit_c(module.get(),"bad-name",f,&e)==MVM_INVALID);
    CHECK(vmwv_emit_c(module.get(),"factory",f,&e)==MVM_OK);CHECK(ftell(f)>100);fclose(f);
}
int main(){try{test_catalog();test_chip8();test_issa();test_jvm_forth();test_graphics_emitter();test_builder();std::cout<<"49 catalog modules and native semantic regressions passed\n";}
 catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
