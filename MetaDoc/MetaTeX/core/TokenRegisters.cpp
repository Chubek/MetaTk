#include "MetaTeX.h"

namespace metatex {
Engine::TokenList Engine::tokenListValue(Register reg) const {
    if(reg.index<0)return {};
    int key=reg.index+static_cast<int>(reg.kind)*256;
    for(std::size_t i=registerScopes_.size();i-->0;)
        if(auto it=registerScopes_[i].find(key);it!=registerScopes_[i].end())return std::get<TokenList>(it->second);
    return {};
}
void Engine::assignTokenList(Register reg){
    auto diagnostics=document_.diagnostics.size();
    auto token=expanded(true);
    if(auto source=registerToken(token)){
        if(source->kind!=RegisterKind::Tokens){report("expected token-list register or braced token list",true);return;}
        if(source->index>=0&&document_.diagnostics.size()==diagnostics)storeRegister(reg,tokenListValue(*source));
    }else if(token.kind==TokenKind::BeginGroup){
        insert({token});auto tokens=readArgument();
        if(document_.diagnostics.size()==diagnostics)storeRegister(reg,std::make_shared<const std::vector<Token>>(std::move(tokens)));
    }else{
        report("expected token-list register or braced token list",true);
        if(token.kind!=TokenKind::Sentinel)insert({token});
    }
}
}
