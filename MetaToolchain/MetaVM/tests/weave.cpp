#include "vmweave/Weave.hpp"
#include <iostream>
#include <stdexcept>
using namespace metavm;
namespace {
void check(bool v) { if (!v) throw std::runtime_error("weave test failed"); }
template<class F> void rejects(F f) { try { f(); } catch(const std::exception &) { return; }throw std::runtime_error("expected rejection"); }
}
int main() {
    try {
        auto p=weave("fn sum(n) { let x=0; loop: br n,body,end; body: let x=x+n; let n=n-1; goto loop; end: return x; }","sum");
        auto f=make_frame(p,{10});check(run(p,f,1000)==Stop::Returned && f.result==55);
        p=weave("fn f(x) { return select(x,tick(1),tick(2)) + tick(3); }","f");
        for (int x:{0,1}) {
            std::vector<std::int64_t> calls;f=make_frame(p,{x});
            run(p,f,100,[&](auto,const auto &args) { calls.push_back(args.at(0));return args.at(0); });
            check(calls==std::vector<std::int64_t>({x?1:2,3}) && f.result==(x?4:5));
        }
        p=weave("fn f(x) { return select(x,7,div(1,0)); }","f");
        f=make_frame(p,{1});run(p,f,100);check(f.result==7 && f.status==Status::Returned);
        f=make_frame(p,{0});run(p,f,100);check(f.trap==Trap::DivisionByZero);
        p=weave("fn f() { return land(tick(0),tick(1)); }","f");
        std::vector<std::int64_t> eager;f=make_frame(p,{});
        run(p,f,100,[&](auto,const auto &args) { eager.push_back(args[0]);return args[0]; });
        check(eager==std::vector<std::int64_t>({0,1}) && f.result==0);
        p=weave("fn f(c) { br select(c,1,0),a,b; a: return 7; b: return 9; }","f");
        for (int c:{0,1}) { f=make_frame(p,{c});run(p,f,100);check(f.result==(c?7:9)); }
        p=weave("fn f() { return outer(inner(1),inner(2)); }","f");
        eager.clear();f=make_frame(p,{});
        run(p,f,100,[&](std::string_view name,const auto &args)->std::int64_t {
            if (name=="inner") { eager.push_back(args[0]);return args[0]+10; }
            check(args==std::vector<std::int64_t>({11,12}));return 42;
        });check(eager==std::vector<std::int64_t>({1,2}) && f.result==42);
        Limits exact;exact.slots=1;exact.instructions=4;
        p=weave("fn f() { return 7; }","f",{},exact);
        f=make_frame(p,{});run(p,f,10);check(f.result==7);
        exact.instructions=3;rejects([&] { weave("fn f() { return 7; }","f",{},exact); });
        rejects([] { weave("fn f() { return x; }","f"); });
        rejects([] { weave("fn f() { let x=x+1; return x; }","f"); });
        rejects([] { weave("fn f(c) { br c,a,b; a: let x=1; goto end; b: goto end; end: return x; }","f"); });
        rejects([] { weave("fn f() { let x=1; }","f"); });
        rejects([] { weave("fn f() { goto end; end: }","f"); });
        rejects([] { weave("fn f() { assume 1; return 2; }","f"); });
        rejects([] { weave("fn f() { return 1; }","missing"); });
        std::cout<<"MetaIL-to-VM tests passed\n";
    } catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
