#include "TracingJIT.hpp"
#include <vmweave/Weave.hpp>
#include <iostream>
#include <stdexcept>
using namespace metavm;
using namespace metajit;
namespace {
std::uint64_t checks=0;
void check(bool v,const char *message) { ++checks;if (!v) throw std::runtime_error(message); }
template<class F> void rejects(F fn) { try { fn(); } catch(const std::exception &) { return; }throw std::runtime_error("expected rejection"); }
void equal(const Frame &a,const Frame &b) {
    check(a.values==b.values,"slot state mismatch");check(a.pc==b.pc,"PC mismatch");
    check(a.steps==b.steps,"step budget mismatch");check(a.status==b.status,"status mismatch");
    check(a.result==b.result && a.trap==b.trap && a.diagnostic==b.diagnostic,"result/trap mismatch");
}
Program loop(Op op,bool alias=false) {
    return Program(5,2,{{Op::Constant,2,0,0,7},{Op::Constant,3,0,0,1},
        {Op::Branch,0,2,3,6},{op,alias?0u:4u,0,1},{Op::Sub,2,2,3},{Op::Jump,0,2},
        {Op::Return,0,alias?0u:4u}});
}
Frame compare(TracingJIT &jit,const std::vector<std::int64_t> &args,std::uint64_t budget,const Host &host={}) {
    auto ref=make_frame(jit.program(),args),actual=ref;
    run(jit.program(),ref,budget,host);jit.run(actual,budget,host);equal(ref,actual);return actual;
}
std::int64_t no_progress(NativeFrame *) { return static_cast<std::int64_t>(NativeExit::Boundary); }
std::int64_t corrupt(NativeFrame *f) { ++f->remaining;return static_cast<std::int64_t>(NativeExit::Budget); }
}
int main() {
    try {
        TraceOptions options;options.hot_threshold=2;
        const std::vector<std::int64_t> values{INT64_MIN,INT64_MIN+1,INT64_MAX,INT64_MAX-1,0,1,-1,2,-2,63,64,-64};
        for (unsigned op=static_cast<unsigned>(Op::Add);op<=static_cast<unsigned>(Op::Lor);++op) {
            for (bool alias:{false,true}) {
                TracingJIT jit(loop(static_cast<Op>(op),alias),options);
                compare(jit,{13,2},1000); // warm a nontrapping trace
                for (auto a:values) for (auto b:values) compare(jit,{a,b},1000);
                for (std::uint64_t budget=0;budget<40;++budget) compare(jit,{-19,3},budget);
                if (TracingJIT::native_supported()) check(jit.stats().native>0 && jit.stats().compiled>0,"no native arithmetic executed");
            }
        }
        auto sum=weave("fn sum(n) { let x=0; loop: br n,body,end; body: let x=x+n; let n=n-1; goto loop; end: return x; }","sum");
        TracingJIT jit(sum,options);auto f=compare(jit,{20},10000);
        check(f.result==210,"wrong sum");
        if (TracingJIT::native_supported()) check(jit.stats().osr_entries>0 && jit.stats().guard_exits>0,"OSR or branch side exit missing");
        TraceOptions immediate=options;immediate.hot_threshold=1;
        TracingJIT returns(sum,immediate);
        compare(returns,{0},100);f=compare(returns,{0},100);
        check(f.status==Status::Returned && f.result==0,"native terminal trace failed");
        if (TracingJIT::native_supported()) check(returns.stats().native>0,"native return path not exercised");
        compare(returns,{4},1000); // guard exits from a terminal specialization
        f.steps=UINT64_MAX;check(returns.run(f,10)==Stop::Returned,"terminal frame unexpectedly overflowed");
        auto alternating=weave("fn f(n) { let x=0; loop: br n,body,end; body: br band(n,1),odd,even; odd: let x=x+11; goto next; even: let x=x-3; next: let n=n-1; goto loop; end: return x; }","f");
        TracingJIT paths(alternating,options);
        compare(paths,{30},10000);
        for (std::uint64_t budget=0;budget<300;++budget) compare(paths,{30},budget);
        if (TracingJIT::native_supported()) check(paths.stats().guard_exits>0,"alternating branch never deoptimized");
        // Every suspension boundary must restore exactly, even halfway through
        // a trace. Resume the same frame one bytecode instruction at a time.
        auto ref=make_frame(sum,{20}),resumed=ref;
        while (ref.status==Status::Running) {
            run(sum,ref,1);jit.run(resumed,1);equal(ref,resumed);
        }
        auto snapshot=DeoptSnapshot(jit.program(),resumed,NativeExit::Returned);
        auto restored=make_frame(sum,{0});snapshot.restore(sum,restored);equal(restored,resumed);
        rejects([&] { snapshot.restore(loop(Op::Add),restored); });
        auto independent=Program::deserialize(sum.serialize());
        rejects([&] { snapshot.restore(independent,restored); });
        rejects([&] { DeoptSnapshot invalid(sum,resumed,NativeExit::Guard); });
        auto p=loop(Op::Div);TracingJIT division(p,options);
        compare(division,{20,2},1000);f=compare(division,{INT64_MIN,-1},1000);
        check(f.trap==Trap::DivisionOverflow,"division did not trap");
        if (TracingJIT::native_supported()) check(division.stats().trap_exits>0,"trap not handled in native code");
        // Host calls split recorded paths. Record call order separately to
        // ensure a guard exit cannot duplicate an already-observed side effect.
        auto calls=weave("fn f(n) { let x=0; loop: br n,body,end; body: let x=x+tick(n); let n=n-1; goto loop; end: return x; }","f");
        TracingJIT effects(calls,options);std::vector<std::int64_t> observed,expected;
        auto host=[&](auto,const auto &args) { observed.push_back(args[0]);return args[0]; };
        auto reference=make_frame(calls,{12});run(calls,reference,10000,[&](auto,const auto &args) { expected.push_back(args[0]);return args[0]; });
        auto actual=effects.execute({12},10000,host);equal(reference,actual);check(observed==expected,"host effects duplicated or reordered");
        if (TracingJIT::native_supported()) check(effects.stats().native>0,"host-boundary trace was not native");
        // Reentry is rejected even from a host callback, and the original engine
        // remains usable after the callback catches the rejection.
        actual=effects.execute({2},10000,[&](auto,const auto &args) {
            rejects([&] { effects.execute({1},10); });return args[0];
        });check(actual.result==3,"callback reentry corrupted execution");
        TraceOptions tiny=options;tiny.max_code_bytes=1;
        TracingJIT bounded(sum,tiny);compare(bounded,{20},10000);check(bounded.stats().compiled==0,"code limit ignored");
        tiny=options;tiny.max_trace_instructions=1;tiny.max_traces=1;
        TracingJIT short_trace(sum,tiny);compare(short_trace,{20},10000);check(short_trace.stats().compiled<=1,"trace count limit ignored");
        tiny=options;tiny.native=false;TracingJIT portable(sum,tiny);compare(portable,{20},10000);
        check(portable.stats().native==0 && portable.stats().compiled==0,"interpreter-only mode compiled code");
        tiny.hot_threshold=0;rejects([&] { TracingJIT invalid(sum,tiny); });
        Program forever(1,0,{{Op::Jump,0,0}});TracingJIT bounded_loop(forever,options);
        auto suspended=compare(bounded_loop,{},1000);
        check(suspended.status==Status::Running && suspended.steps==1000 && suspended.pc==0,"self-loop escaped budget");
        auto state=make_frame(sum,{1});rejects([&] { osr_enter(sum,state,1,10,no_progress); });
        rejects([&] { osr_enter(sum,state,0,10,corrupt); });
        check(state.pc==0 && state.steps==0,"corrupt scalar exit committed");
        std::cout<<checks<<" interpreter/native differential checks passed\n";
    } catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
