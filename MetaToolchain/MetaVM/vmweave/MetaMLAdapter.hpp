#pragma once
// Preserve the supplied dependency byte-for-byte. Clang diagnoses its unused
// parser bookkeeping field; suppress that warning only while reading MetaML.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
#endif
#include "../third_party/MetaML.hpp"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#include "Builder.hpp"
#include "Definition.hpp"
#include <limits>
namespace vmweave {
struct EvaluationLimits {
    uint64_t max_steps=1000000; // MetaML evaluation/call cancellation checkpoints.
    int max_depth=256;
};
// Retain the complete language diagnostics, including secondary spans and fixes.
class MetaMLError : public std::runtime_error {
public:
    std::vector<metaml::Diagnostic> diagnostics;
    MetaMLError(std::string message,std::vector<metaml::Diagnostic> details)
        :std::runtime_error(std::move(message)),diagnostics(std::move(details)){}
};
namespace detail {
inline void check_source(const std::string&source,size_t max_bytes) {
    if(source.size()>max_bytes || source.find('\0')!=std::string::npos)
        throw std::runtime_error("VMWeave authoring source limit or NUL");
}
inline void evaluate(metaml::Session&session,const std::string&source,const std::string&file,
                     const EvaluationLimits&limits) {
    // The supplied MetaML resolves host primitives through qualified paths.
    // Publish language-level aliases to preserve the vm_* / vd_* authoring API.
    std::string aliases;
    for(const auto&name:session.primitive_names())if(name.compare(0,3,"vm_")==0||name.compare(0,3,"vd_")==0) {
        auto*p=session.find_primitive(name);
        session.register_primitive("VMWeave."+name,p->type_src,p->arity,p->fn);
        aliases+="val "+name+" := VMWeave."+name+"\n";
    }
    auto installed=session.eval(aliases,"<VMWeave-primitives>");
    if(installed.is_err())throw MetaMLError("VMWeave primitive installation failed",installed.unwrap_err());
    if(limits.max_depth<1)throw std::runtime_error("VMWeave invalid MetaML depth limit");
    session.set_max_depth(limits.max_depth);
    session.interp().cancelled=[remaining=limits.max_steps]()mutable {
        if(!remaining)return true;
        --remaining;return false;
    };
    auto evaluated=session.eval(source,file);
    if(evaluated.is_err()) {
        std::string message="MetaML evaluation failed; no artifact published";
        for(const auto&d:evaluated.unwrap_err())message+="\n"+session.render(d);
        throw MetaMLError(std::move(message),evaluated.unwrap_err());
    }
}
}
/* A fresh Session per translation. No stdlib or file imports are required.
 * This evaluates trusted build-time MetaML, not untrusted guest bytecode.
 * Host callbacks capture owned shared state so no callback dangles. */
inline metavm::Module compile(const std::string& source, size_t max_bytes,
                              const std::string&file,const EvaluationLimits&limits={}) {
    detail::check_source(source,max_bytes);
    auto builder=std::make_shared<Builder>(max_bytes);
    metaml::Session session;
    session.register_primitive("vm_emit", [builder](std::string line) -> void {builder->emit(line);});
    session.register_primitive("vm_function", [builder](std::string n,int64_t a,int64_t r) -> void {builder->function(n,a,r);});
    session.register_primitive("vm_const", [builder](int64_t r,int64_t v) -> void {builder->constant(r,v);});
    session.register_primitive("vm_binary", [builder](std::string op,int64_t d,int64_t l,int64_t r) -> void {builder->binary(op,d,l,r);});
    session.register_primitive("vm_label", [builder](std::string n) -> void {builder->label(n);});
    session.register_primitive("vm_jump", [builder](std::string n) -> void {builder->jump(n);});
    session.register_primitive("vm_jump_false", [builder](int64_t r,std::string n) -> void {builder->jump_false(r,n);});
    session.register_primitive("vm_return", [builder](int64_t r) -> void {builder->ret(r);});
    session.register_primitive("vm_end", [builder]() -> void {builder->end();});
    detail::evaluate(session,source,file,limits);
    return builder->finish();
}
inline metavm::Module compile(const std::string&source,size_t max_bytes=16*1024*1024) {
    return compile(source,max_bytes,"<VMWeave>");
}

namespace detail {
struct DefinitionAuthor {
    Definition definition;
    bool started=false,open=false;
    size_t nodes=0;
    static size_t bounded(int64_t value,size_t maximum) {
        if(value<0||uint64_t(value)>maximum)throw std::runtime_error("VMWeave definition argument range");
        return size_t(value);
    }
    Definition::Rule&rule() {
        if(!open)throw std::runtime_error("VMWeave definition operation outside rule");
        auto&r=definition.rules.back();
        if(r.nodes.size()>=4096||nodes>=65536)throw std::runtime_error("VMWeave definition node limit");
        if(!r.nodes.empty()&&r.nodes.back().op>=Definition::Op::jump)
            throw std::runtime_error("VMWeave definition operation after control transfer");
        return r;
    }
    Definition::Ref ref(int64_t value) {
        auto&r=rule();auto index=bounded(value,4095);
        if(index>=r.nodes.size()||r.nodes[index].op>Definition::Op::less)
            throw std::runtime_error("VMWeave definition invalid value reference");
        return index;
    }
    void add(Definition::Node node) {rule().nodes.push_back(node);++nodes;}
    int64_t value(Definition::Node node) {add(node);return int64_t(definition.rules.back().nodes.size()-1);}
};
}

// Author the full fixed-width Definition v1 IR using the real MetaML evaluator.
// Source, emitted IR, evaluation checkpoints and recursion depth are bounded.
// MetaML imports/host effects remain trusted build-time code, not a sandbox.
inline Definition compile_definition(const std::string&source,size_t max_bytes=16*1024*1024,
                                     const std::string&file="<VMWeave-definition>",const EvaluationLimits&limits={}) {
    detail::check_source(source,max_bytes);
    using A=detail::DefinitionAuthor;using O=Definition::Op;
    auto author=std::make_shared<A>();metaml::Session session;
    session.register_primitive("vd_machine",[author](std::string name,int64_t bits,std::string endian,
        int64_t registers,int64_t register_bits,int64_t memory)->void {
        if(author->started)throw std::runtime_error("VMWeave definition already started");
        if(name.empty()||name.size()>255||(bits!=8&&bits!=16&&bits!=32)||
           (endian!="big"&&endian!="little")||registers<1||register_bits<1)
            throw std::runtime_error("VMWeave invalid definition state");
        Definition d(name);d.state.instruction_bits=unsigned(bits);
        d.state.endian=endian=="big"?Definition::Endian::big:Definition::Endian::little;
        d.state.registers=A::bounded(registers,4096);d.state.register_bits=unsigned(A::bounded(register_bits,64));
        d.state.memory_bytes=A::bounded(memory,16777216);
        author->definition=std::move(d);author->started=true;
    });
    session.register_primitive("vd_rule",[author](std::string name,int64_t mask,int64_t value)->void {
        if(!author->started||author->open)throw std::runtime_error("VMWeave definition rule structure");
        if(name.empty()||name.size()>255||author->definition.rules.size()>=4096)
            throw std::runtime_error("VMWeave definition rule limit or name");
        author->definition.rules.push_back({name,uint32_t(A::bounded(mask,UINT32_MAX)),uint32_t(A::bounded(value,UINT32_MAX)),{}});
        author->open=true;
    });
    session.register_primitive("vd_const",[author](int64_t value)->int64_t {return author->value({O::constant,0,0,value});});
    session.register_primitive("vd_field",[author](int64_t shift,int64_t width,bool sign)->int64_t {
        Definition::Node n{O::field};n.shift=unsigned(A::bounded(shift,31));n.width=unsigned(A::bounded(width,32));n.sign=sign;
        if(!n.width||n.width>author->definition.state.instruction_bits||n.shift>author->definition.state.instruction_bits-n.width)
            throw std::runtime_error("VMWeave invalid definition field");
        return author->value(n);
    });
    session.register_primitive("vd_read_register",[author](int64_t index)->int64_t {return author->value({O::read_register,author->ref(index)});});
    session.register_primitive("vd_read_byte",[author](int64_t index)->int64_t {return author->value({O::read_byte,author->ref(index)});});
    session.register_primitive("vd_binary",[author](std::string op,int64_t left,int64_t right)->int64_t {
        static const std::vector<std::pair<std::string,O>> operations={
            {"add",O::add},{"sub",O::sub},{"mul",O::mul},{"div",O::div},{"mod",O::mod},
            {"and",O::bit_and},{"or",O::bit_or},{"xor",O::bit_xor},{"shl",O::shl},{"shr",O::shr},
            {"eq",O::equal},{"lt",O::less}};
        for(const auto&entry:operations)if(op==entry.first)
            return author->value({entry.second,author->ref(left),author->ref(right)});
        throw std::runtime_error("VMWeave invalid definition binary operation");
    });
    session.register_primitive("vd_write_register",[author](int64_t index,int64_t value)->void {author->add({O::write_register,author->ref(index),author->ref(value)});});
    session.register_primitive("vd_write_byte",[author](int64_t index,int64_t value)->void {author->add({O::write_byte,author->ref(index),author->ref(value)});});
    session.register_primitive("vd_jump",[author](int64_t target)->void {author->add({O::jump,author->ref(target)});});
    session.register_primitive("vd_branch",[author](int64_t condition,int64_t target)->void {author->add({O::branch,author->ref(condition),author->ref(target)});});
    session.register_primitive("vd_halt",[author](int64_t value)->void {author->add({O::halt,author->ref(value)});});
    session.register_primitive("vd_end",[author]()->void {
        if(!author->open)throw std::runtime_error("VMWeave definition end outside rule");
        author->open=false;
    });
    detail::evaluate(session,source,file,limits);
    if(author->open)throw std::runtime_error("VMWeave unterminated definition rule");
    author->definition.verify();return std::move(author->definition);
}
}
