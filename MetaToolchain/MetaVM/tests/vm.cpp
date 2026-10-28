#include "vmspec/VM.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace metavm;
namespace {
void check(bool b) { if (!b) throw std::runtime_error("VM test failed"); }
template<class F> void rejects(F fn) { try { fn(); } catch (const std::exception &) { return; } throw std::runtime_error("expected rejection"); }
Program binary(Op op) { return Program(3,2,{{op,2,0,1},{Op::Return,0,2}}); }
Frame execute(const Program &p,std::vector<std::int64_t> args) { auto f=make_frame(p,args);run(p,f,100);return f; }
}
int main() {
    try {
        // Independent wire anchor: fixed v1 header, Constant(-1), Return r0.
        const std::vector<std::uint8_t> anchor{
            'M','V','M',0, 1,0,0,0, 1,0,0,0, 0,0,0,0, 2,0,0,0, 0,0,0,0,
            0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
            255,255,255,255,255,255,255,255,
            26,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0,0,0,0};
        Program anchor_program(1,0,{{Op::Constant,0,0,0,-1},{Op::Return,0,0}});
        check(anchor_program.serialize()==anchor);
        auto anchored=Program::deserialize(anchor);
        check(execute(anchored,{}).result==-1);
        check(execute(binary(Op::Add),{INT64_MAX,1}).result==INT64_MIN);
        check(execute(binary(Op::Sub),{INT64_MIN,1}).result==INT64_MAX);
        check(execute(binary(Op::Mul),{INT64_MAX,2}).result==-2);
        check(execute(binary(Op::Div),{-9,2}).result==-4);
        check(execute(binary(Op::Mod),{-9,2}).result==-1);
        check(execute(binary(Op::Shr),{-9,1}).result==-5);
        for (Op op:{Op::Div,Op::Mod}) {
            auto f=execute(binary(op),{INT64_MIN,-1});check(f.trap==Trap::DivisionOverflow && f.pc==0 && f.steps==1);
            check(execute(binary(op),{1,0}).trap==Trap::DivisionByZero);
        }
        for (Op op:{Op::Shl,Op::Shr}) for (std::int64_t count:{-1,64})
            check(execute(binary(op),{1,count}).trap==Trap::InvalidShift);
        // sum(n..1), including resumable budget exhaustion and a backward branch.
        Program sum(4,1,{{Op::Constant,1,0,0,0},{Op::Constant,2,0,0,1},
            {Op::Branch,0,0,3,6},{Op::Add,1,1,0},{Op::Sub,0,0,2},{Op::Jump,0,2},{Op::Return,0,1}});
        auto f=make_frame(sum,{10});check(run(sum,f,0)==Stop::Budget && f.pc==0);
        check(run(sum,f,3)==Stop::Budget && f.pc==3);
        check(run(sum,f,100)==Stop::Returned && f.result==55 && f.steps==44);
        check(run(sum,f,100)==Stop::Returned && f.steps==44);
        auto wire=sum.serialize();check(Program::deserialize(wire).serialize()==wire);
        auto independent=Program::deserialize(wire);
        rejects([&] { run(independent,f,1); }); // identical bytes, different identity
        for (std::size_t n=0;n<wire.size();++n) {
            auto short_wire=wire;short_wire.resize(n);rejects([&] { Program::deserialize(short_wire); });
        }
        auto bad=wire;bad[4]=2;rejects([&] { Program::deserialize(bad); });
        bad=wire;bad.push_back(0);rejects([&] { Program::deserialize(bad); });
        bad=wire;bad[8]=255;bad[9]=255;rejects([&] { Program::deserialize(bad); });
        Limits small;small.input_bytes=1;rejects([&] { Program::deserialize(wire,small); });
        rejects([] { Program(1,0,{{Op::Add,1,0,0},{Op::Return,0,0}}); });
        rejects([] { Program(1,0,{{Op::Jump,0,1}}); });
        rejects([] { Program(1,0,{{Op::Constant,0}}); });
        rejects([] { Program(1,0,{{static_cast<Op>(100),0}}); });
        Program call(2,1,{{Op::Call,1,0,0,1},{Op::Return,0,1}},{"tick"});
        auto c=make_frame(call,{7});int calls=0;
        run(call,c,5,[&](std::string_view name,const auto &args) { ++calls;check(name=="tick");return args.at(0)+1; });
        check(calls==1 && c.result==8);
        check(execute(call,{7}).trap==Trap::MissingHost);
        c=make_frame(call,{7});run(call,c,5,[](auto,const auto &)->std::int64_t { throw std::runtime_error("host failure"); });
        check(c.trap==Trap::HostFailure && c.diagnostic=="host failure" && c.pc==0);
        rejects([&] { make_frame(call,{}); });
        c=make_frame(call,{7});c.pc=99;rejects([&] { run(call,c,1); });
        c=make_frame(call,{7});rejects([&] { run(sum,c,1); });
        c=make_frame(call,{7});
        run(call,c,10,[&](auto,const auto &args) {
            rejects([&] { run(call,c,1); });return args[0];
        });check(c.result==7 && !c.active);
        c=make_frame(call,{7});
        rejects([&] { run(call,c,10,[](auto,const auto &)->std::int64_t { throw std::bad_alloc(); }); });
        check(c.status==Status::Trapped && c.trap==Trap::HostFailure && !c.active);
        c=make_frame(call,{7});c.steps=UINT64_MAX;rejects([&] { run(call,c,1); });
        // Deterministic hostile wire mutations: any accepted document must
        // round-trip and execute within a finite budget without invalid access.
        std::uint64_t seed=0x375298;
        for (unsigned test=0;test<2000;++test) {
            seed=seed*6364136223846793005ULL+1;
            auto mutated=wire;mutated[static_cast<std::size_t>(seed%mutated.size())]^=static_cast<std::uint8_t>(seed>>32);
            try {
                auto candidate=Program::deserialize(mutated);
                auto frame=make_frame(candidate,std::vector<std::int64_t>(candidate.parameters()));
                run(candidate,frame,100);check(candidate.serialize()==mutated);
            } catch (const std::invalid_argument &) { }
        }
        std::cout << "versioned VM tests passed\n";
    } catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
