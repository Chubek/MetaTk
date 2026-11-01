#include "SECDMachine.hpp"
#include "MetaVM.h"
#include <stdexcept>
#include <string>
#include <utility>

namespace metavm { namespace machines {
namespace {
using M=SECDMachine;using O=M::Op;
[[noreturn]] void fail(size_t block,size_t pc,const std::string&message) {
    throw std::runtime_error("SECD block "+std::to_string(block)+" instruction "+std::to_string(pc)+": "+message);
}
bool terminal(O op){return op==O::ret||op==O::join||op==O::stop;}
struct Continuation {bool call;size_t block,pc,environment,base,height;};
}
void SECDMachine::verify(const Program&p){verify(p,Limits{});}
void SECDMachine::verify(const Program&p,const Limits&limits) {
    if(p.version!=1)throw std::runtime_error("SECD unsupported profile version");
    if(p.blocks.empty()||p.blocks.size()>limits.max_blocks||p.entry>=p.blocks.size())
        throw std::runtime_error("SECD block count or entry invalid");
    size_t count=0;
    for(size_t b=0;b<p.blocks.size();++b) {
        const auto&code=p.blocks[b];
        if(code.empty()||code.size()>limits.max_instructions-count)fail(b,0,"instruction budget or empty block");
        count+=code.size();
        for(size_t pc=0;pc<code.size();++pc) {
            const auto&in=code[pc];
            if(in.op<O::constant||in.op>O::stop)fail(b,pc,"unknown opcode");
            if(in.op!=O::constant&&in.immediate)fail(b,pc,"unexpected immediate");
            if(in.op!=O::load&&in.op!=O::closure&&in.op!=O::select&&in.a)fail(b,pc,"unexpected operand a");
            if(in.op!=O::select&&in.b)fail(b,pc,"unexpected operand b");
            if((in.op==O::closure||in.op==O::select)&&in.a>=p.blocks.size())fail(b,pc,"block target out of range");
            if(in.op==O::select&&in.b>=p.blocks.size())fail(b,pc,"block target out of range");
            if(terminal(in.op)!=(pc+1==code.size()))fail(b,pc,"block must end with exactly one RET, JOIN or STOP");
        }
    }
}
SECDMachine::Result SECDMachine::run(const Program&p){return run(p,Limits{});}
SECDMachine::Result SECDMachine::run(const Program&p,const Limits&limits) {
    verify(p,limits);
    std::vector<Value> stack;
    std::vector<Continuation> dump;
    std::vector<Environment> environments;
    size_t block=p.entry,pc=0,env=empty_environment,base=0;uint64_t steps=0;
    auto tick=[&]{if(steps==limits.max_steps)fail(block,pc,"step limit");++steps;};
    auto need=[&](size_t n){if(stack.size()-base<n)fail(block,pc,"stack underflow");};
    auto push=[&](Value value){if(stack.size()>=limits.max_stack_values)fail(block,pc,"stack value limit");stack.push_back(value);};
    auto save=[&](bool call){
        if(dump.size()>=limits.max_dump)fail(block,pc,"dump limit");
        dump.push_back({call,block,pc+1,env,base,stack.size()});
    };
    auto integer=[&](const Value&value){
        if(!std::holds_alternative<int64_t>(value))fail(block,pc,"expected integer");
        return std::get<int64_t>(value);
    };
    for(;;) {
        tick();const auto&in=p.blocks[block][pc];
        switch(in.op) {
        case O::constant:push(in.immediate);++pc;break;
        case O::load: {
            size_t frame=env;
            if(frame==empty_environment||in.a>=environments[frame].depth)fail(block,pc,"unbound lexical address");
            for(size_t depth=0;depth<in.a;++depth){tick();frame=environments[frame].parent;}
            push(environments[frame].value);++pc;break;
        }
        case O::closure:push(Closure{in.a,env});++pc;break;
        case O::apply: {
            need(2);
            if(!std::holds_alternative<Closure>(stack.back()))fail(block,pc,"AP expects closure above argument");
            auto closure=std::get<Closure>(stack.back());auto argument=stack[stack.size()-2];
            size_t depth=closure.environment==empty_environment?0:environments[closure.environment].depth;
            if(depth>=limits.max_environment_depth)fail(block,pc,"environment depth limit");
            if(environments.size()>=limits.max_environment_cells)fail(block,pc,"environment cell limit");
            stack.resize(stack.size()-2);save(true);
            environments.push_back({argument,closure.environment,depth+1});
            env=environments.size()-1;base=stack.size();block=closure.block;pc=0;break;
        }
        case O::ret: {
            if(dump.empty()||!dump.back().call)fail(block,pc,"RET without call continuation");
            if(stack.size()-base!=1)fail(block,pc,"RET requires exactly one result");
            auto saved=dump.back();dump.pop_back();
            block=saved.block;pc=saved.pc;env=saved.environment;base=saved.base;break;
        }
        case O::select: {
            need(1);auto condition=integer(stack.back());stack.pop_back();save(false);
            block=condition!=0?in.a:in.b;pc=0;break;
        }
        case O::join: {
            if(dump.empty()||dump.back().call)fail(block,pc,"JOIN without selection continuation");
            auto saved=dump.back();
            if(stack.size()<=saved.height||stack.size()-saved.height!=1||base!=saved.base||env!=saved.environment)
                fail(block,pc,"JOIN requires one branch result and unchanged environment");
            dump.pop_back();block=saved.block;pc=saved.pc;break;
        }
        case O::add:case O::sub:case O::mul:case O::div:case O::equal:case O::less_equal: {
            need(2);auto rhs=integer(stack.back()),lhs=integer(stack[stack.size()-2]);int64_t value=0;
            if(in.op==O::equal)value=lhs==rhs;
            else if(in.op==O::less_equal)value=lhs<=rhs;
            else {
                auto op=in.op==O::add?MVM_OP_ADD:in.op==O::sub?MVM_OP_SUB:in.op==O::mul?MVM_OP_MUL:MVM_OP_DIV;
                if(mvm_integer_eval(op,lhs,rhs,&value)!=MVM_OK)fail(block,pc,"integer arithmetic trap");
            }
            stack.pop_back();stack.back()=value;++pc;break;
        }
        case O::stop:
            if(!dump.empty()||base||stack.size()!=1)fail(block,pc,"STOP requires one result and empty dump");
            return {p,stack.back(),std::move(environments),steps};
        }
    }
}
}}
