#include "MetaTeX.h"
#include <limits>
#include <stdexcept>

namespace metatex {
namespace {
bool space(const Token& token){return token.kind==TokenKind::Space||token.kind==TokenKind::EndLine;}
std::vector<Token> characters(std::string_view text,const Token& location){
    std::vector<Token> result;
    for(char c:text){
        Token token=location;token.kind=c==' '?TokenKind::Space:TokenKind::Character;
        token.value=std::string(1,c);token.expansionProtected=false;token.active=false;
        token.category=c==' '?10:12;result.push_back(std::move(token));
    }
    return result;
}
}

Token Engine::expanded(bool skipSpace,bool definition){
    for(;;){
        Token token=pop();
        if(token.definitionOpaque)return token;
        if(expandToken(token,definition))continue;
        if(skipSpace&&space(token))continue;
        return token;
    }
}

bool Engine::expandToken(const Token& token,bool definition){
    if(token.kind!=TokenKind::Control||token.expansionProtected)return false;
    if(const auto* macro=lookup(token.controlName())){auto copy=*macro;expand(copy);return true;}
    // Locally installed plugin commands are unexpandable, even when they shadow
    // the name of a primitive. Expansion never executes arbitrary handlers.
    for(const auto& scope:commandScopes_)if(scope.contains(token.controlName()))return false;
    const auto name=token.controlName();
    if(!expandablePrimitives_.contains(name))return false;
    if(expansionDepth_>=128)throw std::runtime_error("nested expansion limit exceeded");
    struct Depth {std::size_t& value;explicit Depth(std::size_t& v):value(v){++value;}~Depth(){--value;}} guard(expansionDepth_);
    if(name=="noexpand"){
        auto next=pop();
        if(next.kind==TokenKind::Sentinel)report("missing token after \\noexpand",true);
        else {
            bool localCommand=false;
            for(const auto& scope:commandScopes_)if(scope.contains(next.controlName()))localCommand=true;
            next.expansionProtected=next.kind==TokenKind::Control&&
                (lookup(next.controlName())||!hasCommand(next.controlName())||(!localCommand&&expandablePrimitives_.contains(next.controlName())));
            insert({next});
        }
    }else if(name=="expandafter"){
        auto first=pop(),second=pop();
        if(first.kind==TokenKind::Sentinel||second.kind==TokenKind::Sentinel)report("missing token after \\expandafter",true);
        else {if(!expandToken(second))insert({second});insert({first});}
    }else if(name=="csname"){
        std::string control;
        for(;;){
            auto next=expanded();
            if(next.kind==TokenKind::Control&&next.value=="endcsname")break;
            if(next.kind==TokenKind::Sentinel){report("unterminated \\csname",true);return true;}
            if(next.kind==TokenKind::Control){report("non-character token in \\csname",true);insert({next});return true;}
            control+=space(next)?" ":next.value;
        }
        if(!hasCommand(control))defineCommand(control,[](Engine&){});
        auto result=token;result.value=std::move(control);insert({result});
    }else if(name=="string"){
        auto next=pop();
        if(next.kind==TokenKind::Sentinel)report("missing token after \\string",true);
        else insert(characters((next.kind==TokenKind::Control&&!next.active?"\\":"")+next.value,token));
    }else if(name=="the"){
        auto internal=expanded(true);
        if(internal.kind==TokenKind::Control&&!internal.active&&internal.value=="catcode"){
            int character=scanInteger();
            if(character<0||character>255)report("category-code character must be 0..255",true);
            else insert(characters(std::to_string(catcodes_[character]),token));
        }else{
            if(internal.kind!=TokenKind::Sentinel)insert({internal});
            auto reg=scanRegister();
            if(reg&&reg->index>=0){
                if(reg->kind==RegisterKind::Tokens){
                    auto stored=tokenListValue(*reg);
                    auto tokens=stored?*stored:std::vector<Token>{};
                    if(definition)for(auto& item:tokens)item.definitionOpaque=true;
                    insert(std::move(tokens));return true;
                }
                int value=registerValue(*reg);
                auto text=reg->kind==RegisterKind::Glue?glueText(glueValue(*reg)):
                    reg->kind==RegisterKind::Dimension?dimensionText(value):std::to_string(value);
                insert(characters(text,token));
            }
        }
    }else if(name=="number"||name=="romannumeral"){
        int value=scanInteger();std::string result;
        if(name=="number")result=std::to_string(value);
        else {
            static const int values[]={1000,900,500,400,100,90,50,40,10,9,5,4,1};
            static const char* digits[]={"m","cm","d","cd","c","xc","l","xl","x","ix","v","iv","i"};
            for(int i=0;i<13;++i)while(value>=values[i]){
                if(result.size()>=1500000)throw std::runtime_error("roman numeral expansion limit exceeded");
                result+=digits[i];value-=values[i];
            }
        }
        insert(characters(result,token));
    }else if(name=="iftrue"||name=="iffalse")conditional(name=="iftrue");
    else if(name=="ifdefined"){
        auto next=pop();conditional(next.kind==TokenKind::Control&&hasCommand(next.controlName()));
    }else if(name=="else"){
        if(conditions_.size()>conditionalFloor_)conditions_.back()=true;
        finishConditional(true);
    }else if(name=="fi"){
        finishConditional(false);
    }else if(name=="ifnum"){
        int left=scanInteger();auto relation=expanded(true);int right=scanInteger();
        if(relation.value!="<"&&relation.value!=">"&&relation.value!="=")report("invalid comparison in \\ifnum",true);
        conditional(relation.value=="<"?left<right:relation.value==">"?left>right:relation.value=="="&&left==right);
    }else if(name=="ifdim"){
        auto left=scanDimension();auto relation=expanded(true);auto right=scanDimension();
        if(relation.value!="<"&&relation.value!=">"&&relation.value!="=")report("invalid comparison in \\ifdim",true);
        conditional(left&&right&&(relation.value=="<"?*left<*right:relation.value==">"?*left>*right:relation.value=="="&&*left==*right));
    }
    return true;
}

int Engine::scanInteger(){
    if(expansionDepth_>=128)throw std::runtime_error("nested integer scanning limit exceeded");
    struct Depth {std::size_t& value;explicit Depth(std::size_t& v):value(v){++value;}~Depth(){--value;}} guard(expansionDepth_);
    bool negative=false;Token token=expanded(true);
    while(token.kind==TokenKind::Character&&(token.value=="+"||token.value=="-")){
        if(token.value=="-")negative=!negative;
        token=expanded(true);
    }
    if(token.kind==TokenKind::Control){
        if(!token.active&&token.value=="catcode"){
            int character=scanInteger();
            if(character<0||character>255){report("category-code character must be 0..255",true);return 0;}
            return negative?-catcodes_[character]:catcodes_[character];
        }
        if(auto reg=registerToken(token)){
            if(reg->kind==RegisterKind::Tokens){report("token list cannot be used as an integer",true);return 0;}
            int value=registerValue(*reg);
            return negative?-value:value;
        }
    }
    if(token.kind==TokenKind::Character&&token.value=="`"){
        auto character=pop();
        if(character.value.size()!=1){report("invalid alphabetic integer constant",true);return 0;}
        int value=static_cast<unsigned char>(character.value[0]);
        if(space(peek()))pop();
        return negative?-value:value;
    }
    int radix=10;
    if(token.kind==TokenKind::Character&&(token.value=="'"||token.value=="\"")){
        radix=token.value=="'"?8:16;token=expanded();
    }
    int value=0;bool any=false,overflow=false;
    for(;;){
        int digit=-1;
        if(token.kind==TokenKind::Character&&token.value.size()==1){
            char c=token.value[0];
            if(c>='0'&&c<='9')digit=c-'0';
            else if(c>='A'&&c<='F')digit=c-'A'+10;
        }
        if(digit<0||digit>=radix)break;
        any=true;
        if(value>(std::numeric_limits<int>::max()-digit)/radix)overflow=true;
        else if(!overflow)value=value*radix+digit;
        token=expanded();
    }
    if(!space(token)&&token.kind!=TokenKind::Sentinel)insert({token});
    if(!any)report("missing integer",true);
    if(overflow){report("integer overflow",true);value=std::numeric_limits<int>::max();}
    return negative?-value:value;
}

void Engine::installExpansion(){
    for(const char* name:{"noexpand","expandafter","string","number","romannumeral","the",
                         "iftrue","iffalse","ifdefined","ifnum","ifdim","csname","else","fi"}){
        registerCommand(name,[](Engine&){}); // Names exposed to plugins/REPL; expanded by the scanner.
        expandablePrimitives_.insert(name);
    }
}
}
