#include "MetaTeX.h"
#include <cstdint>
#include <stdexcept>

namespace metatex {
namespace {
bool space(const Token& token){return token.kind==TokenKind::Space||token.kind==TokenKind::EndLine;}
int digit(const Token& token){
    return token.kind==TokenKind::Character&&token.category==12&&token.value.size()==1&&
        token.value[0]>='0'&&token.value[0]<='9'?token.value[0]-'0':-1;
}
}

std::optional<int> Engine::scanDimension(int* order){
    if(expansionDepth_>=128)throw std::runtime_error("nested dimension scanning limit exceeded");
    struct Depth {std::size_t& value;explicit Depth(std::size_t& v):value(v){++value;}~Depth(){--value;}} guard(expansionDepth_);
    auto diagnostics=document_.diagnostics.size();
    if(order)*order=0;
    bool negative=false;auto token=expanded(true);
    while(token.kind==TokenKind::Character&&(token.value=="+"||token.value=="-")){
        if(token.value=="-")negative=!negative;
        token=expanded(true);
    }
    std::int64_t whole=0;int fraction=0;
    if(auto reg=registerToken(token)){
        if(reg->index<0||document_.diagnostics.size()!=diagnostics)return std::nullopt;
        if(reg->kind==RegisterKind::Tokens){report("token list cannot be used as a dimension",true);return std::nullopt;}
        int value=registerValue(*reg);
        if(reg->kind!=RegisterKind::Count)return negative?-value:value;
        whole=value;
    }else if(digit(token)>=0||(token.kind==TokenKind::Character&&(token.value=="."||token.value==","))){
        while(digit(token)>=0){
            whole=whole*10+digit(token);
            if(whole>2147483647){
                report("dimension coefficient overflow",true);
                // Continue scanning digits without overflowing the accumulator.
                do {token=expanded();}while(digit(token)>=0);
                whole=2147483647;break;
            }
            token=expanded();
        }
        if(token.kind==TokenKind::Character&&(token.value=="."||token.value==",")){
            std::vector<int> digits;
            token=expanded();
            while(digit(token)>=0){
                if(digits.size()<17)digits.push_back(digit(token));
                token=expanded();
            }
            // TeX's rounddecimals: round to a 16-bit fraction using at most
            // seventeen decimal places, independent of host floating point.
            for(auto it=digits.rbegin();it!=digits.rend();++it)fraction=(fraction+*it*131072)/10;
            fraction=(fraction+1)/2;
        }
        if(!space(token)&&token.kind!=TokenKind::Sentinel)insert({token});
    }else{
        if(token.kind!=TokenKind::Sentinel)insert({token});
        whole=scanInteger(); // Radix/character constants and internal integers.
    }
    if(whole<0){negative=!negative;whole=-whole;}
    std::int64_t value=0;
    if(order&&scanKeyword("fil")){
        *order=1;
        while(scanKeyword("l")){
            if(*order==3){report("glue order exceeds filll",true);return std::nullopt;}
            ++*order;
        }
        value=whole*65536+fraction;
        if(negative)value=-value;
        if(value>1073741823||value<-1073741823){report("dimension overflow",true);return std::nullopt;}
        if(document_.diagnostics.size()!=diagnostics)return std::nullopt;
        return static_cast<int>(value);
    }
    auto unit=expanded(true);
    if(auto reg=registerToken(unit)){
        if(reg->index<0)return std::nullopt;
        if(reg->kind==RegisterKind::Tokens){report("token list cannot be used as a dimension unit",true);return std::nullopt;}
        auto scale=static_cast<std::int64_t>(registerValue(*reg));
        value=whole*scale+(fraction*scale)/65536;
    }else{
        if(unit.kind!=TokenKind::Sentinel)insert({unit});
        struct Unit {const char* name;int numerator,denominator;};
        static constexpr Unit units[]={{"pt",1,1},{"in",7227,100},{"pc",12,1},
            {"cm",7227,254},{"mm",7227,2540},{"bp",7227,7200},{"dd",1238,1157},{"cc",14856,1157}};
        bool found=false;
        if(scanKeyword("sp")){value=whole;found=true;}
        else for(const auto& candidate:units)if(scanKeyword(candidate.name)){
            value=((whole*65536+fraction)*candidate.numerator)/candidate.denominator;
            found=true;break;
        }
        if(!found){report("missing or unsupported dimension unit (expected pt, sp, in, pc, cm, mm, bp, dd, cc or an internal quantity)",true);return std::nullopt;}
        // Consume at most one expanded space following an explicit unit.
        auto following=expanded();
        if(!space(following)&&following.kind!=TokenKind::Sentinel)insert({following});
    }
    if(negative)value=-value;
    if(value>1073741823||value<-1073741823){report("dimension overflow",true);return std::nullopt;}
    if(document_.diagnostics.size()!=diagnostics)return std::nullopt;
    return static_cast<int>(value);
}

std::string Engine::dimensionText(int value){
    std::string result;
    if(value<0){result="-";value=-value;}
    result+=std::to_string(value/65536)+".";
    int remainder=10*(value%65536)+5,delta=10;
    // TeX's printscaled rounding yields the shortest round-trippable decimal
    // under its own fixed-point scanner, always including a decimal digit.
    do {
        if(delta>65536)remainder-=17232;
        result+=static_cast<char>('0'+remainder/65536);
        remainder=10*(remainder%65536);delta*=10;
    }while(remainder>delta);
    return result+"pt";
}
}
