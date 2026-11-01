#include "VMLibrary/SECD/SECDMachine.hpp"
#include "VMLibrary/Krivine/KrivineMachine.hpp"
#include <iostream>
#include <functional>
#include <stdexcept>
#include <string>

#define CHECK(x) do{if(!(x))throw std::runtime_error("check failed: " #x);}while(0)
using S=metavm::machines::SECDMachine;
using K=metavm::machines::KrivineMachine;
template<class F> void rejects(F f,const std::string&message) {
    try{f();}catch(const std::runtime_error&e){CHECK(std::string(e.what()).find(message)!=std::string::npos);return;}
    throw std::runtime_error("expected rejection: "+message);
}
static int64_t integer(const S::Program&p){return std::get<int64_t>(S::run(p).value);}
static void secd() {
    using O=S::Op;
    S::Program identity{1,{{{O::constant,42},{O::closure,0,1},{O::apply},{O::stop}},{{O::load},{O::ret}}},0};
    CHECK(integer(identity)==42);
    // ((lambda x. lambda y. x+y) 40) 2; the outer closure escapes its call.
    S::Program capture{1,{{{O::constant,2},{O::constant,40},{O::closure,0,1},{O::apply},{O::apply},{O::stop}},
        {{O::closure,0,2},{O::ret}},{{O::load,0,1},{O::load},{O::add},{O::ret}}},0};
    CHECK(integer(capture)==42);
    CHECK(S::run(capture).steps==13); // Includes one skipped lexical ancestor.
    auto shadow=capture;shadow.blocks[2]={{O::load},{O::ret}};CHECK(integer(shadow)==2);
    // Independent runs capture their own values without shared mutable state.
    for(int64_t n=-12;n<=12;++n){auto p=capture;p.blocks[0][1].immediate=n;CHECK(integer(p)==n+2);}
    auto owned=S::run(S::Program{1,{{{O::constant,40},{O::closure,0,1},{O::apply},{O::stop}},
        {{O::closure,0,2},{O::ret}},{{O::load,0,1},{O::ret}}},0});
    auto closure=std::get<S::Closure>(owned.value);
    CHECK(closure.block==2&&owned.program.blocks.size()==3);
    CHECK(std::get<int64_t>(owned.environments.at(closure.environment).value)==40);
    for(int64_t condition:{int64_t(0),int64_t(1),int64_t(-1)}) {
        S::Program p{1,{{{O::constant,condition},{O::select,0,1,2},{O::stop}},
            {{O::constant,7},{O::join}},{{O::constant,9},{O::join}}},0};
        CHECK(integer(p)==(condition?7:9));
    }
    // SELECT inside a function, AP inside a selected arm: typed dump restoration.
    S::Program nested{1,{{{O::constant,1},{O::closure,0,1},{O::apply},{O::stop}},
        {{O::load},{O::select,0,2,3},{O::ret}},
        {{O::constant,42},{O::closure,0,4},{O::apply},{O::join}},
        {{O::constant,0},{O::join}},{{O::load},{O::ret}}},0};
    CHECK(integer(nested)==42);
    struct Arithmetic {O op;int64_t a,b,result;};
    for(auto t:{Arithmetic{O::add,INT64_MAX,1,INT64_MIN},Arithmetic{O::sub,INT64_MIN,1,INT64_MAX},
        Arithmetic{O::mul,INT64_MIN,-1,INT64_MIN},Arithmetic{O::div,INT64_MIN,-1,INT64_MIN},
        Arithmetic{O::sub,7,3,4},Arithmetic{O::div,-7,3,-2},Arithmetic{O::equal,4,4,1},
        Arithmetic{O::equal,4,5,0},Arithmetic{O::less_equal,-1,0,1},Arithmetic{O::less_equal,7,3,0}})
        CHECK(integer({1,{{{O::constant,t.a},{O::constant,t.b},{t.op},{O::stop}}},0})==t.result);
    rejects([]{integer({1,{{{O::constant,1},{O::constant,0},{O::div},{O::stop}}},0});},"arithmetic trap");
    rejects([]{integer({1,{{{O::closure,0,0},{O::constant,1},{O::add},{O::stop}}},0});},"expected integer");
    rejects([]{integer({1,{{{O::constant,1},{O::constant,2},{O::apply},{O::stop}}},0});},"expects closure");
    rejects([]{integer({1,{{{O::load},{O::stop}}},0});},"unbound lexical");
    rejects([]{integer({1,{{{O::add},{O::stop}}},0});},"stack underflow");
    rejects([]{integer({1,{{{O::ret}}},0});},"RET without");
    rejects([]{integer({1,{{{O::join}}},0});},"JOIN without");
    auto bad=identity;bad.version=2;rejects([&]{S::verify(bad);},"version");
    bad=identity;bad.entry=2;rejects([&]{S::verify(bad);},"entry");
    bad=identity;bad.blocks[1].clear();rejects([&]{S::verify(bad);},"empty block");
    bad=identity;bad.blocks[0][1].a=99;rejects([&]{S::verify(bad);},"target");
    bad=identity;bad.blocks[1][0].op=O(999);rejects([&]{S::verify(bad);},"unknown opcode");
    bad=identity;bad.blocks.push_back({{O(999)},{O::stop}});rejects([&]{S::verify(bad);},"unknown opcode");
    bad=identity;bad.blocks[1][0].b=1;rejects([&]{S::verify(bad);},"unexpected operand");
    bad=identity;bad.blocks[1][0].immediate=1;rejects([&]{S::verify(bad);},"unexpected immediate");
    bad=identity;bad.blocks[1].pop_back();rejects([&]{S::verify(bad);},"block must end");
    bad=identity;bad.blocks[1][0].op=O::ret;rejects([&]{S::verify(bad);},"block must end");
    bad=identity;bad.blocks[1]={{O::constant,1},{O::constant,2},{O::ret}};
    rejects([&]{S::run(bad);},"exactly one result");
    bad=nested;bad.blocks[2].back().op=O::ret;rejects([&]{S::run(bad);},"RET without");
    bad=identity;bad.blocks[1].back().op=O::join;rejects([&]{S::run(bad);},"JOIN without");
    bad=nested;bad.blocks[2]={{O::join}};rejects([&]{S::run(bad);},"one branch result");
    bad=nested;bad.blocks[1][1].b=SIZE_MAX;rejects([&]{S::verify(bad);},"target");
    bad=identity;bad.blocks[1].back().op=O::stop;rejects([&]{S::run(bad);},"empty dump");
    S::Limits limits;limits.max_blocks=2;limits.max_instructions=6;limits.max_stack_values=2;
    limits.max_dump=1;limits.max_environment_cells=1;limits.max_environment_depth=1;limits.max_steps=6;
    CHECK(std::get<int64_t>(S::run(identity,limits).value)==42);
    for(unsigned field=0;field<7;++field) {
        auto bound=limits;
        switch(field){case 0:--bound.max_blocks;break;case 1:--bound.max_instructions;break;
        case 2:--bound.max_stack_values;break;case 3:--bound.max_dump;break;
        case 4:--bound.max_environment_cells;break;case 5:--bound.max_environment_depth;break;case 6:--bound.max_steps;break;}
        rejects([&]{S::run(identity,bound);},field==0?"block count":field==1?"instruction budget":"limit");
    }
    // Self-application diverges without recursive environment cells or host recursion.
    S::Program omega{1,{{{O::closure,0,1},{O::closure,0,1},{O::apply},{O::stop}},
        {{O::load},{O::load},{O::apply},{O::ret}}},0};
    limits=S::Limits{};limits.max_steps=100;rejects([&]{S::run(omega,limits);},"step limit");
    limits=S::Limits{};limits.max_dump=3;rejects([&]{S::run(omega,limits);},"dump limit");
    limits=S::Limits{};limits.max_environment_cells=3;rejects([&]{S::run(omega,limits);},"cell limit");
    limits=S::Limits{};limits.max_environment_depth=1;rejects([&]{S::run(capture,limits);},"depth limit");
}
static void krivine() {
    using O=K::Op;
    // 0=#0, 1=I, 2=#1, 3=lambda.#1, 4=K, 5=#0 #0, 6=lambda.(#0 #0), 7=Omega.
    K::Program p{1,{{O::variable,0},{O::abstraction,0},{O::variable,1},{O::abstraction,2},
        {O::abstraction,3},{O::application,0,0},{O::abstraction,5},{O::application,6,6}},1};
    CHECK(K::run(p).value.term==1);
    auto apply=[&](size_t f,size_t a){p.terms.push_back({O::application,f,a});return p.terms.size()-1;};
    p.root=apply(apply(1,1),1);CHECK(K::run(p).value.term==1);CHECK(K::run(p).steps==7);
    p.root=apply(apply(4,1),7);CHECK(K::run(p).value.term==1); // K I Omega ignores divergent argument.
    p.root=apply(apply(4,1),4);CHECK(K::run(p).value.term==1); // Lexical capture.
    CHECK(K::run(p).steps==7); // Includes the variable-one Succ transition.
    // lambda.lambda.#0 chooses the most recent binding.
    p.terms.push_back({O::abstraction,1});size_t second=p.terms.size()-1;
    p.root=apply(apply(second,1),4);CHECK(K::run(p).value.term==4);
    p.root=apply(4,1);auto capture=K::run(p);
    CHECK(capture.value.term==3&&capture.value.environment!=K::empty_environment);
    CHECK(capture.environments.at(capture.value.environment).value.term==1);
    // An abstraction containing Omega is already WHNF; its body is not reduced.
    p.terms.push_back({O::abstraction,7});p.root=p.terms.size()-1;
    CHECK(K::run(p).steps==1);
    auto owned=K::run(K::Program{1,{{O::variable,0},{O::abstraction,0}},1});
    CHECK(owned.program.terms.at(owned.value.term).op==O::abstraction);
    K::Program identity{1,{{O::variable,0},{O::abstraction,0},{O::application,1,1}},2};
    K::Limits limits;limits.max_terms=3;limits.max_stack=1;limits.max_environment_cells=1;
    limits.max_environment_depth=1;limits.max_steps=4;
    CHECK(K::run(identity,limits).value.term==1);
    for(unsigned field=0;field<5;++field) {
        auto bound=limits;
        switch(field){case 0:--bound.max_terms;break;case 1:--bound.max_stack;break;
        case 2:--bound.max_environment_cells;break;case 3:--bound.max_environment_depth;break;case 4:--bound.max_steps;break;}
        rejects([&]{K::run(identity,bound);},field==0?"term count":"limit");
    }
    p.root=7;limits=K::Limits{};limits.max_steps=100;rejects([&]{K::run(p,limits);},"step limit");
    limits=K::Limits{};limits.max_environment_cells=3;rejects([&]{K::run(p,limits);},"cell limit");
    p.root=apply(apply(4,1),1);limits=K::Limits{};limits.max_environment_depth=1;
    rejects([&]{K::run(p,limits);},"depth limit");
    limits=K::Limits{};limits.max_stack=1;rejects([&]{K::run(p,limits);},"stack limit");
    auto bad=identity;bad.version=2;rejects([&]{K::verify(bad);},"version");
    bad=identity;bad.root=99;rejects([&]{K::verify(bad);},"root");
    bad=identity;bad.root=0;rejects([&]{K::verify(bad);},"not closed");
    bad=identity;bad.terms[0].a=SIZE_MAX;rejects([&]{K::verify(bad);},"variable index");
    bad=identity;bad.terms[1].a=1;rejects([&]{K::verify(bad);},"precede");
    bad=identity;bad.terms[2].b=2;rejects([&]{K::verify(bad);},"precede");
    bad=identity;bad.terms[0].op=O(999);rejects([&]{K::verify(bad);},"unknown opcode");
    bad=identity;bad.terms.push_back({O(999)});rejects([&]{K::verify(bad);},"unknown opcode");
    bad=identity;bad.terms[1].b=1;rejects([&]{K::verify(bad);},"unused operand");
    // Deep syntax validates and destroys iteratively, even with a tiny execution budget.
    K::Program deep{1,{{O::variable,0}},0};
    for(size_t n=0;n<50000;++n)deep.terms.push_back({O::abstraction,n});
    deep.root=deep.terms.size()-1;limits=K::Limits{};limits.max_steps=1;
    CHECK(K::run(deep,limits).value.term==deep.root);
}
static void differential() {
    // A test-only compiler from a small, terminating pure-lambda family to the
    // unary SECD profile. Compare two independent execution strategies against
    // known selector results; do not compare transition counts across machines.
    using T=K::Op;using O=S::Op;
    for(unsigned seed=0;seed<=64;++seed) {
        K::Program p{1,{{T::variable,0},{T::abstraction,0},{T::variable,1},
            {T::abstraction,2},{T::abstraction,3},{T::abstraction,1}},1}; // I=1, K=4, second=5
        auto app=[&](size_t f,size_t a){p.terms.push_back({T::application,f,a});return p.terms.size()-1;};
        size_t expected=seed&1?1:4,term=expected;
        for(unsigned depth=0;depth<5;++depth) {
            size_t other=(seed>>(depth+1))&1?1:4;
            switch((seed+depth)%3) {
            case 0:term=app(1,term);break;
            case 1:term=app(app(4,term),other);break;
            case 2:term=app(app(5,term),other);expected=other;break;
            }
        }
        if(seed==64) {
            // (K I) Omega terminates by name but diverges in the strict compiler.
            auto self=app(0,0);p.terms.push_back({T::abstraction,self});size_t delta=p.terms.size()-1;
            auto omega=app(delta,delta);term=app(app(4,1),omega);expected=1;
        }
        p.root=term;CHECK(K::run(p).value.term==expected);
        S::Program code;code.blocks.emplace_back();
        std::vector<size_t> bodies(p.terms.size(),SIZE_MAX);
        std::function<void(size_t,size_t)> emit=[&](size_t t,size_t b) {
            const auto&node=p.terms[t];
            if(node.op==T::variable)code.blocks[b].push_back({O::load,0,node.a});
            else if(node.op==T::application){emit(node.b,b);emit(node.a,b);code.blocks[b].push_back({O::apply});}
            else {
                if(bodies[t]==SIZE_MAX){size_t body=code.blocks.size();bodies[t]=body;code.blocks.emplace_back();emit(node.a,body);code.blocks[body].push_back({O::ret});}
                code.blocks[b].push_back({O::closure,0,bodies[t]});
            }
        };
        emit(p.root,0);code.blocks[0].push_back({O::stop});
        if(seed==64) {
            S::Limits limits;limits.max_steps=100;rejects([&]{S::run(code,limits);},"step limit");
        } else {
            auto result=S::run(code);CHECK(std::get<S::Closure>(result.value).block==bodies[expected]);
        }
    }
}
int main(){try{secd();krivine();differential();std::cout<<"SECD and Krivine lexical closures, evaluation and resource regressions passed\n";}
catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
