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
    struct Character {std::string value;unsigned cp;std::size_t size;};
    auto character=[&](){
        unsigned char c=source_[position_];unsigned count=c<128?1:c<224?2:c<240?3:4;
        unsigned cp=count==1?c:c&(0x7F>>count);
        for(unsigned i=1;i<count;++i)cp=(cp<<6)|(static_cast<unsigned char>(source_[position_+i])&63);
        if(c=='\r'&&position_+1<source_.size()&&source_[position_+1]=='\n')count=2;
        return Character{source_.substr(position_,count),cp,count};
    };
    auto advance=[&](const Character& c){position_+=c.size;if(c.cp=='\r'||c.cp=='\n'){++line_;column_=1;}else ++column_;};
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
                auto rest=character();advance(rest);
                if(rest.cp=='\r'||rest.cp=='\n')break;
            }
            state_=State::NewLine;continue;
        }
        if(code==5){
            // A category-5 character terminates the physical input line,
            // even when it is not the file's newline character.
            if(c.cp!='\r'&&c.cp!='\n')while(position_<source_.size()){
                auto rest=character();advance(rest);
                if(rest.cp=='\r'||rest.cp=='\n')break;
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
}
