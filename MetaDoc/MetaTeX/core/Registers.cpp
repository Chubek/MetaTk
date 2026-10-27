#include "MetaTeX.h"
#include <limits>

namespace metatex {
std::optional<Engine::Register> Engine::registerAlias(const std::string& name) const {
    for(std::size_t i=scopes_.size();i-->0;){
        if(auto it=registerAliasScopes_[i].find(name);it!=registerAliasScopes_[i].end())return it->second;
        if(scopes_[i].contains(name)||commandScopes_[i].contains(name))return std::nullopt;
    }
    return std::nullopt;
}
int Engine::registerValue(Register reg) const {
    if(reg.index<0)return 0;
    int key=reg.index+static_cast<int>(reg.kind)*256;
    for(std::size_t i=registerScopes_.size();i-->0;)
        if(auto it=registerScopes_[i].find(key);it!=registerScopes_[i].end()){
            if(auto number=std::get_if<int>(&it->second))return *number;
            if(auto glue=std::get_if<Glue>(&it->second))return glue->width;
            return 0;
        }
    return 0;
}
int Engine::scanRegisterIndex(){
    int index=scanInteger();
    if(index<0||index>255){report("register index must be 0..255",true);return -1;}
    return index;
}
std::optional<Engine::Register> Engine::registerToken(const Token& token){
    if(token.kind!=TokenKind::Control||token.expansionProtected)return std::nullopt;
    auto name=token.controlName();
    if(auto reg=registerAlias(name))return reg;
    for(const auto& scope:commandScopes_)if(scope.contains(name))return std::nullopt;
    if(!registerPrimitives_.contains(name))return std::nullopt;
    return Register{name=="dimen"?RegisterKind::Dimension:name=="skip"?RegisterKind::Glue:name=="toks"?RegisterKind::Tokens:RegisterKind::Count,scanRegisterIndex()};
}
std::optional<Engine::Register> Engine::scanRegister(){
    Token token=expanded(true);
    if(auto reg=registerToken(token))return reg;
    report("expected register",true);
    if(token.kind!=TokenKind::Sentinel)insert({token});
    return std::nullopt;
}
void Engine::scanOptionalEquals(){
    auto token=expanded(true);
    if(token.kind!=TokenKind::Character||token.value!="=")
        if(token.kind!=TokenKind::Sentinel)insert({token});
}
bool Engine::scanKeyword(std::string_view keyword){
    std::vector<Token> read;
    for(std::size_t i=0;i<keyword.size();++i){
        auto token=expanded(i==0);
        if(token.kind!=TokenKind::Sentinel)read.push_back(token);
        auto lower=[](char c){return c>='A'&&c<='Z'?c+'a'-'A':c;};
        if(token.kind!=TokenKind::Character||token.value.size()!=1||lower(token.value[0])!=keyword[i]){
            insert(std::move(read));return false;
        }
    }
    return true;
}
void Engine::storeRegister(Register reg,RegisterValue value){
    if(reg.index<0)return;
    int key=reg.index+static_cast<int>(reg.kind)*256;
    if(globalAssignment_){for(auto& scope:registerScopes_)scope.erase(key);registerScopes_.front()[key]=value;}
    else registerScopes_.back()[key]=value;
}
void Engine::assignRegister(Register reg){
    scanOptionalEquals();
    if(reg.kind==RegisterKind::Tokens)assignTokenList(reg);
    else if(reg.kind==RegisterKind::Glue){auto value=scanGlue();if(value)storeRegister(reg,*value);}
    else if(reg.kind==RegisterKind::Dimension){auto value=scanDimension();if(value)storeRegister(reg,*value);}
    else {int value=scanInteger();storeRegister(reg,value);}
}
void Engine::defineRegister(std::string name,Register reg){
    if(reg.index<0)return;
    std::size_t level=scopes_.size()-1;
    if(globalAssignment_){
        for(auto& scope:scopes_)scope.erase(name);
        for(auto& scope:commandScopes_)scope.erase(name);
        for(auto& scope:registerAliasScopes_)scope.erase(name);
        level=0;
    }else scopes_.back().erase(name);
    commandScopes_[level][name]=[reg](Engine& engine){engine.assignRegister(reg);};
    registerAliasScopes_[level][std::move(name)]=reg;
}
void Engine::installRegisters(){
    registerCommand("catcode",[](Engine& engine){engine.assignCatcode();});
    for(auto kind:{RegisterKind::Count,RegisterKind::Dimension,RegisterKind::Glue,RegisterKind::Tokens}){
        std::string name=kind==RegisterKind::Count?"count":kind==RegisterKind::Dimension?"dimen":kind==RegisterKind::Glue?"skip":"toks";
        registerCommand(name,[kind](Engine& engine){int index=engine.scanRegisterIndex();engine.assignRegister({kind,index});});
        registerPrimitives_.insert(name);
        registerCommand(name+"def",[kind,name](Engine& engine){
            auto destination=engine.pop(true);
            if(destination.kind!=TokenKind::Control){engine.report("\\"+name+"def expects a control sequence",true);return;}
            engine.scanOptionalEquals();int index=engine.scanRegisterIndex();engine.defineRegister(destination.controlName(),{kind,index});
        });
    }
    for(const char* operation:{"advance","multiply","divide"}){
        registerCommand(operation,[operation=std::string(operation)](Engine& engine){
            auto reg=engine.scanRegister();if(!reg||reg->index<0)return;
            if(reg->kind==RegisterKind::Tokens){engine.report("arithmetic is not defined for token-list registers",true);return;}
            engine.scanKeyword("by");
            if(reg->kind==RegisterKind::Glue){engine.arithmeticGlue(*reg,operation);return;}
            auto diagnostics=engine.document_.diagnostics.size();
            std::optional<int> operand;
            if(operation=="advance"&&reg->kind==RegisterKind::Dimension)operand=engine.scanDimension();
            else operand=engine.scanInteger();
            if(!operand||engine.document_.diagnostics.size()!=diagnostics)return;
            std::int64_t value=engine.registerValue(*reg);
            if(operation=="advance")value+=*operand;
            else if(operation=="multiply")value*=*operand;
            else {
                if(!*operand){engine.report("division by zero in register arithmetic",true);return;}
                value/=*operand;
            }
            const std::int64_t limit=reg->kind==RegisterKind::Dimension?1073741823:std::numeric_limits<int>::max();
            if(value>limit||value<-limit){engine.report("register arithmetic overflow",true);return;}
            engine.storeRegister(*reg,static_cast<int>(value));
        });
    }
    registerCommand("global",[](Engine& engine){
        Token next;
        do {next=engine.expanded(true);}while(next.kind==TokenKind::Control&&next.value=="global");
        if(next.kind!=TokenKind::Control||
           (next.value!="def"&&next.value!="gdef"&&next.value!="edef"&&next.value!="xdef"&&next.value!="let"&&
            next.value!="count"&&next.value!="countdef"&&next.value!="dimen"&&next.value!="dimendef"&&
            next.value!="skip"&&next.value!="skipdef"&&
            next.value!="toks"&&next.value!="toksdef"&&
            next.value!="advance"&&next.value!="multiply"&&next.value!="divide"&&next.value!="catcode"&&
            !engine.registerAlias(next.controlName()))){
            engine.report("\\global must precede an assignment",true);return;
        }
        bool previous=engine.globalAssignment_;engine.globalAssignment_=true;
        try {engine.command(next.controlName());}catch(...){engine.globalAssignment_=previous;throw;}
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
