#include "ISSA.hpp"
#include "MetaVM.h"
#include <optional>
namespace metavm { namespace machines {
namespace {
bool branch(ISSA::Op op) {
    using O=ISSA::Op;
    return op==O::jump||op==O::branch_equal||op==O::branch_le||
           op==O::branch_ne||op==O::branch_lt||op==O::branch_ge||op==O::branch_gt;
}
bool produces(ISSA::Op op) {return !branch(op)&&op!=ISSA::Op::pfe&&op!=ISSA::Op::ret;}
}
void ISSA::verify(const std::vector<Instruction>&p) {verify(p,Limits{});}
void ISSA::verify(const std::vector<Instruction>&p,const Limits&limits) {
    if(p.empty()||p.size()>limits.max_instructions)throw std::runtime_error("ISSA program size invalid");
    size_t operands=0;
    for(size_t pc=0;pc<p.size();++pc) {
      try {
        const auto&in=p[pc];
        if(in.op<Op::constant||in.op>Op::branch_gt)throw std::runtime_error("ISSA unknown opcode");
        if(in.operands.size()>limits.max_operands-operands)throw std::runtime_error("ISSA operand budget");
        operands+=in.operands.size();
        size_t n=0;
        switch(in.op) {
        case Op::constant:case Op::argument:case Op::jump:case Op::pfe:n=0;break;
        case Op::ret:case Op::array:case Op::length:case Op::negate:n=1;break;
        case Op::set:n=3;break;
        case Op::phi:if(in.operands.empty())throw std::runtime_error("empty phi");n=in.operands.size();break;
        default:n=2;break;
        }
        if(in.operands.size()!=n)throw std::runtime_error("ISSA operand arity");
        for(auto r:in.operands) {
            if(r>=p.size())throw std::runtime_error("ISSA operand out of range");
            if(!produces(p[r].op))throw std::runtime_error("ISSA operand refers to instruction without result");
        }
        if(in.op==Op::argument&&in.immediate<0)throw std::runtime_error("ISSA negative argument index");
        if(pc+1==p.size()&&in.op!=Op::ret&&in.op!=Op::jump)throw std::runtime_error("ISSA fallthrough past program");
        if(branch(in.op)&&in.target>=p.size())
            throw std::runtime_error("ISSA target out of range");
        if(branch(in.op)) {
            if((p[in.target].op==Op::pfe && in.target && p[in.target-1].op==Op::phi) || (p[in.target].op==Op::phi && in.target && p[in.target-1].op==Op::phi))
                throw std::runtime_error("ISSA branch enters middle of phi group");
        }
      } catch(const std::runtime_error&e) {throw VerificationError(pc,e.what());}
    }
    for(size_t k=0;k<p.size();++k)if(p[k].op==Op::phi) {
        size_t end=k;
        while(end<p.size()&&p[end].op==Op::phi)++end;
        if(end==p.size()||p[end].op!=Op::pfe)throw VerificationError(k,"phi group needs pfe");
        k=end;
    }
}
ISSA::Result ISSA::run(const std::vector<Instruction>&p,const std::vector<int64_t>&args,uint64_t fuel) {
    Limits limits;limits.max_steps=fuel;return run_limited(p,limits,args);
}
ISSA::Result ISSA::run_limited(const std::vector<Instruction>&p,const Limits&limits,const std::vector<int64_t>&args) {
    verify(p,limits);
    std::vector<std::optional<Value>> rr(p.size()),pending(p.size());
    std::vector<size_t> staged;
    size_t pc=0,cen=0,allocated_elements=0,allocations=0; uint64_t steps=0;
    auto charge=[&](size_t n){
        if(allocations==limits.max_array_allocations)throw std::runtime_error("ISSA array allocation count limit");
        if(n>limits.max_allocated_elements-allocated_elements)throw std::runtime_error("ISSA allocation budget");
        allocated_elements+=n;++allocations;
    };
    auto value=[&](size_t r)->Value { if(!rr[r])throw std::runtime_error("ISSA undefined result");return *rr[r]; };
    auto scalar=[&](size_t r)->int64_t {
        Value v=value(r);if(!std::holds_alternative<int64_t>(v))throw std::runtime_error("ISSA expected scalar");return std::get<int64_t>(v);
    };
    auto arr=[&](size_t r)->Array {
        Value v=value(r);if(!std::holds_alternative<Array>(v))throw std::runtime_error("ISSA expected array");return std::get<Array>(v);
    };
    for(;;) {
        if(steps==limits.max_steps)throw std::runtime_error("ISSA fuel exhausted");
        ++steps;
        if(pc>=p.size())throw std::runtime_error("ISSA fell off program");
        const auto&in=p[pc]; const auto&o=in.operands; size_t next=pc+1;
        if(!staged.empty()&&in.op!=Op::phi&&in.op!=Op::pfe)throw std::runtime_error("ISSA phi group needs pfe");
        switch(in.op) {
        case Op::constant:rr[pc]=in.immediate;break;
        case Op::argument:
            if(in.immediate<0||uint64_t(in.immediate)>=args.size())throw std::runtime_error("ISSA argument index");
            rr[pc]=args[size_t(in.immediate)];break;
        case Op::add:case Op::sub:case Op::mul:case Op::div: {
            int64_t result; mvm_opcode op=in.op==Op::add?MVM_OP_ADD:in.op==Op::sub?MVM_OP_SUB:in.op==Op::mul?MVM_OP_MUL:MVM_OP_DIV;
            if(mvm_integer_eval(op,scalar(o[0]),scalar(o[1]),&result)!=MVM_OK)throw std::runtime_error("ISSA integer arithmetic trap");
            rr[pc]=result;break;
        }
        case Op::equal:rr[pc]=int64_t(scalar(o[0])==scalar(o[1]));break;
        case Op::less_equal:rr[pc]=int64_t(scalar(o[0])<=scalar(o[1]));break;
        case Op::logical_and:case Op::logical_or: {
            // The paper's AND/OR are logical, not bitwise. Check both operands.
            auto a=scalar(o[0]),b=scalar(o[1]);
            rr[pc]=int64_t(in.op==Op::logical_and?(a!=0&&b!=0):(a!=0||b!=0));break;
        }
        case Op::negate: {
            int64_t result;
            if(mvm_integer_eval(MVM_OP_SUB,0,scalar(o[0]),&result)!=MVM_OK)throw std::runtime_error("ISSA integer arithmetic trap");
            rr[pc]=result;break;
        }
        case Op::jump:next=in.target;cen=in.edge;break;
        case Op::branch_equal:case Op::branch_le:case Op::branch_ne:case Op::branch_lt:case Op::branch_ge:case Op::branch_gt: {
            auto a=scalar(o[0]),b=scalar(o[1]);
            bool taken=in.op==Op::branch_equal?a==b:in.op==Op::branch_le?a<=b:
                in.op==Op::branch_ne?a!=b:in.op==Op::branch_lt?a<b:in.op==Op::branch_ge?a>=b:a>b;
            if(taken){next=in.target;cen=in.edge;}else cen=0;
            break;
        }
        case Op::phi:
            if(cen>=o.size())throw std::runtime_error("ISSA edge exceeds phi arity");
            pending[pc]=value(o[cen]);staged.push_back(pc);break;
        case Op::pfe:
            for(auto r:staged){rr[r]=std::move(pending[r]);pending[r].reset();}staged.clear();cen=0;break;
        case Op::array: {
            auto n=scalar(o[0]);if(n<0||uint64_t(n)>limits.max_array_elements)throw std::runtime_error("ISSA array allocation limit");
            charge(size_t(n));
            rr[pc]=Array(std::make_shared<const std::vector<int64_t>>(size_t(n),0));break;
        }
        case Op::length:rr[pc]=int64_t(arr(o[0])->size());break;
        case Op::get:case Op::set: {
            auto a=arr(o[0]);auto idx=scalar(o[1]);if(idx<0||uint64_t(idx)>=a->size())throw std::runtime_error("ISSA array bounds");
            if(in.op==Op::get)rr[pc]=(*a)[size_t(idx)];
            else {charge(a->size());auto copy=std::make_shared<std::vector<int64_t>>(*a);(*copy)[size_t(idx)]=scalar(o[2]);rr[pc]=Array(copy);}
            break;
        }
        case Op::ret:return {value(o[0]),steps};
        }
        pc=next;
    }
}
}}
