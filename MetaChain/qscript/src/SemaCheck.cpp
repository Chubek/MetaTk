#include "qscript/QScript.h"
#include <map>
#include <set>
#include <string>

namespace qscript {
std::vector<Diagnostic> sema_check(const AST& ast,Options o) {
    std::vector<Diagnostic> errors; std::vector<std::set<std::string>> scopes(1); std::size_t count=0,live=0;
    auto fail=[&](std::string message){errors.push_back({metachain::Stage::Verify,std::move(message),{},0,0,0});};
    auto defined=[&](const std::string& name){for (auto it=scopes.rbegin();it!=scopes.rend();++it) if (it->count(name)) return true; return false;};
    std::function<void(const dsl::ASTNode&,std::size_t,bool)> check;
    check=[&](const auto& n,std::size_t d,bool expression) {
        if (++count>o.max_nodes||d>std::min<std::size_t>(o.max_depth,256)) { fail("AST resource limit exceeded"); return; }
        auto tag=n.tag();
        if (expression) {
            if (n.is_leaf()&&tag=="integer"&&metachain::text::signed_integer(n.value())) return;
            if (n.is_leaf()&&tag=="name") { if (!defined(n.value())) fail("undefined variable: "+n.value()); return; }
            static const std::set<std::string_view> binary{"+","-","*","/","%","&","|","^","<<",">>","==","!=","<",">","<=",">="};
            bool unary=tag=="neg"||tag=="not"||tag=="invert";
            if (n.is_leaf()||(!unary&&!binary.count(tag))||n.size()!=(unary?1u:2u)) { fail("invalid expression AST"); return; }
            for (const auto& c:n.children()) check(c,d+1,true);
            return;
        }
        if (n.is_leaf()) { fail("expected statement AST"); return; }
        if (tag=="program"||tag=="block") {
            scopes.emplace_back();
            for (const auto& c:n.children()) check(c,d+1,false);
            live-=scopes.back().size(); scopes.pop_back(); return;
        }
        if ((tag=="let"||tag=="assign")&&n.size()==2&&n.child(0).is_leaf()&&n.child(0).tag()=="name") {
            const auto& name=n.child(0).value(); check(n.child(1),d+1,true);
            if (!metachain::text::identifier(name)) { fail("invalid variable name"); return; }
            if (tag=="let") {
                if (!scopes.back().insert(name).second) fail("duplicate variable: "+name);
                else if (++live>16) fail("QScript supports at most 16 simultaneously live variables");
            } else if (!defined(name)) fail("assignment to undefined variable: "+name);
            return;
        }
        if ((tag=="print"||tag=="return")&&n.size()==1) { check(n.child(0),d+1,true); return; }
        if ((tag=="while"&&n.size()==2)||(tag=="if"&&(n.size()==2||n.size()==3))) {
            check(n.child(0),d+1,true);
            for (std::size_t k=1;k<n.size();++k) { scopes.emplace_back(); check(n.child(k),d+1,false); live-=scopes.back().size(); scopes.pop_back(); }
            return;
        }
        fail("invalid statement AST");
    };
    if (ast.root.tag()!="program") fail("expected program AST"); else check(ast.root,0,false);
    return errors;
}
} // namespace qscript
