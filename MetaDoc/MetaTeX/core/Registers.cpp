#include "MetaTeX.h"
#include <limits>

namespace metatex {
std::optional<int> Engine::countAlias(const std::string& name) const {
    for(std::size_t i=scopes_.size();i-->0;){
        if(auto it=countAliasScopes_[i].find(name);it!=countAliasScopes_[i].end())return it->second;
        if(scopes_[i].contains(name)||commandScopes_[i].contains(name))return std::nullopt;
    }
    return std::nullopt;
}
int Engine::countValue(int index) const {
    for(std::size_t i=countScopes_.size();i-->0;)
        if(auto it=countScopes_[i].find(index);it!=countScopes_[i].end())return it->second;
    return 0;
}
int Engine::scanCountIndex(){
    int index=scanInteger();
    if(index<0||index>255){report("count register index must be 0..255",true);return -1;}
    return index;
}
int Engine::scanCountRegister(){
    Token token=expanded(true);
    if(token.kind==TokenKind::Control){
        if(auto index=countAlias(token.value))return *index;
        if(token.value=="count")return scanCountIndex();
    }
    report("expected count register",true);
    if(token.kind!=TokenKind::Sentinel)insert({token});
    return -1;
}
void Engine::scanOptionalEquals(){
    auto token=expanded(true);
    if(token.kind!=TokenKind::Character||token.value!="=")
        if(token.kind!=TokenKind::Sentinel)insert({token});
}
void Engine::storeCount(int index,int value){
    if(index<0)return;
    if(globalAssignment_){for(auto& scope:countScopes_)scope.erase(index);countScopes_.front()[index]=value;}
    else countScopes_.back()[index]=value;
}
void Engine::assignCount(int index){scanOptionalEquals();int value=scanInteger();storeCount(index,value);}
void Engine::defineCount(std::string name,int index){
    if(index<0)return;
    std::size_t level=scopes_.size()-1;
    if(globalAssignment_){
        for(auto& scope:scopes_)scope.erase(name);
        for(auto& scope:commandScopes_)scope.erase(name);
        for(auto& scope:countAliasScopes_)scope.erase(name);
        level=0;
    }else scopes_.back().erase(name);
    commandScopes_[level][name]=[index](Engine& engine){engine.assignCount(index);};
    countAliasScopes_[level][std::move(name)]=index;
}
void Engine::installRegisters(){
    registerCommand("catcode",[](Engine& engine){engine.assignCatcode();});
    registerCommand("count",[](Engine& engine){int index=engine.scanCountIndex();engine.assignCount(index);});
    registerCommand("countdef",[](Engine& engine){
        auto name=engine.pop(true);
        if(name.kind!=TokenKind::Control){engine.report("\\countdef expects a control sequence",true);return;}
        engine.scanOptionalEquals();int index=engine.scanCountIndex();engine.defineCount(name.value,index);
    });
    for(const char* operation:{"advance","multiply","divide"}){
        registerCommand(operation,[operation=std::string(operation)](Engine& engine){
            int index=engine.scanCountRegister();if(index<0)return;
            // TeX's optional keyword scanner expands tokens and ignores case.
            auto first=engine.expanded(true);
            if(first.kind==TokenKind::Character&&(first.value=="b"||first.value=="B")){
                auto second=engine.expanded();
                if(second.kind!=TokenKind::Character||(second.value!="y"&&second.value!="Y"))engine.insert({first,second});
            }else if(first.kind!=TokenKind::Sentinel)engine.insert({first});
            int operand=engine.scanInteger();std::int64_t value=engine.countValue(index);
            if(operation=="advance")value+=operand;
            else if(operation=="multiply")value*=operand;
            else {
                if(!operand){engine.report("division by zero in register arithmetic",true);return;}
                value/=operand;
            }
            // TeX integers use the symmetric range -(2^31-1)..2^31-1.
            if(value>std::numeric_limits<int>::max()||value<-std::int64_t(std::numeric_limits<int>::max())){
                engine.report("register arithmetic overflow",true);return;
            }
            engine.storeCount(index,static_cast<int>(value));
        });
    }
    registerCommand("global",[](Engine& engine){
        Token next;
        do {next=engine.expanded(true);}while(next.kind==TokenKind::Control&&next.value=="global");
        if(next.kind!=TokenKind::Control||
           (next.value!="def"&&next.value!="gdef"&&next.value!="edef"&&next.value!="xdef"&&
            next.value!="count"&&next.value!="countdef"&&next.value!="advance"&&next.value!="multiply"&&
            next.value!="divide"&&next.value!="catcode"&&!engine.countAlias(next.value))){
            engine.report("\\global must precede an assignment",true);return;
        }
        bool previous=engine.globalAssignment_;engine.globalAssignment_=true;
        try {engine.command(next.value);}catch(...){engine.globalAssignment_=previous;throw;}
        engine.globalAssignment_=previous;
    });
}
void Engine::assignCatcode(){
    int character=scanInteger();scanOptionalEquals();int code=scanInteger();
    if(character<0||character>255){report("category-code character must be 0..255",true);return;}
    if(code<0||code>15){report("category code must be 0..15",true);return;}
    if(globalAssignment_){for(auto& saved:catcodeSaves_)saved.erase(character);}
    else if(!catcodeSaves_.empty())catcodeSaves_.back().try_emplace(character,catcodes_[character]);
    catcodes_[character]=code;
}
}
