#include "KrivineMachine.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace metavm { namespace machines {
namespace {
[[noreturn]] void fail(size_t term,const std::string&message) {
    throw std::runtime_error("Krivine term "+std::to_string(term)+": "+message);
}
}
void KrivineMachine::verify(const Program&p){verify(p,Limits{});}
void KrivineMachine::verify(const Program&p,const Limits&limits) {
    if(p.version!=1)throw std::runtime_error("Krivine unsupported profile version");
    if(p.terms.empty()||p.terms.size()>limits.max_terms||p.root>=p.terms.size())
        throw std::runtime_error("Krivine term count or root invalid");
    std::vector<size_t> required(p.terms.size());
    for(size_t n=0;n<p.terms.size();++n) {
        const auto&t=p.terms[n];
        switch(t.op) {
        case Op::variable:
            if(t.b||t.a==SIZE_MAX)fail(n,"invalid variable index or unused operand");
            required[n]=t.a+1;break;
        case Op::abstraction:
            if(t.a>=n||t.b)fail(n,"body must precede abstraction; unused operand must be zero");
            required[n]=required[t.a]?required[t.a]-1:0;break;
        case Op::application:
            if(t.a>=n||t.b>=n)fail(n,"children must precede application");
            required[n]=std::max(required[t.a],required[t.b]);break;
        default:fail(n,"unknown opcode");
        }
    }
    if(required[p.root])fail(p.root,"root is not closed");
}
KrivineMachine::Result KrivineMachine::run(const Program&p){return run(p,Limits{});}
KrivineMachine::Result KrivineMachine::run(const Program&p,const Limits&limits) {
    verify(p,limits);
    std::vector<Closure> stack;
    std::vector<Environment> environments;
    size_t term=p.root,env=empty_environment;uint64_t steps=0;
    auto tick=[&]{if(steps==limits.max_steps)fail(term,"step limit");++steps;};
    for(;;) {
        tick();const auto&t=p.terms[term];
        switch(t.op) {
        case Op::application:
            if(stack.size()>=limits.max_stack)fail(term,"argument stack limit");
            stack.push_back({t.b,env});term=t.a;break;
        case Op::abstraction: {
            if(stack.empty())return {p,{term,env},std::move(environments),steps};
            size_t depth=env==empty_environment?0:environments[env].depth;
            if(depth>=limits.max_environment_depth)fail(term,"environment depth limit");
            if(environments.size()>=limits.max_environment_cells)fail(term,"environment cell limit");
            environments.push_back({stack.back(),env,depth+1});stack.pop_back();
            env=environments.size()-1;term=t.a;break;
        }
        case Op::variable: {
            size_t frame=env;
            if(frame==empty_environment||t.a>=environments[frame].depth)fail(term,"unbound variable");
            // Each skipped environment cell is a Succ transition, charged to fuel.
            for(size_t n=0;n<t.a;++n){tick();frame=environments[frame].parent;}
            auto closure=environments[frame].value;term=closure.term;env=closure.environment;break;
        }
        }
    }
}
}}
