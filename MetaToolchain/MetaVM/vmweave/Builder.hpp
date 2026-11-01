#pragma once
#include "../Machinery/MetaVM.hpp"
#include <cctype>
#include <string>
namespace vmweave {
/* The typed authoring layer emits Machinery's verified instruction language.
 * Labels and callee references are resolved by Machinery at finalization. */
class Builder {
    std::string source_;
    size_t limit_;
    bool open_=false;
    uint32_t registers_=0;
    void require_function()const {if(!open_)throw std::runtime_error("VMWeave operation outside function");}
    std::string local(int64_t r)const {
        require_function();
        if(r<0||uint64_t(r)>=registers_)throw std::runtime_error("VMWeave function register range");
        return reg(r);
    }
    static std::string name(const std::string&s) {
        if(s.empty()||s.size()>255||!(std::isalpha(static_cast<unsigned char>(s[0]))||s[0]=='_'))
            throw std::runtime_error("invalid VMWeave identifier");
        for(char c:s)if(!std::isalnum(static_cast<unsigned char>(c))&&c!='_')throw std::runtime_error("invalid VMWeave identifier");
        return s;
    }
    static std::string reg(int64_t r) {
        if(r<0||uint64_t(r)>UINT32_MAX)throw std::runtime_error("VMWeave register range");
        return "r"+std::to_string(r);
    }
public:
    explicit Builder(size_t limit=16*1024*1024):limit_(limit){}
    void emit(const std::string&line) {
        if(line.find('\0')!=std::string::npos||line.size()>=limit_||source_.size()>limit_-line.size()-1)
            throw std::runtime_error("VMWeave source limit or NUL");
        source_.reserve(source_.size()+line.size()+1);
        source_+=line;source_+='\n';
    }
    void function(const std::string&n,int64_t arity,int64_t registers) {
        if(open_||arity<0||registers<arity||uint64_t(registers)>UINT32_MAX)throw std::runtime_error("invalid VMWeave function");
        emit(".function "+name(n)+" "+std::to_string(arity)+" "+std::to_string(registers));registers_=uint32_t(registers);open_=true;
    }
    void constant(int64_t dst,int64_t value){emit("const "+local(dst)+" "+std::to_string(value));}
    void binary(const std::string&op,int64_t dst,int64_t left,int64_t right) {
        if(op!="add"&&op!="sub"&&op!="mul"&&op!="div"&&op!="mod"&&op!="eq"&&op!="lt"&&op!="le"&&op!="and"&&op!="or"&&op!="xor"&&op!="shl"&&op!="shr")
            throw std::runtime_error("invalid VMWeave binary operation");
        emit(op+" "+local(dst)+" "+local(left)+" "+local(right));
    }
    void label(const std::string&n){require_function();emit(name(n)+":");}
    void jump(const std::string&n){require_function();emit("jmp "+name(n));}
    void jump_false(int64_t r,const std::string&n){emit("jz "+local(r)+" "+name(n));}
    void ret(int64_t r){emit("ret "+local(r));}
    void end(){if(!open_)throw std::runtime_error("no VMWeave function");emit(".end");open_=false;}
    metavm::Module finish()const {if(open_)throw std::runtime_error("unterminated VMWeave function");return metavm::Module::compile(source_);}
    const std::string&source()const{return source_;}
};
}
