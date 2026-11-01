#include "Forth.hpp"
#include "MetaVM.h"
#include <charconv>
#include <map>
#include <sstream>
#include <stdexcept>
namespace metavm { namespace machines {
std::vector<int64_t> forth(const std::string&source,uint64_t fuel,size_t limit) {
    if(source.size()>16*1024*1024||!limit||limit>1000000)throw std::runtime_error("FORTH limits");
    std::map<std::string,std::vector<std::string>> dict;
    std::istringstream in(source);std::string t;std::vector<std::string> main;
    while(in>>t) {
        if(t==":") {std::string name;if(!(in>>name)||name==":"||name==";")throw std::runtime_error("FORTH definition name");
            std::vector<std::string> body;while(in>>t&&t!=";")body.push_back(t);
            if(t!=";")throw std::runtime_error("FORTH unterminated definition");
            dict[name]=std::move(body);
        }else {if(t==";")throw std::runtime_error("FORTH stray semicolon");main.push_back(t);}
    }
    struct Frame {const std::vector<std::string>*code;size_t pc;};
    std::vector<Frame> frames{{&main,0}};std::vector<int64_t>s;
    auto pop=[&](){if(s.empty())throw std::runtime_error("FORTH stack underflow");auto x=s.back();s.pop_back();return x;};
    auto push=[&](int64_t x){if(s.size()>=limit)throw std::runtime_error("FORTH stack limit");s.push_back(x);};
    uint64_t steps=0;
    while(!frames.empty()) {
        auto &f=frames.back();if(f.pc==f.code->size()){frames.pop_back();continue;}
        if(steps++==fuel)throw std::runtime_error("FORTH fuel exhausted");
        const std::string&w=(*f.code)[f.pc++];int64_t n=0;
        auto parsed=std::from_chars(w.data(),w.data()+w.size(),n);
        if(parsed.ec==std::errc()&&parsed.ptr==w.data()+w.size()){push(n);continue;}
        auto found=dict.find(w);if(found!=dict.end()) {if(frames.size()>=4096)throw std::runtime_error("FORTH return stack limit");frames.push_back({&found->second,0});continue;}
        if(w=="dup"){auto a=pop();push(a);push(a);}
        else if(w=="drop")pop();
        else if(w=="swap"){auto b=pop(),a=pop();push(b);push(a);}
        else if(w=="over"){auto b=pop(),a=pop();push(a);push(b);push(a);}
        else {
            mvm_opcode op;
            if(w=="+")op=MVM_OP_ADD;else if(w=="-")op=MVM_OP_SUB;else if(w=="*")op=MVM_OP_MUL;
            else if(w=="/")op=MVM_OP_DIV;else if(w=="mod")op=MVM_OP_MOD;else if(w=="=")op=MVM_OP_EQ;else if(w=="<")op=MVM_OP_LT;
            else throw std::runtime_error("FORTH unknown word: "+w);
            auto b=pop(),a=pop();int64_t r;if(mvm_integer_eval(op,a,b,&r)!=MVM_OK)throw std::runtime_error("FORTH arithmetic trap");
            push((op==MVM_OP_EQ||op==MVM_OP_LT)?-r:r);
        }
    }
    return s;
}
}}
