#include "qscript/QScript.h"
#include <map>
#include <set>
#include <stdexcept>

namespace qscript {
Result<metail::Program> ast_to_ir(const AST& ast,Options o) {
    Result<metail::Program> out; out.diagnostics=sema_check(ast,o); if (!out.diagnostics.empty()) return out;
    metail::Program p; std::vector<std::map<std::string,unsigned>> scopes(1); std::set<unsigned> free;
    for (unsigned n=0;n<16;++n) free.insert(n);
    auto lookup=[&](const std::string& name){for (auto it=scopes.rbegin();it!=scopes.rend();++it) { auto found=it->find(name); if (found!=it->end()) return found->second; } throw std::invalid_argument("undefined variable");};
    auto emit=[&](metail::Instruction i) {
        if (p.size()>=o.toolchain.il.max_instructions) throw std::invalid_argument("instruction limit exceeded");
        p.code.push_back(i); return p.size()-1;
    };
    auto reg=[](unsigned n){return static_cast<std::uint8_t>(n);};
    std::function<void(const dsl::ASTNode&,unsigned)> expr;
    expr=[&](const auto& n,unsigned target) {
        if (target>=32) throw std::invalid_argument("expression requires more than 16 temporary registers");
        if (n.tag()=="integer") { emit({metail::Op::Const,reg(target),0,0,*metachain::text::signed_integer(n.value())}); return; }
        if (n.tag()=="name") { emit({metail::Op::Mov,reg(target),reg(lookup(n.value())),0,0}); return; }
        expr(n.child(0),target);
        if (n.size()==1) {
            if (target+1>=32) throw std::invalid_argument("expression temporary limit exceeded");
            emit({metail::Op::Const,reg(target+1),0,0,n.tag()=="invert"?-1:0});
            if (n.tag()=="neg") emit({metail::Op::Sub,reg(target),reg(target+1),reg(target),0});
            else emit({n.tag()=="not"?metail::Op::Eq:metail::Op::Xor,reg(target),reg(target),reg(target+1),0});
            return;
        }
        expr(n.child(1),target+1);
        static const std::map<std::string_view,metail::Op> ops{{"+",metail::Op::Add},{"-",metail::Op::Sub},{"*",metail::Op::Mul},{"/",metail::Op::Div},{"%",metail::Op::Mod},{"&",metail::Op::And},{"|",metail::Op::Or},{"^",metail::Op::Xor},{"<<",metail::Op::Shl},{">>",metail::Op::Shr},{"==",metail::Op::Eq},{"!=",metail::Op::Eq},{"<",metail::Op::Lt},{">",metail::Op::Lt},{"<=",metail::Op::Lt},{">=",metail::Op::Lt}};
        bool reverse=n.tag()==">"||n.tag()=="<=";
        emit({ops.at(n.tag()),reg(target),reg(target+(reverse?1:0)),reg(target+(reverse?0:1)),0});
        if (n.tag()=="!="||n.tag()=="<="||n.tag()==">=") {
            emit({metail::Op::Const,reg(target+1),0,0,0}); emit({metail::Op::Eq,reg(target),reg(target),reg(target+1),0});
        }
    };
    auto enter=[&]{scopes.emplace_back();};
    auto leave=[&]{for (auto [name,r]:scopes.back()) { (void)name; free.insert(r); } scopes.pop_back();};
    std::function<void(const dsl::ASTNode&)> stmt;
    stmt=[&](const auto& n) {
        if (n.tag()=="program"||n.tag()=="block") { enter(); for (const auto& c:n.children()) stmt(c); leave(); }
        else if (n.tag()=="let"||n.tag()=="assign") {
            expr(n.child(1),16); unsigned r;
            if (n.tag()=="let") {
                if (free.empty()) throw std::invalid_argument("expression requires more than 16 temporary registers");
                r=*free.begin(); free.erase(free.begin()); scopes.back()[n.child(0).value()]=r;
            } else r=lookup(n.child(0).value());
            emit({metail::Op::Mov,reg(r),16,0,0});
        } else if (n.tag()=="print"||n.tag()=="return") { expr(n.child(0),16); emit({n.tag()=="print"?metail::Op::Print:metail::Op::Halt,16,0,0,0}); }
        else if (n.tag()=="if") {
            expr(n.child(0),16); auto branch=emit({metail::Op::Jz,16,0,0,0}); enter(); stmt(n.child(1)); leave();
            if (n.size()==3) {
                auto end=emit({metail::Op::Jmp,0,0,0,0}); p.code[branch].immediate=static_cast<std::int64_t>(p.size());
                enter(); stmt(n.child(2)); leave(); p.code[end].immediate=static_cast<std::int64_t>(p.size());
            } else p.code[branch].immediate=static_cast<std::int64_t>(p.size());
        } else if (n.tag()=="while") {
            auto start=p.size(); expr(n.child(0),16); auto branch=emit({metail::Op::Jz,16,0,0,0});
            enter(); stmt(n.child(1)); leave(); emit({metail::Op::Jmp,0,0,0,static_cast<std::int64_t>(start)});
            p.code[branch].immediate=static_cast<std::int64_t>(p.size());
        }
    };
    try {
        stmt(ast.root); emit({metail::Op::Const,16,0,0,0}); emit({metail::Op::Halt,16,0,0,0});
        return verify_ir(std::move(p),o);
    } catch (const std::invalid_argument& e) { out.diagnostics.push_back({metachain::Stage::Lower,e.what(),{},0,0,0}); }
    return out;
}
} // namespace qscript
