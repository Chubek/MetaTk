#include "Weave.hpp"
#include <MetaIL.h>
#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>

namespace metavm {
namespace {
class Compiler {
    const metail_function &fn_;
    Limits limits_;
    std::map<std::string,std::uint32_t> locals_,hosts_;
    std::map<std::string,std::size_t> labels_;
    std::vector<std::string> host_names_;
    std::vector<Instruction> code_;
    std::vector<std::size_t> positions_;
    std::uint32_t next_=0, peak_=0;
    std::uint32_t temporary() {
        if (next_>=limits_.slots) throw std::length_error("vmweave: slot limit exceeded");
        auto slot=next_++;peak_=std::max(peak_,next_);return slot;
    }
    std::size_t emit(Instruction i) {
        if (code_.size()>=limits_.instructions) throw std::length_error("vmweave: instruction limit exceeded");
        code_.push_back(i);return code_.size()-1;
    }
    std::uint32_t expression(const metail_expr &e) {
        // Always snapshot a variable into a temporary before later host calls.
        auto out=temporary();
        if (e.kind==METAIL_INT) { emit({Op::Constant,out,0,0,e.value});return out; }
        if (e.kind==METAIL_VAR) { emit({Op::Move,out,locals_.at(e.name)});return out; }
        std::vector<std::uint32_t> args;
        if (e.kind==METAIL_OP && std::string_view(e.op)=="select") {
            auto condition=expression(*e.args[0]);
            auto branch=emit({Op::Branch,0,condition,0,0});
            code_[branch].b=static_cast<std::uint32_t>(code_.size());
            auto yes=expression(*e.args[1]);emit({Op::Move,out,yes});
            auto jump=emit({Op::Jump,0,0});
            code_[branch].imm=static_cast<std::int64_t>(code_.size());
            auto no=expression(*e.args[2]);emit({Op::Move,out,no});
            code_[jump].a=static_cast<std::uint32_t>(code_.size());return out;
        }
        for (std::size_t i=0;i<e.arg_count;++i) args.push_back(expression(*e.args[i]));
        if (e.kind==METAIL_CALL) {
            if (args.size()>64) throw std::invalid_argument("vmweave: at most 64 host arguments");
            auto found=hosts_.find(e.name);
            if (found==hosts_.end()) {
                if (host_names_.size()>=limits_.hosts) throw std::length_error("vmweave: host limit exceeded");
                auto index=static_cast<std::uint32_t>(host_names_.size());host_names_.push_back(e.name);
                found=hosts_.emplace(e.name,index).first;
            }
            auto base=next_;
            for (auto value:args) { auto dst=temporary();emit({Op::Move,dst,value}); }
            emit({Op::Call,out,found->second,base,static_cast<std::int64_t>(args.size())});return out;
        }
        static const std::map<std::string,Op> ops{
            {"add",Op::Add},{"sub",Op::Sub},{"mul",Op::Mul},{"div",Op::Div},{"mod",Op::Mod},
            {"shl",Op::Shl},{"shr",Op::Shr},{"band",Op::Band},{"bor",Op::Bor},{"bxor",Op::Bxor},
            {"neg",Op::Neg},{"bnot",Op::Bnot},{"lnot",Op::Lnot},{"eq",Op::Eq},{"ne",Op::Ne},
            {"lt",Op::Lt},{"le",Op::Le},{"gt",Op::Gt},{"ge",Op::Ge},{"land",Op::Land},{"lor",Op::Lor}};
        auto op=ops.find(e.op);
        if (op==ops.end()) throw std::invalid_argument("vmweave: unsupported operator");
        emit({op->second,out,args[0],args.size()>1 ? args[1] : 0});return out;
    }
    void variables(const metail_expr &e,std::set<std::uint32_t> &reads) {
        if (e.kind==METAIL_VAR) {
            auto it=locals_.find(e.name);
            if (it==locals_.end()) throw std::invalid_argument("vmweave: undefined variable: "+std::string(e.name));
            reads.insert(it->second);
        }
        for (std::size_t i=0;i<e.arg_count;++i) variables(*e.args[i],reads);
    }
    void validate() {
        if (!fn_.statement_count || fn_.statement_count>limits_.instructions)
            throw std::invalid_argument("vmweave: empty function or statement limit exceeded");
        auto local=[&](const char *name) { if (!locals_.count(name)) locals_.emplace(name,temporary()); };
        for (std::size_t i=0;i<fn_.param_count;++i) local(fn_.params[i]);
        for (std::size_t i=0;i<fn_.statement_count;++i) {
            const auto &s=fn_.statements[i];
            if (s.kind==METAIL_LET) local(s.name);
            if (s.kind==METAIL_LABEL) labels_.emplace(s.name,i);
            if (s.kind==METAIL_ASSUME) throw std::invalid_argument("vmweave: assume has no execution contract");
        }
        const auto count=fn_.statement_count;
        std::vector<std::set<std::uint32_t>> incoming(count),reads(count);
        std::vector<std::vector<std::size_t>> successors(count);
        for (std::size_t i=0;i<count;++i) {
            const auto &s=fn_.statements[i];
            if (s.expr) variables(*s.expr,reads[i]);
            if (s.kind==METAIL_GOTO) successors[i]={labels_.at(s.target)};
            else if (s.kind==METAIL_BRANCH) successors[i]={labels_.at(s.yes),labels_.at(s.no)};
            else if (s.kind!=METAIL_RETURN) successors[i]={i+1};
        }
        for (std::uint32_t i=0;i<fn_.param_count;++i) incoming[0].insert(i);
        std::vector<bool> reached(count,false),queued(count,false);
        std::deque<std::size_t> work{0};reached[0]=queued[0]=true;
        while (!work.empty()) {
            auto pc=work.front();work.pop_front();queued[pc]=false;
            auto out=incoming[pc];const auto &s=fn_.statements[pc];
            if (s.kind==METAIL_LET) out.insert(locals_.at(s.name));
            for (auto next:successors[pc]) {
                if (next==count) throw std::invalid_argument("vmweave: reachable fallthrough without return");
                auto merged=out;
                if (reached[next]) for (auto it=merged.begin();it!=merged.end();) {
                    if (!incoming[next].count(*it)) it=merged.erase(it);else ++it;
                }
                if (!reached[next] || merged!=incoming[next]) {
                    incoming[next]=std::move(merged);reached[next]=true;
                    if (!queued[next]) { work.push_back(next);queued[next]=true; }
                }
            }
        }
        for (std::size_t i=0;i<count;++i) if (reached[i]) for (auto value:reads[i])
            if (!incoming[i].count(value)) throw std::invalid_argument("vmweave: possibly uninitialized variable at statement "+std::to_string(i+1));
    }
public:
    Compiler(const metail_function &fn,Limits limits):fn_(fn),limits_(limits) {}
    Program compile() {
        validate();
        auto locals=next_;
        for (std::size_t pc=0;pc<fn_.statement_count;++pc) {
            next_=locals;positions_.push_back(code_.size());
            const auto &s=fn_.statements[pc];
            std::uint32_t value=0;if (s.expr) value=expression(*s.expr);
            if (s.kind==METAIL_LET) emit({Op::Move,locals_.at(s.name),value});
            else if (s.kind==METAIL_RETURN) emit({Op::Return,0,value});
            else if (s.kind==METAIL_GOTO) emit({Op::Jump,0,0});
            else if (s.kind==METAIL_BRANCH) emit({Op::Branch,0,value,0,0});
        }
        // Labels have no emitted instruction; empty/unreachable suffixes need a
        // valid landing instruction too. It is unreachable on validated paths.
        peak_=std::max(peak_,std::uint32_t(1));
        emit({Op::Constant,0,0,0,0});emit({Op::Return,0,0});
        for (std::size_t pc=0;pc<fn_.statement_count;++pc) {
            const auto &s=fn_.statements[pc];
            if (s.kind==METAIL_GOTO) code_[positions_[pc]].a=static_cast<std::uint32_t>(positions_[labels_.at(s.target)]);
            else if (s.kind==METAIL_BRANCH) {
                auto end=(pc+1<positions_.size() ? positions_[pc+1] : code_.size()-2);
                auto &branch=code_[end-1];
                branch.b=static_cast<std::uint32_t>(positions_[labels_.at(s.yes)]);
                branch.imm=static_cast<std::int64_t>(positions_[labels_.at(s.no)]);
            }
        }
        return Program(peak_,static_cast<std::uint32_t>(fn_.param_count),std::move(code_),std::move(host_names_),limits_);
    }
};
}
Program weave(std::string_view source,std::string_view name,const std::string &home,Limits limits) {
    if (source.size()>limits.input_bytes || source.find('\0')!=source.npos || name.find('\0')!=name.npos || home.find('\0')!=home.npos)
        throw std::invalid_argument("vmweave: invalid source size or embedded NUL");
    char error[2048]{};
    std::unique_ptr<metail_context,decltype(&metail_destroy)> ctx(metail_create(home.empty()?nullptr:home.c_str(),error,sizeof error),metail_destroy);
    if (!ctx) throw std::runtime_error(error);
    std::string text(source);metail_module *raw=nullptr;
    auto status=metail_parse(ctx.get(),text.c_str(),"<vmweave>",&raw);
    std::unique_ptr<metail_module,decltype(&metail_module_free)> module(raw,metail_module_free);
    if (status!=METAIL_OK) throw std::invalid_argument(metail_last_error(ctx.get()));
    for (std::size_t i=0;i<module->function_count;++i) if (name==module->functions[i].name)
        return Compiler(module->functions[i],limits).compile();
    throw std::invalid_argument("vmweave: function not found");
}
} // namespace metavm
