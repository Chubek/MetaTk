#include "MetaTeX.h"
#include <stdexcept>
namespace metatex {
Lexer::Lexer(std::string source):source_(std::move(source)){
    if(source_.size()>16*1024*1024)throw std::runtime_error("source size limit exceeded");
    for(std::size_t at=0;at<source_.size();){
        unsigned char c=source_[at];
        if(c<128){if(!c)throw std::runtime_error("NUL byte in source");++at;continue;}
        unsigned count=c>=0xC2&&c<=0xDF?2:c>=0xE0&&c<=0xEF?3:c>=0xF0&&c<=0xF4?4:0;
        if(!count||at+count>source_.size())throw std::runtime_error("invalid UTF-8 at byte "+std::to_string(at));
        unsigned cp=c & (0x7F>>count);
        for(unsigned j=1;j<count;++j){unsigned char b=source_[at+j];
            if((b&0xC0)!=0x80)throw std::runtime_error("invalid UTF-8 at byte "+std::to_string(at+j));
            cp=(cp<<6)|(b&63);
        }
        if((count==3&&cp<0x800)||(count==4&&cp<0x10000)||(cp>=0xD800&&cp<=0xDFFF)||cp>0x10FFFF)
            throw std::runtime_error("invalid UTF-8 at byte "+std::to_string(at));
        at+=count;
    }
}
Lexer::Catcodes Lexer::defaultCatcodes(){
    Catcodes codes;codes.fill(12);
    for(int c='A';c<='Z';++c)codes[c]=11;
    for(int c='a';c<='z';++c)codes[c]=11;
    for(int c=128;c<256;++c)codes[c]=11; // Preserve the existing Unicode text extension.
    codes['\\']=0;codes['{']=1;codes['}']=2;codes['$']=3;codes['&']=4;
    codes['\r']=codes['\n']=5;codes['#']=6;codes['^']=7;codes['_']=8;
    codes[0]=9;codes[' ']=codes['\t']=10;codes['%']=14;codes[127]=15;
    return codes;
}
Token Lexer::next(const Catcodes& codes){
    struct Character {std::string value;unsigned cp;std::size_t size,columns;bool newline;};
    auto raw=[&](std::size_t at){
        unsigned char c=source_[at];unsigned count=c<128?1:c<224?2:c<240?3:4;
        unsigned cp=count==1?c:c&(0x7F>>count);
        for(unsigned i=1;i<count;++i)cp=(cp<<6)|(static_cast<unsigned char>(source_[at+i])&63);
        if(c=='\r'&&at+1<source_.size()&&source_[at+1]=='\n')count=2;
        return Character{source_.substr(at,count),cp,count,1,c=='\r'||c=='\n'};
    };
    auto character=[&](){
        auto c=raw(position_);
        auto hex=[](unsigned char ch){return ch>='0'&&ch<='9'?ch-'0':ch>='a'&&ch<='f'?ch-'a'+10:-1;};
        // TeX getnext translates doubled superscript characters before looking
        // up the resulting category, including within control words. Repeated
        // translations consume source bytes, so they cannot cycle.
        while(c.cp<256&&codes[c.cp]==7&&position_+c.size<source_.size()){
            auto second=raw(position_+c.size);
            auto at=position_+c.size+second.size;
            if(second.cp!=c.cp||second.newline||at>=source_.size())break;
            auto third=raw(at);
            if(third.cp>=128||third.newline)break;
            c.size+=second.size+third.size;c.columns+=2;
            int high=hex(static_cast<unsigned char>(third.cp));
            int low=c.size+position_<source_.size()?hex(static_cast<unsigned char>(source_[position_+c.size])):-1;
            if(high>=0&&low>=0){c.cp=16*high+low;++c.size;++c.columns;}
            else c.cp=third.cp<64?third.cp+64:third.cp-64;
            c.value.clear();
            if(c.cp<128)c.value+=static_cast<char>(c.cp);
            else {c.value+=static_cast<char>(0xC0|(c.cp>>6));c.value+=static_cast<char>(0x80|(c.cp&63));}
        }
        return c;
    };
    auto advance=[&](const Character& c){position_+=c.size;if(c.newline){++line_;column_=1;}else column_+=c.columns;};
    auto category=[&](const Character& c){return c.cp<256?codes[c.cp]:11;};
    auto emit=[&](Token token){if(++emitted_>1500000)throw std::runtime_error("source token limit exceeded");return token;};
    while(position_<source_.size()){
        auto c=character();int code=category(c);
        Token token{TokenKind::Character,c.value,position_,line_,column_};token.category=code;
        advance(c);
        if(code==9)continue;
        if(code==15)throw std::runtime_error("invalid category-code character at byte "+std::to_string(token.offset));
        if(code==14){
            while(position_<source_.size()){
                auto rest=raw(position_);advance(rest);
                if(rest.newline)break;
            }
            state_=State::NewLine;continue;
        }
        if(code==5){
            // A category-5 character terminates the physical input line,
            // even when it is not the file's newline character.
            if(!c.newline)while(position_<source_.size()){
                auto rest=raw(position_);advance(rest);
                if(rest.newline)break;
            }
            auto previous=state_;state_=State::NewLine;
            if(previous==State::SkipSpaces)continue;
            token.kind=previous==State::NewLine?TokenKind::Control:TokenKind::Space;
            token.value=previous==State::NewLine?"par":" ";return emit(token);
        }
        if(code==10){
            if(state_!=State::MidLine)continue;
            state_=State::SkipSpaces;token.kind=TokenKind::Space;token.value=" ";return emit(token);
        }
        if(code==0){
            token.kind=TokenKind::Control;token.value.clear();state_=State::SkipSpaces;
            if(position_==source_.size())return emit(token);
            auto next=character();int nextCode=category(next);
            if(nextCode==11){
                do {token.value+=next.value;advance(next);
                    if(position_==source_.size())break;
                    next=character();
                }while(category(next)==11);
            }else{
                token.value=(nextCode==5||nextCode==10)?" ":next.value;advance(next);
                if(nextCode!=5&&nextCode!=10)state_=State::MidLine;
            }
            return emit(token);
        }
        state_=State::MidLine;
        switch(code){
        case 1:token.kind=TokenKind::BeginGroup;break;
        case 2:token.kind=TokenKind::EndGroup;break;
        case 3:token.kind=TokenKind::MathShift;break;
        case 4:token.kind=TokenKind::Alignment;break;
        case 6:token.kind=TokenKind::Parameter;break;
        case 7:token.kind=TokenKind::Superscript;break;
        case 8:token.kind=TokenKind::Subscript;break;
        case 13:token.kind=TokenKind::Control;token.active=true;break;
        default:break;
        }
        return emit(token);
    }
    return {TokenKind::Sentinel,"",position_,line_,column_};
}
std::vector<Token> Lexer::scan(std::string_view source){
    Lexer lexer{std::string(source)};auto codes=defaultCatcodes();std::vector<Token> tokens;
    for(;;){auto token=lexer.next(codes);if(token.kind==TokenKind::Sentinel)break;tokens.push_back(std::move(token));}
    return tokens;
}
std::string Lexer::readLiteral(std::string_view terminator,bool balancedBraces,bool singleLine){
    if(terminator.empty())throw std::invalid_argument("empty literal terminator");
    const auto start=position_;std::size_t depth=0;
    auto advance=[&](){
        unsigned char c=source_[position_++];
        if(c=='\r'){
            if(position_<source_.size()&&source_[position_]=='\n')++position_;
            ++line_;column_=1;
        }else if(c=='\n'){++line_;column_=1;}
        else if((c&0xC0)!=0x80)++column_;
    };
    while(position_<source_.size()){
        if(singleLine&&(source_[position_]=='\r'||source_[position_]=='\n'))break;
        if(!depth&&source_.compare(position_,terminator.size(),terminator)==0){
            auto result=source_.substr(start,position_-start);
            auto end=position_+terminator.size();while(position_<end)advance();
            state_=State::MidLine;return result;
        }
        if(balancedBraces){
            if(source_[position_]=='{')++depth;
            else if(source_[position_]=='}'&&depth)--depth;
        }
        advance();
    }
    throw std::runtime_error("unterminated literal text (expected "+std::string(terminator)+")");
}
std::string Lexer::readLiteralDelimited(){
    if(position_<source_.size()&&source_[position_]=='*'){++position_;++column_;}
    if(position_==source_.size())throw std::runtime_error("missing literal delimiter");
    unsigned char delimiter=source_[position_];
    if(delimiter<=32||delimiter>=127)throw std::runtime_error("literal delimiter must be a non-space ASCII character");
    ++position_;++column_;return readLiteral(std::string(1,delimiter),false,true);
}
}
