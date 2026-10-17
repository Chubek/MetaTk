#include "qscript/QScript.h"
namespace qscript {
bool visit(const AST& ast,const std::function<bool(const dsl::ASTNode&)>& fn,std::size_t limit) {
    std::vector<std::pair<const dsl::ASTNode*,std::size_t>> work{{&ast.root,0}};
    while (!work.empty()) {
        auto [node,depth]=work.back(); work.pop_back(); if (depth>limit) return false;
        if (!fn(*node)||node->is_leaf()) continue;
        for (auto it=node->children().rbegin();it!=node->children().rend();++it) work.emplace_back(&*it,depth+1);
    }
    return true;
}
} // namespace qscript
