#include "MetaTeX.h"
#include <cstdint>
#include <stdexcept>

namespace metatex {
Engine::Glue Engine::glueValue(Register reg) const {
    if(reg.index<0)return {};
    int key=reg.index+static_cast<int>(reg.kind)*256;
    for(std::size_t i=registerScopes_.size();i-->0;)
        if(auto it=registerScopes_[i].find(key);it!=registerScopes_[i].end())return std::get<Glue>(it->second);
    return {};
}

std::optional<Engine::Glue> Engine::scanGlue(){
    if(expansionDepth_>=128)throw std::runtime_error("nested glue scanning limit exceeded");
    struct Depth {std::size_t& value;explicit Depth(std::size_t& v):value(v){++value;}~Depth(){--value;}} guard(expansionDepth_);
    auto diagnostics=document_.diagnostics.size();
    auto token=expanded(true);bool negative=false;
    while(token.kind==TokenKind::Character&&(token.value=="+"||token.value=="-")){
        if(token.value=="-")negative=!negative;
        token=expanded(true);
    }
    Glue glue;std::optional<int> width;
    if(auto reg=registerToken(token)){
        if(reg->index<0||document_.diagnostics.size()!=diagnostics)return std::nullopt;
        if(reg->kind==RegisterKind::Tokens){report("token list cannot be used as glue",true);return std::nullopt;}
        if(reg->kind==RegisterKind::Glue){
            glue=glueValue(*reg);
            if(negative){glue.width=-glue.width;glue.stretch=-glue.stretch;glue.shrink=-glue.shrink;}
            return glue;
        }
        if(reg->kind==RegisterKind::Dimension)width=registerValue(*reg);
        else {insert(Lexer::scan(std::to_string(registerValue(*reg))));width=scanDimension();}
    }else{
        if(token.kind!=TokenKind::Sentinel)insert({token});
        width=scanDimension();
    }
    if(!width)return std::nullopt;
    glue.width=negative?-*width:*width;
    if(scanKeyword("plus")){
        auto stretch=scanDimension(&glue.stretchOrder);if(!stretch)return std::nullopt;
        glue.stretch=*stretch;
    }
    if(scanKeyword("minus")){
        auto shrink=scanDimension(&glue.shrinkOrder);if(!shrink)return std::nullopt;
        glue.shrink=*shrink;
    }
    if(document_.diagnostics.size()!=diagnostics)return std::nullopt;
    return glue;
}

std::string Engine::glueText(const Glue& glue){
    auto component=[](int value,int order){
        auto text=dimensionText(value);
        if(order){text.resize(text.size()-2);text+="fi"+std::string(order,'l');}
        return text;
    };
    auto result=dimensionText(glue.width);
    if(glue.stretch)result+=" plus "+component(glue.stretch,glue.stretchOrder);
    if(glue.shrink)result+=" minus "+component(glue.shrink,glue.shrinkOrder);
    return result;
}

void Engine::arithmeticGlue(Register reg,const std::string& operation){
    Glue result=glueValue(reg);bool overflow=false;
    auto checked=[&](std::int64_t value){
        if(value>1073741823||value<-1073741823){overflow=true;return 0;}
        return static_cast<int>(value);
    };
    if(operation=="advance"){
        auto operand=scanGlue();if(!operand)return;
        result.width=checked(static_cast<std::int64_t>(result.width)+operand->width);
        auto add=[&](int& value,int& order,int incoming,int incomingOrder){
            if(!incoming)return;
            if(!value||incomingOrder>order){value=incoming;order=incomingOrder;}
            else if(incomingOrder==order)value=checked(static_cast<std::int64_t>(value)+incoming);
        };
        add(result.stretch,result.stretchOrder,operand->stretch,operand->stretchOrder);
        add(result.shrink,result.shrinkOrder,operand->shrink,operand->shrinkOrder);
    }else{
        auto diagnostics=document_.diagnostics.size();
        int operand=scanInteger();
        if(document_.diagnostics.size()!=diagnostics)return;
        if(operation=="divide"&&!operand){report("division by zero in register arithmetic",true);return;}
        auto apply=[&](int value){return checked(operation=="multiply"?static_cast<std::int64_t>(value)*operand:static_cast<std::int64_t>(value)/operand);};
        result.width=apply(result.width);result.stretch=apply(result.stretch);result.shrink=apply(result.shrink);
    }
    if(overflow){report("register arithmetic overflow",true);return;}
    storeRegister(reg,result);
}
}
