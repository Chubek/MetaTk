#include "ISSA.hpp"
#include <charconv>
#include <sstream>
#include <map>
namespace metavm { namespace machines {
namespace {
using O=ISSA::Op;
const std::map<std::string,O> names={
    {"const",O::constant},{"int_arg",O::argument},{"add",O::add},{"iadd",O::add},
    {"sub",O::sub},{"isub",O::sub},{"mul",O::mul},{"imul",O::mul},{"div",O::div},{"idiv",O::div},
    {"eq",O::equal},{"le",O::less_equal},{"goto",O::jump},{"beq",O::branch_equal},
    {"ble",O::branch_le},{"phi",O::phi},{"pfe",O::pfe},{"array",O::array},
    {"get",O::get},{"set",O::set},{"length",O::length},{"return",O::ret},
    {"newarray",O::array},{"access",O::get},{"update",O::set},
    {"and",O::logical_and},{"or",O::logical_or},{"neg",O::negate},
    {"bne",O::branch_ne},{"blt",O::branch_lt},{"bge",O::branch_ge},{"bgt",O::branch_gt}};
bool branch(O op) {
    return op==O::jump||op==O::branch_equal||op==O::branch_le||
           op==O::branch_ne||op==O::branch_lt||op==O::branch_ge||op==O::branch_gt;
}
const char*canonical(O op) {
    switch(op) {
    case O::constant:return "const";case O::argument:return "int_arg";
    case O::add:return "add";case O::sub:return "sub";case O::mul:return "mul";case O::div:return "div";
    case O::equal:return "eq";case O::less_equal:return "le";case O::jump:return "goto";
    case O::branch_equal:return "beq";case O::branch_le:return "ble";case O::phi:return "phi";
    case O::pfe:return "pfe";case O::array:return "array";case O::get:return "get";
    case O::set:return "set";case O::length:return "length";case O::ret:return "return";
    case O::logical_and:return "and";case O::logical_or:return "or";case O::negate:return "neg";
    case O::branch_ne:return "bne";case O::branch_lt:return "blt";case O::branch_ge:return "bge";case O::branch_gt:return "bgt";
    }
    throw std::runtime_error("unknown ISSA opcode");
}
int64_t integer(const std::string&s) {
    int64_t n;auto parsed=std::from_chars(s.data(),s.data()+s.size(),n);
    if(parsed.ec!=std::errc()||parsed.ptr!=s.data()+s.size())throw std::runtime_error("invalid decimal integer: "+s);
    return n;
}
size_t index(const std::string&s,char open=0,char close=0) {
    std::string t=s;
    if(open) {
        if(s.size()<3||s.front()!=open||s.back()!=close)throw std::runtime_error("invalid ISSA reference: "+s);
        t=s.substr(1,s.size()-2);
    }
    auto n=integer(t);if(n<0||uint64_t(n)>SIZE_MAX)throw std::runtime_error("ISSA index limit");return size_t(n);
}
}
std::vector<ISSA::Instruction> ISSA::parse(const std::string&source) {
    return parse(source,Limits{});
}
std::vector<ISSA::Instruction> ISSA::parse(const std::string&source,const Limits&limits) {
    return parse_impl(source,limits,nullptr);
}
std::vector<ISSA::Instruction> ISSA::parse_checked(const std::string&source) {
    return parse_checked(source,Limits{},AnalysisLimits{});
}
std::vector<ISSA::Instruction> ISSA::parse_checked(const std::string&source,const Limits&limits,const AnalysisLimits&analysis_limits) {
    return parse_impl(source,limits,&analysis_limits);
}
std::vector<ISSA::Instruction> ISSA::parse_impl(const std::string&source,const Limits&limits,const AnalysisLimits*analysis_limits) {
    if(source.size()>limits.max_source_bytes||source.find('\0')!=std::string::npos)throw std::runtime_error("ISSA source limit or NUL");
    std::istringstream input(source);std::string line;size_t line_number=0;
    std::vector<Instruction> program;std::vector<size_t> source_lines;size_t argument_ordinal=0,operands=0;
    while(std::getline(input,line)) {
        ++line_number;
        try {
            if(line.size()>limits.max_line_bytes)throw std::runtime_error("ISSA line length limit");
            auto comment=line.find_first_of("#;");if(comment!=std::string::npos)line.resize(comment);
            std::istringstream words(line);std::vector<std::string> tokens;std::string token;
            while(words>>token){if(tokens.size()>=limits.max_tokens_per_line)throw std::runtime_error("ISSA line token limit");tokens.push_back(token);}
            if(tokens.empty())continue;
            if(program.size()>=limits.max_instructions)throw std::runtime_error("ISSA instruction limit");
            size_t pos=0;
            if(tokens[0][0]>='0'&&tokens[0][0]<='9') {
                if(index(tokens[0])!=program.size())throw std::runtime_error("instruction numbers must be consecutive");
                ++pos;
            }
            if(pos==tokens.size())throw std::runtime_error("missing opcode");
            auto found=names.find(tokens[pos++]);if(found==names.end())throw std::runtime_error("unsupported opcode");
            Instruction in{found->second,{}};
            if(in.op==O::constant||in.op==O::argument) {
                if(in.op==O::argument && pos==tokens.size())in.immediate=int64_t(argument_ordinal);
                else {
                    if(tokens.size()-pos!=1)throw std::runtime_error("expected one immediate");
                    in.immediate=integer(tokens[pos++]);
                }
                if(in.op==O::argument)++argument_ordinal;
            } else {
                size_t references=in.op==O::phi?tokens.size()-pos:
                    in.op==O::ret||in.op==O::array||in.op==O::length||in.op==O::negate?1:
                    in.op==O::pfe||in.op==O::jump?0:in.op==O::set?3:2;
                if(references>limits.max_operands-operands)throw std::runtime_error("ISSA operand budget");
                operands+=references;
                for(size_t k=0;k<references;++k) {
                    if(pos==tokens.size())throw std::runtime_error("missing operand");
                    in.operands.push_back(index(tokens[pos++],'(',')'));
                }
                if(branch(in.op)) {
                    if(tokens.size()-pos!=2)throw std::runtime_error("expected [target] and edge number");
                    in.target=index(tokens[pos++],'[',']');in.edge=index(tokens[pos++]);
                }
            }
            if(pos!=tokens.size())throw std::runtime_error("extra operand");
            program.push_back(std::move(in));source_lines.push_back(line_number);
        }catch(const std::exception&e){throw std::runtime_error("ISSA line "+std::to_string(line_number)+": "+e.what());}
    }
    try{if(analysis_limits)analyze(program,limits,*analysis_limits);else verify(program,limits);}
    catch(const VerificationError&e){throw std::runtime_error("ISSA line "+std::to_string(source_lines.at(e.instruction))+": "+e.what());}
    return program;
}
std::string ISSA::format(const std::vector<Instruction>&program) {
    return format(program,Limits{});
}
std::string ISSA::format(const std::vector<Instruction>&program,const Limits&limits) {
    verify(program,limits);std::ostringstream out;
    for(size_t pc=0;pc<program.size();++pc) {
        const auto&in=program[pc];out<<pc<<' '<<canonical(in.op);
        if(in.op==O::constant||in.op==O::argument)out<<' '<<in.immediate;
        for(auto ref:in.operands)out<<" ("<<ref<<')';
        if(branch(in.op))out<<" ["<<in.target<<"] "<<in.edge;
        out<<'\n';
    }
    return out.str();
}
}}
