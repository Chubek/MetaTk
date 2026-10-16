#include "qscript/QScript.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (false)
int main() {
    qscript::Options o;
    int calls=0;
    o.hosts["sum"]={41,2,[&](std::span<const std::int64_t> args) { ++calls; return args[0]+args[1]; }};
    auto unit=qscript::compile_unit("fn f(n) { if (n==0) return 0; return sum(n,f(n-1)); } return f(20);",o);
    CHECK(unit);
    for (bool jit:{false,true}) {
        o.jit=jit;
        o.toolchain.jit.mode=metajit::Mode::Method;
        o.toolchain.jit.method_threshold=1;
        auto result=qscript::run(unit->program,o);
        CHECK(result && result->value==210);
        auto called=qscript::call(*unit,"f",{20},o);
        CHECK(called && called->value==210);
    }
    CHECK(calls==80);
    CHECK(!qscript::run(unit->program));
    CHECK(!qscript::compile("fn sum() { return 0; }",o));
    {
        auto invalid=o;
        invalid.hosts["duplicate"]=invalid.hosts.at("sum");
        CHECK(!qscript::run(unit->program,invalid));
        auto malformed=*unit;
        malformed.program.code[0]={metail::Op::Jmp,0,0,0,INT64_MAX};
        CHECK(!qscript::call(malformed,"f",{1},o));
        malformed=*unit;
        for (auto& f:malformed.functions) if (f.name=="f") f.entry=malformed.program.size();
        CHECK(!qscript::call(malformed,"f",{1},o));
        auto limited=o; limited.toolchain.il.max_instructions=unit->program.size();
        CHECK(!qscript::call(*unit,"f",{1},limited));
    }
    // Optimizations must preserve overflow, traps, effects, PCs and source maps.
    for (auto op:{metail::Op::Add,metail::Op::Sub,metail::Op::Mul,metail::Op::Div,
                  metail::Op::Mod,metail::Op::And,metail::Op::Or,metail::Op::Xor,
                  metail::Op::Shl,metail::Op::Shr,metail::Op::Eq,metail::Op::Lt}) {
        for (std::int64_t rhs:{-1,0,1,63,64}) {
            metail::Program p;
            p.code={{metail::Op::Host,0,0,0,0},{metail::Op::Const,1,0,0,rhs},
                    {op,2,0,1,0},{metail::Op::Print,2,0,0,0},{metail::Op::Halt,2,0,0,0}};
            p.source_map.resize(p.size());
            auto optimized=metail::optimize(p); CHECK(optimized);
            CHECK(optimized.program.size()==p.size() && optimized.program.source_map.size()==p.size());
            for (std::int64_t lhs:std::array<std::int64_t,7>{INT64_MIN,INT64_MAX,-42,-1,0,1,42}) {
                metavm::VMOptions vm;
                int effects=0;
                vm.host_call=[&](auto id,auto args) { CHECK(id==0 && args.size()==32); ++effects; return lhs; };
                auto a=metail::run(p,vm),b=metail::run(optimized.program,vm);
                CHECK(a.status==b.status && a.value==b.value && a.output==b.output && a.steps==b.steps);
                CHECK(effects==2);
            }
        }
    }
    auto raw=metail::parse("host r0, 7\nconst r1, 0\nadd r2, r0, r1\nhalt r2\n"); CHECK(raw);
    auto opt=metail::optimize(raw.program); CHECK(opt && opt.program.code[2].op==metail::Op::Mov);
    auto bytes=metail::encode(raw.program); auto decoded=metail::decode(bytes);
    CHECK(decoded && decoded.program.code==raw.program.code);
    CHECK(metail::parse(metail::emit(raw.program)));
    CHECK(!metail::parse("host r0, -1\n"));
    metavm::VMOptions vm;
    vm.host_call=[](auto,auto)->std::int64_t { throw std::runtime_error("host trap"); };
    auto interpreted=metail::run(raw.program,vm);
    CHECK(!interpreted && interpreted.diagnostics.front().message=="host trap");
    metajit::Options jit; jit.mode=metajit::Mode::Method; jit.method_threshold=1;
    auto compiled=metajit::run(raw.program,jit,vm);
    CHECK(!compiled && compiled.diagnostics.front().message=="host trap");
    vm.max_steps=0;
    CHECK(metail::run(raw.program,vm).status==metavm::Status::StepLimit);
    CHECK(metajit::run(raw.program,jit,vm).status==metavm::Status::StepLimit);
    std::cout << "QScript C++ FFI and optimizer tests passed\n";
}
