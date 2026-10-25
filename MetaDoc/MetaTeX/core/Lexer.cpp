#include "MetaTeX.h"
#include <algorithm>
#include <stdexcept>
namespace metatex {
std::vector<Token> Lexer::scan(std::string_view s) {
    if(s.size()>16*1024*1024)throw std::runtime_error("source size limit exceeded");
    // Reject malformed UTF-8 before scanning control words as well as text.
    for(std::size_t at=0;at<s.size();){
        unsigned char c=static_cast<unsigned char>(s[at]);
        if(c<128){if(c==0)throw std::runtime_error("NUL byte in source");++at;continue;}
        unsigned count=c>=0xC2&&c<=0xDF?2:c>=0xE0&&c<=0xEF?3:c>=0xF0&&c<=0xF4?4:0;
        if(!count||at+count>s.size())throw std::runtime_error("invalid UTF-8 at byte "+std::to_string(at));
        unsigned cp=c & (0x7F>>count);
        for(unsigned j=1;j<count;++j){unsigned char b=static_cast<unsigned char>(s[at+j]);
            if((b&0xC0)!=0x80)throw std::runtime_error("invalid UTF-8 at byte "+std::to_string(at+j));
            cp=(cp<<6)|(b&63);
        }
        if((count==3&&cp<0x800)||(count==4&&cp<0x10000)||(cp>=0xD800&&cp<=0xDFFF)||cp>0x10FFFF)
            throw std::runtime_error("invalid UTF-8 at byte "+std::to_string(at));
        at+=count;
    }
    std::vector<Token> out;
    std::size_t i=0, line=1, col=1;
    bool afterControlWord=false;
    auto emit=[&](TokenKind k, std::string v, std::size_t at, std::size_t ln, std::size_t cl) {
        if(out.size()>=1500000)throw std::runtime_error("source token limit exceeded");
        out.push_back({k,std::move(v),at,ln,cl});
    };
    auto letter=[](unsigned char c) {return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c>=128;};
    while(i<s.size()) {
        std::size_t at=i, ln=line, cl=col;
        unsigned char c=static_cast<unsigned char>(s[i]);
        if (c=='\r') {++i; if(i<s.size() && s[i]=='\n') ++i; ++line; col=1; emit(TokenKind::EndLine,"\n",at,ln,cl); afterControlWord=false; continue;}
        if (c=='\n') {++i; ++line; col=1; emit(TokenKind::EndLine,"\n",at,ln,cl); afterControlWord=false; continue;}
        if (c=='%') {while(i<s.size() && s[i]!='\n' && s[i]!='\r') {++i;++col;} continue;}
        if (c=='\\') {
            ++i; ++col;
            if(i==s.size()) {emit(TokenKind::Control,"",at,ln,cl);break;}
            std::string name;
            if(letter(static_cast<unsigned char>(s[i]))) {
                while(i<s.size() && letter(static_cast<unsigned char>(s[i]))) {name+=s[i++];++col;}
                afterControlWord=true;
            } else {
                if(s[i]=='\n' || s[i]=='\r') {
                    name=" ";if(s[i++]=='\r' && i<s.size() && s[i]=='\n')++i;
                    ++line;col=1;
                } else {name+=s[i++];++col;}
                afterControlWord=false;
            }
            emit(TokenKind::Control,std::move(name),at,ln,cl);continue;
        }
        if(c==' ' || c=='\t') {
            while(i<s.size() && (s[i]==' '||s[i]=='\t')) {++i;++col;}
            if(!afterControlWord) emit(TokenKind::Space," ",at,ln,cl);
            afterControlWord=false;continue;
        }
        afterControlWord=false;
        TokenKind kind=TokenKind::Character;
        switch(c) {case '{':kind=TokenKind::BeginGroup;break;case '}':kind=TokenKind::EndGroup;break;
            case '$':kind=TokenKind::MathShift;break;case '&':kind=TokenKind::Alignment;break;
            case '#':kind=TokenKind::Parameter;break;case '^':kind=TokenKind::Superscript;break;
            case '_':kind=TokenKind::Subscript;break;default:break;}
        std::string val;
        if(c>=0x80) { // Preserve a complete UTF-8 codepoint.
            int count=(c&0xE0)==0xC0?2:(c&0xF0)==0xE0?3:(c&0xF8)==0xF0?4:1;
            count=static_cast<int>(std::min<std::size_t>(count,s.size()-i));
            val.assign(s.substr(i,count));i+=count;++col;
        } else {val=static_cast<char>(c);++i;++col;}
        emit(kind,std::move(val),at,ln,cl);
    }
    return out;
}
}
