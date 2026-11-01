#include "ISSA.hpp"
#include <deque>
#include <map>
#include <utility>

namespace metavm { namespace machines {
namespace {
using I=ISSA;
using O=I::Op;
// A may-set: undefined must survive a join with a defined predecessor.
constexpr uint8_t undefined=1,scalar=2,array=4;
bool conditional(O op) {
    return op==O::branch_equal||op==O::branch_le||op==O::branch_ne||
           op==O::branch_lt||op==O::branch_ge||op==O::branch_gt;
}
I::ValueKind kind(uint8_t facts) {
    return facts==(scalar|array)?I::ValueKind::either:
        facts==scalar?I::ValueKind::scalar:facts==array?I::ValueKind::array:I::ValueKind::none;
}
struct Block {size_t begin,end;};
struct State {
    size_t block,cen;
    std::vector<uint8_t> facts;
    bool queued=true;
};
}

ISSA::Analysis ISSA::analyze(const std::vector<Instruction>&program) {
    return analyze(program,Limits{},AnalysisLimits{});
}
ISSA::Analysis ISSA::analyze(const std::vector<Instruction>&p,const Limits&limits,const AnalysisLimits&budget) {
    verify(p,limits);
    const size_t n=p.size();
    Analysis result;
    size_t pc=0;
    auto charge=[&](uint64_t amount) {
        if(amount>budget.max_work-result.work)throw VerificationError(pc,"ISSA analysis work limit");
        result.work+=amount;
    };
    // Preflight the initial fact table before allocating analysis storage.
    if(!budget.max_states)throw VerificationError(0,"ISSA analysis state limit");
    if(n>budget.max_cells)throw VerificationError(0,"ISSA analysis cell limit");
    charge(n);
    std::vector<bool> leader(n,false);
    leader[0]=true;
    for(size_t k=0;k<n;++k) {
        auto op=p[k].op;
        if(op==O::jump||conditional(op))leader[p[k].target]=true;
        if((op==O::jump||conditional(op)||op==O::ret)&&k+1<n)leader[k+1]=true;
    }
    charge(n);
    std::vector<Block> blocks;
    std::vector<size_t> block_of(n);
    for(size_t k=0;k<n;++k) {
        if(leader[k]) {
            if(!blocks.empty())blocks.back().end=k;
            blocks.push_back({k,n});
        }
        block_of[k]=blocks.size()-1;
    }
    result.reachable.assign(n,false);
    std::vector<uint8_t> produced(n,0);
    uint8_t returned=0;
    std::vector<State> states;
    std::map<std::pair<size_t,size_t>,size_t> indices;
    std::deque<size_t> worklist;
    auto merge=[&](size_t block,size_t cen,const std::vector<uint8_t>&facts) {
        auto key=std::make_pair(block,cen);
        auto found=indices.find(key);
        if(found==indices.end()) {
            if(states.size()>=budget.max_states)throw VerificationError(pc,"ISSA analysis state limit");
            // Division avoids overflow in (states.size()+1)*n.
            if(states.size()>=budget.max_cells/n)throw VerificationError(pc,"ISSA analysis cell limit");
            charge(n);
            size_t id=states.size();
            states.push_back({block,cen,facts,true});
            indices.emplace(key,id);worklist.push_back(id);
        } else {
            charge(n);
            auto&state=states[found->second];bool changed=false;
            for(size_t r=0;r<n;++r) {
                auto combined=uint8_t(state.facts[r]|facts[r]);
                if(combined!=state.facts[r]){state.facts[r]=combined;changed=true;}
            }
            if(changed&&!state.queued){state.queued=true;worklist.push_back(found->second);}
        }
    };
    charge(n);
    merge(0,0,std::vector<uint8_t>(n,undefined));
    while(!worklist.empty()) {
        size_t id=worklist.front();worklist.pop_front();states[id].queued=false;
        auto block=blocks[states[id].block];size_t cen=states[id].cen;
        pc=block.begin;charge(n);
        // Do not retain references to states across merge(), which can grow it.
        auto facts=states[id].facts;
        bool terminated=false;
        auto read=[&](size_t operand,uint8_t expected) {
            charge(1);
            size_t r=p[pc].operands[operand];uint8_t actual=facts[r];
            std::string where=" at operand "+std::to_string(operand)+" ("+std::to_string(r)+")";
            if(actual&undefined)throw VerificationError(pc,"ISSA possibly undefined result"+where);
            if(actual&uint8_t(~expected))throw VerificationError(pc,
                std::string("ISSA expected ")+(expected==scalar?"scalar":"array")+where);
            return actual;
        };
        auto write=[&](uint8_t value){facts[pc]=value;produced[pc]|=value;};
        while(pc<block.end) {
            charge(1);result.reachable[pc]=true;
            const auto&in=p[pc];
            switch(in.op) {
            case O::constant:case O::argument:write(scalar);break;
            case O::add:case O::sub:case O::mul:case O::div:
            case O::equal:case O::less_equal:case O::logical_and:case O::logical_or:
                read(0,scalar);read(1,scalar);write(scalar);break;
            case O::negate:read(0,scalar);write(scalar);break;
            case O::array:read(0,scalar);write(array);break;
            case O::get:read(0,array);read(1,scalar);write(scalar);break;
            case O::set:read(0,array);read(1,scalar);read(2,scalar);write(array);break;
            case O::length:read(0,array);write(scalar);break;
            case O::phi: {
                size_t first=pc;
                std::vector<uint8_t> staged;
                // Structural verification prevents block boundaries inside this
                // group. Every read sees the old committed register table.
                do {
                    if(pc!=first){charge(1);result.reachable[pc]=true;}
                    if(cen>=p[pc].operands.size())throw VerificationError(pc,
                        "ISSA CEN "+std::to_string(cen)+" exceeds PHI arity");
                    staged.push_back(read(cen,scalar|array));++pc;
                }while(p[pc].op==O::phi);
                charge(1);result.reachable[pc]=true; // PFE
                for(size_t k=0;k<staged.size();++k) {
                    facts[first+k]=staged[k];produced[first+k]|=staged[k];
                }
                cen=0;break;
            }
            case O::pfe:cen=0;break;
            case O::jump:merge(block_of[in.target],in.edge,facts);terminated=true;break;
            case O::branch_equal:case O::branch_le:case O::branch_ne:
            case O::branch_lt:case O::branch_ge:case O::branch_gt:
                read(0,scalar);read(1,scalar);
                merge(block_of[in.target],in.edge,facts);
                merge(block_of[pc+1],0,facts);terminated=true;break;
            case O::ret:returned|=read(0,scalar|array);terminated=true;break;
            }
            ++pc;
        }
        if(!terminated) {
            // Last instruction cannot fall off the program (structural check).
            pc=block.end-1;merge(block_of[block.end],cen,facts);
        }
    }
    pc=0;charge(n);
    result.results.reserve(n);
    for(auto value:produced)result.results.push_back(kind(value));
    result.return_kind=kind(returned);result.states=states.size();
    return result;
}
}}
