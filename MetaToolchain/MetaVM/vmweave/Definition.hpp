#pragma once
#include "Builder.hpp"
#include <algorithm>
#include <optional>
#include <cstring>
#include <utility>
#include <vector>
namespace vmweave {
// Version 1: fixed-width guest instructions, scalar SSA expressions, explicit
// register/memory state, and statically resolvable absolute branch destinations.
// Lowering produces a normal verified Machinery module, with no new VM engine.
class Definition {
public:
    enum class Endian { little, big };
    enum class Op { constant, field, read_register, read_byte, add, sub, mul, div,
        mod, bit_and, bit_or, bit_xor, shl, shr, equal, less, write_register,
        write_byte, jump, branch, halt };
    using Ref=std::size_t;
    struct Node {
        Op op; Ref a=0,b=0;
        int64_t immediate=0;
        unsigned shift=0,width=0;
        bool sign=false;
    };
    struct Rule {
        std::string name;
        uint32_t mask=0,value=0;
        std::vector<Node> nodes;
        Ref constant(int64_t n){nodes.push_back({Op::constant,0,0,n});return nodes.size()-1;}
        Ref field(unsigned shift,unsigned width,bool sign=false){
            Node n{Op::field};n.shift=shift;n.width=width;n.sign=sign;
            nodes.push_back(n);return nodes.size()-1;
        }
        Ref read_register(Ref index){nodes.push_back({Op::read_register,index});return nodes.size()-1;}
        Ref read_byte(Ref index){nodes.push_back({Op::read_byte,index});return nodes.size()-1;}
        Ref binary(Op op,Ref a,Ref b){
            if(op<Op::add||op>Op::less)throw std::runtime_error("not a binary definition operation");
            nodes.push_back({op,a,b});return nodes.size()-1;
        }
        void write_register(Ref index,Ref value){nodes.push_back({Op::write_register,index,value});}
        void write_byte(Ref index,Ref value){nodes.push_back({Op::write_byte,index,value});}
        void jump(Ref target){nodes.push_back({Op::jump,target});}
        void branch(Ref condition,Ref target){nodes.push_back({Op::branch,condition,target});}
        void halt(Ref value){nodes.push_back({Op::halt,value});}
    };
    struct State {
        unsigned instruction_bits=16,register_bits=64;
        std::size_t registers=16,memory_bytes=4096;
        Endian endian=Endian::big;
    } state;
    struct Result {
        int64_t value=0;
        std::vector<int64_t> registers;
        std::vector<uint8_t> memory;
        uint64_t steps=0; // Machinery instructions, including decode lowering.
    };
    std::string name;
    std::vector<Rule> rules;
    explicit Definition(std::string name="anonymous"):name(std::move(name)){}
    void verify()const;
    metavm::Module lower(const std::vector<uint8_t>&code,std::size_t max_source=16*1024*1024)const;
    Result execute(const std::vector<uint8_t>&code,const std::vector<int64_t>&registers={},
        const std::vector<uint8_t>&memory={},const mvm_limits&limits=mvm_default_limits(),
        mvm_engine engine=MVM_ENGINE_DEFAULT)const;
private:
    static bool produces(Op op){return op<=Op::less;}
    static bool terminal(Op op){return op==Op::jump||op==Op::branch||op==Op::halt;}
    static const char* mnemonic(Op op) {
        switch(op){case Op::add:return "add";case Op::sub:return "sub";
        case Op::mul:return "mul";case Op::div:return "div";case Op::mod:return "mod";
        case Op::bit_and:return "and";case Op::bit_or:return "or";case Op::bit_xor:return "xor";
        case Op::shl:return "shl";case Op::shr:return "shr";case Op::equal:return "eq";
        case Op::less:return "lt";default:throw std::runtime_error("invalid definition arithmetic");}
    }
    static uint64_t mask(unsigned width){return width==64?UINT64_MAX:(UINT64_C(1)<<width)-1;}
    static int64_t from_bits(uint64_t bits) {
        int64_t result;std::memcpy(&result,&bits,sizeof result);return result;
    }
};
inline void Definition::verify()const {
    if(name.empty()||name.size()>255||(state.instruction_bits!=8&&state.instruction_bits!=16&&state.instruction_bits!=32)||
       !state.register_bits||state.register_bits>64||!state.registers||state.registers>4096||
       state.memory_bytes>16777216||rules.empty()||rules.size()>4096||
       (state.endian!=Endian::big&&state.endian!=Endian::little))throw std::runtime_error("invalid machine definition state");
    const auto word_mask=mask(state.instruction_bits);
    for(std::size_t r=0;r<rules.size();++r) {
        const auto&rule=rules[r];
        if(rule.name.empty()||rule.name.size()>255||rule.nodes.empty()||rule.nodes.size()>4096||
           (rule.mask&~word_mask)||(rule.value&~rule.mask))throw std::runtime_error("invalid instruction rule");
        for(std::size_t j=0;j<r;++j) {
            const auto&other=rules[j];
            if(rule.name==other.name)throw std::runtime_error("duplicate instruction rule name");
            if(((rule.value^other.value)&(rule.mask&other.mask))==0)throw std::runtime_error("ambiguous instruction masks");
        }
        for(std::size_t k=0;k<rule.nodes.size();++k) {
            const auto&n=rule.nodes[k];
            if(n.op<Op::constant||n.op>Op::halt)throw std::runtime_error("unknown definition operation");
            auto ref=[&](Ref x){if(x>=k||!produces(rule.nodes[x].op))throw std::runtime_error("invalid definition value reference");};
            if(terminal(n.op)&&k+1!=rule.nodes.size())throw std::runtime_error("instruction continues after control transfer");
            if(n.op==Op::field) {
                if(!n.width||n.width>state.instruction_bits||n.shift>state.instruction_bits-n.width)
                    throw std::runtime_error("invalid instruction field");
            } else if(n.op!=Op::constant) {
                ref(n.a);
                if((n.op>=Op::add&&n.op<=Op::less)||n.op==Op::write_register||n.op==Op::write_byte||n.op==Op::branch)ref(n.b);
            }
        }
    }
}
inline metavm::Module Definition::lower(const std::vector<uint8_t>&code,std::size_t max_source)const {
    verify();const std::size_t bytes=state.instruction_bits/8;
    if(code.empty()||code.size()>65536||code.size()%bytes)throw std::runtime_error("invalid guest code length");
    std::size_t node_count=0;for(const auto&r:rules)node_count=std::max(node_count,r.nodes.size());
    const std::size_t scratch=2+node_count,mask_register=scratch+1;
    Builder out(max_source);out.function("main",2,int64_t(scratch+2));
    auto reg=[](std::size_t r){return "r"+std::to_string(r);};
    auto label=[](std::size_t offset){return "guest_"+std::to_string(offset);};
    for(std::size_t pc=0;pc<code.size();pc+=bytes) {
        uint32_t word=0;
        for(std::size_t k=0;k<bytes;++k) {
            if(state.endian==Endian::big)word=(word<<8)|code[pc+k];
            else word|=uint32_t(code[pc+k])<<unsigned(k*8);
        }
        const Rule*selected=nullptr;
        for(const auto&r:rules)if((word&r.mask)==r.value){selected=&r;break;}
        if(!selected)throw std::runtime_error("unknown guest opcode at byte "+std::to_string(pc));
        const auto&nodes=selected->nodes;
        if(pc+bytes==code.size()&&!terminal(nodes.back().op))throw std::runtime_error("guest program falls off end");
        if(pc+bytes==code.size()&&nodes.back().op==Op::branch)throw std::runtime_error("conditional branch falls off end");
        out.label(label(pc));std::vector<std::optional<int64_t>> known(nodes.size());
        auto target=[&](Ref ref){
            if(!known[ref])throw std::runtime_error("dynamic branch target unsupported by definition v1");
            int64_t n=*known[ref];
            if(n<0||uint64_t(n)>=code.size()||uint64_t(n)%bytes)throw std::runtime_error("guest branch target out of bounds or unaligned");
            return label(std::size_t(n));
        };
        for(std::size_t k=0;k<nodes.size();++k) {
            const auto&n=nodes[k];std::size_t dest=k+2,a=n.a+2,b=n.b+2;
            switch(n.op) {
            case Op::constant:known[k]=n.immediate;out.constant(int64_t(dest),n.immediate);break;
            case Op::field:{
                uint64_t v=(uint64_t(word)>>n.shift)&mask(n.width);
                if(n.sign&&(v&(UINT64_C(1)<<(n.width-1))))v|=~mask(n.width);
                known[k]=from_bits(v);out.constant(int64_t(dest),*known[k]);break;
            }
            case Op::read_register:case Op::read_byte:
                out.emit("get "+reg(dest)+(n.op==Op::read_register?" r0 ":" r1 ")+reg(a));break;
            case Op::write_register:case Op::write_byte: {
                unsigned width=n.op==Op::write_byte?8:state.register_bits;
                out.constant(int64_t(mask_register),from_bits(mask(width)));
                out.binary("and",int64_t(scratch),int64_t(b),int64_t(mask_register));
                out.emit(std::string("set ")+(n.op==Op::write_register?"r0 ":"r1 ")+reg(a)+" "+reg(scratch));break;
            }
            case Op::jump:out.jump(target(n.a));break;
            case Op::branch:
                // Machinery considers integer zero truthy. Convert the guest
                // integer condition to a boolean before using its JZ opcode.
                out.constant(int64_t(mask_register),0);
                out.binary("eq",int64_t(scratch),int64_t(a),int64_t(mask_register));
                out.jump_false(int64_t(scratch),target(n.b));break;
            case Op::halt:out.ret(int64_t(a));break;
            default: {
                const auto*op=mnemonic(n.op);
                if(known[n.a]&&known[n.b]) {
                    mvm_opcode selected_op=MVM_OP_ADD;
                    for(int j=0;j<int(MVM_OP_COUNT);++j)if(std::string(mvm_opcode_name(mvm_opcode(j)))==op){selected_op=mvm_opcode(j);break;}
                    int64_t result=0;
                    if(mvm_integer_eval(selected_op,*known[n.a],*known[n.b],&result)!=MVM_OK)throw std::runtime_error("invalid constant expression in definition");
                    known[k]=result;
                }
                out.binary(op,int64_t(dest),int64_t(a),int64_t(b));
                if(n.op==Op::equal||n.op==Op::less) {
                    // Definition values are integer scalars; Machinery's
                    // comparison opcodes return booleans, so normalize them.
                    const auto base="compare_"+std::to_string(pc)+"_"+std::to_string(k);
                    out.jump_false(int64_t(dest),base+"_false");
                    out.constant(int64_t(dest),1);out.jump(base+"_done");
                    out.label(base+"_false");out.constant(int64_t(dest),0);out.label(base+"_done");
                }
                break;
            }
            }
        }
    }
    out.end();return out.finish();
}
inline Definition::Result Definition::execute(const std::vector<uint8_t>&code,const std::vector<int64_t>&initial,
    const std::vector<uint8_t>&memory,const mvm_limits&limits,mvm_engine engine)const {
    auto module=lower(code);
    if(initial.size()>state.registers||memory.size()>state.memory_bytes)throw std::runtime_error("guest initial state exceeds definition");
    metavm::VM vm(limits);mvm_error error{};mvm_value registers,bytes;
    metavm::check(mvm_array_new(vm.get(),state.registers,&registers),error);auto register_root=vm.pin(registers);
    metavm::check(mvm_array_new(vm.get(),state.memory_bytes,&bytes),error);auto memory_root=vm.pin(bytes);
    for(std::size_t k=0;k<state.registers;++k) {
        int64_t v=k<initial.size()?initial[k]:0;
        v=from_bits(uint64_t(v)&mask(state.register_bits));
        metavm::check(mvm_array_set(vm.get(),registers,k,mvm_int(v)),error);
    }
    for(std::size_t k=0;k<state.memory_bytes;++k)
        metavm::check(mvm_array_set(vm.get(),bytes,k,mvm_int(k<memory.size()?memory[k]:0)),error);
    auto value=vm.run(module,0,{registers,bytes},engine);
    if(value.kind!=MVM_INT)throw std::runtime_error("guest halt value is not scalar");
    Result result;result.value=value.as.integer;result.steps=mvm_steps(vm.get());
    for(std::size_t k=0;k<state.registers;++k) {
        mvm_value v;metavm::check(mvm_array_get(vm.get(),registers,k,&v),error);
        if(v.kind!=MVM_INT)throw std::runtime_error("invalid guest register value");
        result.registers.push_back(v.as.integer);
    }
    for(std::size_t k=0;k<state.memory_bytes;++k) {
        mvm_value v;metavm::check(mvm_array_get(vm.get(),bytes,k,&v),error);
        if(v.kind!=MVM_INT)throw std::runtime_error("invalid guest memory value");
        result.memory.push_back(uint8_t(v.as.integer));
    }
    return result;
}
}
