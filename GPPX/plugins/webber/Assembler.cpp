/** Webber reference two-pass assembler and extension plugin.
 * ISA is an illustrative stack VM format, defined in CATALOGUE.yaml.
 * Data directives are evaluated in pass 1 for layout and pass 2 for emission.
 */
#include "Assembler.hpp"
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using Bytes=std::vector<unsigned char>;
constexpr size_t kMaxProgram=65536;
std::string trim(const std::string& s) {
    auto a=s.find_first_not_of(" \t\r\n");
    if(a==s.npos)return {};
    return s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);
}
std::string upper(std::string s) {
    for(char& c:s)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
std::string lower(std::string s) {
    for(char& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
[[noreturn]] void fail(size_t line,const std::string& msg) {
    throw std::runtime_error("Webber line "+std::to_string(line)+": "+msg);
}
bool identifier(const std::string& name) {
    if(name.empty()||(!std::isalpha(static_cast<unsigned char>(name[0]))&&name[0]!='_'))return false;
    for(char c:name)if(!std::isalnum(static_cast<unsigned char>(c))&&c!='_')return false;
    return true;
}
std::string stripComment(const std::string& text) {
    bool quoted=false,escape=false;
    for(size_t i=0;i<text.size();++i) {
        char c=text[i];
        if(quoted){if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quoted=false;}
        else if(c=='"')quoted=true;
        else if(c==';')return text.substr(0,i);
    }
    return text;
}
std::vector<std::string> comma(const std::string& s,size_t line) {
    std::vector<std::string> fields;
    bool quoted=false,escape=false;
    size_t begin=0;
    for(size_t i=0;i<s.size();++i) {
        char c=s[i];
        if(quoted) {if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quoted=false;}
        else if(c=='"')quoted=true;
        else if(c==',') {
            auto item=trim(s.substr(begin,i-begin));
            if(item.empty())fail(line,"empty list element");
            fields.push_back(item);begin=i+1;
        }
    }
    if(quoted)fail(line,"unterminated quoted operand");
    auto last=trim(s.substr(begin));
    if(last.empty())fail(line,"expected operand");
    fields.push_back(last);return fields;
}
long long integer(const std::string& s,size_t line) {
    auto token=trim(s);
    if(token.empty())fail(line,"integer operand is missing");
    char* end=nullptr;errno=0;
    long long val=std::strtoll(token.c_str(),&end,0);
    if(errno==ERANGE||end==token.c_str()||*end)fail(line,"invalid integer: "+token);
    return val;
}
long long expression(const std::string& s,const std::map<std::string,size_t>& symbols,size_t line) {
    auto token=trim(s);
    if(token.empty())fail(line,"empty expression");
    if(std::isdigit(static_cast<unsigned char>(token[0]))||token[0]=='-'||token[0]=='+')return integer(token,line);
    size_t end=0;
    while(end<token.size() && (std::isalnum(static_cast<unsigned char>(token[end]))||token[end]=='_'))++end;
    auto label=token.substr(0,end);
    if(!identifier(label))fail(line,"invalid symbol: "+label);
    auto it=symbols.find(label);
    if(it==symbols.end())fail(line,"undefined symbol: "+label);
    long long v=static_cast<long long>(it->second);
    auto suffix=trim(token.substr(end));
    if(suffix.empty())return v;
    char op=suffix[0];
    if(op!='+'&&op!='-')fail(line,"expected +offset or -offset: "+suffix);
    long long offset=integer(suffix.substr(1),line);
    if(offset<0||offset>static_cast<long long>(kMaxProgram))fail(line,"offset out of range");
    return op=='+'?v+offset:v-offset;
}
unsigned char byte(long long v,size_t line) {
    if(v<-128||v>255)fail(line,"byte out of range: "+std::to_string(v));
    return static_cast<unsigned char>(v);
}
void word(Bytes& out,long long v,size_t line) {
    if(v<-32768||v>65535)fail(line,"16-bit value out of range: "+std::to_string(v));
    out.push_back(static_cast<unsigned char>(v));
    out.push_back(static_cast<unsigned char>(v>>8));
}
Bytes quoted(const std::string& operand,size_t line) {
    auto s=trim(operand);
    if(s.size()<2||s.front()!='"'||s.back()!='"')fail(line,"expected double-quoted string");
    Bytes result;
    for(size_t i=1;i+1<s.size();++i) {
        unsigned char c=static_cast<unsigned char>(s[i]);
        if(c=='\\') {
            if(i+2>=s.size())fail(line,"incomplete string escape");
            char x=s[++i];
            if(x=='n')c='\n';else if(x=='r')c='\r';else if(x=='t')c='\t';
            else if(x=='0')c=0;else if(x=='"'||x=='\\')c=static_cast<unsigned char>(x);
            else if(x=='x') {
                if(i+2>=s.size()-1)fail(line,"\\x escape requires two hexadecimal digits");
                auto hex=[](char h)->int {
                    if(h>='0'&&h<='9')return h-'0';
                    if(h>='a'&&h<='f')return h-'a'+10;
                    if(h>='A'&&h<='F')return h-'A'+10;
                    return -1;
                };
                int hi=hex(s[++i]),lo=hex(s[++i]);
                if(hi<0||lo<0)fail(line,"invalid hex escape");
                c=static_cast<unsigned char>((hi<<4)|lo);
            } else fail(line,"unsupported string escape: "+std::string(1,x));
        }
        result.push_back(c);
    }
    return result;
}
struct Stmt {size_t line,pc,length;std::string op,arg;};
size_t length(const std::string& op,const std::string& arg,size_t pc,size_t line) {
    static const std::vector<std::string> noarg={"NOP","DUP","DROP","SWAP","ADD","SUB","MUL","DIV","MOD","PRINT","HALT"};
    if(std::find(noarg.begin(),noarg.end(),op)!=noarg.end()) {
        if(!arg.empty())fail(line,op+" takes no operand");
        return 1;
    }
    if(op=="PUSH"||op=="JMP"||op=="JZ"||op=="JNZ") {
        if(arg.empty())fail(line,op+" requires one operand");
        return op=="PUSH"?2:3;
    }
    if(op==".BYTE"||op==".WORD")return comma(arg,line).size()*(op==".BYTE"?1:2);
    if(op==".ASCII"||op==".ASCIZ")return quoted(arg,line).size()+(op==".ASCIZ"?1:0);
    if(op==".SPACE"||op==".ORG"||op==".ALIGN") {
        auto val=integer(arg,line);
        if(val<0||val>static_cast<long long>(kMaxProgram))fail(line,"layout argument out of range");
        if(op==".SPACE")return static_cast<size_t>(val);
        if(op==".ORG") {
            if(static_cast<size_t>(val)<pc)fail(line,".ORG cannot move backward");
            return static_cast<size_t>(val)-pc;
        }
        if(!val || (val&(val-1)) || val>4096)fail(line,".ALIGN requires power of two <=4096");
        return (static_cast<size_t>(val)-pc%static_cast<size_t>(val))%static_cast<size_t>(val);
    }
    fail(line,"unknown opcode or data directive: "+op);
}
}
namespace webber {
std::vector<unsigned char> assemble(std::string_view source) {
    std::map<std::string,size_t> symbols;
    std::vector<Stmt> instructions;
    std::istringstream input{std::string(source)};
    std::string line;size_t lineno=0,pc=0;
    while(std::getline(input,line)) {
        ++lineno;line=trim(stripComment(line));
        if(line.empty())continue;
        // Support consecutive labels and labels followed by instructions.
        while(!line.empty()) {
            auto colon=line.find(':');
            auto ws=line.find_first_of(" \t");
            if(colon==line.npos||(ws!=line.npos&&ws<colon))break;
            auto label=line.substr(0,colon);
            if(!identifier(label))fail(lineno,"invalid label: "+label);
            if(!symbols.emplace(label,pc).second)fail(lineno,"duplicate label or constant: "+label);
            line=trim(line.substr(colon+1));
        }
        if(line.empty())continue;
        auto space=line.find_first_of(" \t");
        auto op=upper(line.substr(0,space));
        auto arg=space==line.npos?std::string():trim(line.substr(space+1));
        if(op==".EQU") {
            auto fields=comma(arg,lineno);
            if(fields.size()!=2||!identifier(fields[0]))fail(lineno,".EQU requires NAME, integer");
            long long val=integer(fields[1],lineno);
            if(val<0||val>65535)fail(lineno,".EQU value outside 0..65535");
            if(!symbols.emplace(fields[0],static_cast<size_t>(val)).second)fail(lineno,"duplicate symbol: "+fields[0]);
            continue;
        }
        size_t n=length(op,arg,pc,lineno);
        if(n>kMaxProgram-pc)fail(lineno,"program exceeds 65536 bytes");
        instructions.push_back({lineno,pc,n,op,arg});pc+=n;
    }
    Bytes out;out.reserve(pc);
    for(const auto& instr:instructions) {
        const auto& o=instr.op;const auto& a=instr.arg;
        if(out.size()!=instr.pc)fail(instr.line,"internal layout mismatch");
        if(o=="NOP")out.push_back(0x00);
        else if(o=="PUSH") {out.push_back(0x10);out.push_back(byte(expression(a,symbols,instr.line),instr.line));}
        else if(o=="DUP")out.push_back(0x11);
        else if(o=="DROP")out.push_back(0x12);
        else if(o=="SWAP")out.push_back(0x13);
        else if(o=="ADD")out.push_back(0x20);
        else if(o=="SUB")out.push_back(0x21);
        else if(o=="MUL")out.push_back(0x22);
        else if(o=="DIV")out.push_back(0x23);
        else if(o=="MOD")out.push_back(0x24);
        else if(o=="PRINT")out.push_back(0x30);
        else if(o=="JMP"||o=="JZ"||o=="JNZ") {
            out.push_back(o=="JMP"?0x40:o=="JZ"?0x41:0x42);
            word(out,expression(a,symbols,instr.line),instr.line);
        }
        else if(o=="HALT")out.push_back(0xFF);
        else if(o==".BYTE"||o==".WORD") {
            for(const auto& item:comma(a,instr.line)) {
                auto v=expression(item,symbols,instr.line);
                if(o==".BYTE")out.push_back(byte(v,instr.line));
                else word(out,v,instr.line);
            }
        }
        else if(o==".ASCII"||o==".ASCIZ") {
            auto data=quoted(a,instr.line);out.insert(out.end(),data.begin(),data.end());
            if(o==".ASCIZ")out.push_back(0);
        }
        else if(o==".SPACE"||o==".ORG"||o==".ALIGN")out.insert(out.end(),instr.length,0);
        if(out.size()!=instr.pc+instr.length)fail(instr.line,"internal opcode size mismatch");
    }
    return out;
}
std::string asCArray(const std::vector<unsigned char>& bytes) {
    static const char* hex="0123456789ABCDEF";
    std::string result;result.reserve(bytes.size()*6);
    for(auto b:bytes) {
        if(!result.empty())result+=", ";
        result+="0x";result+=hex[b>>4];result+=hex[b&15];
    }
    return result;
}
}
namespace {
char* duplicate(const std::string& s) {
    char* ptr=static_cast<char*>(std::malloc(s.size()+1));
    if(ptr)std::memcpy(ptr,s.c_str(),s.size()+1);
    return ptr;
}
int upperDirective(GPPX_State*,const char* input,char** out,void*) {
    *out=duplicate(upper(input?input:""));return *out?0:1;
}
int lowerDirective(GPPX_State*,const char* input,char** out,void*) {
    *out=duplicate(lower(input?input:""));return *out?0:1;
}
int upperMacro(GPPX_State*,int argc,const char* const* args,char** out,void*) {
    if(argc!=1)return 1;*out=duplicate(upper(args[0]));return *out?0:1;
}
int versionMacro(GPPX_State*,int argc,const char* const*,char** out,void*) {
    if(argc!=0)return 1;*out=duplicate("1");return *out?0:1;
}
int asmEmbed(GPPX_State*,const unsigned char* data,size_t size,char** out,void*) {
    *out=nullptr;
    try {
        if(!data && size)return 1;
        auto result=webber::assemble(std::string_view(reinterpret_cast<const char*>(data?data:reinterpret_cast<const unsigned char*>("")),size));
        *out=duplicate(webber::asCArray(result));return *out?0:1;
    } catch (...) {return 1;}
}
int asmHexEmbed(GPPX_State*,const unsigned char* data,size_t size,char** out,void*) {
    *out=nullptr;
    try {
        if(!data && size)return 1;
        auto result=webber::assemble(std::string_view(reinterpret_cast<const char*>(data?data:reinterpret_cast<const unsigned char*>("")),size));
        static const char* hex="0123456789ABCDEF";
        std::string str;str.reserve(result.size()*2);
        for(auto b:result){str+=hex[b>>4];str+=hex[b&15];}
        *out=duplicate(str);return *out?0:1;
    } catch (...) {return 1;}
}
int contains(GPPX_State*,const char* pattern,const char* text,void*) {
    if(!pattern||!text)return -1;
    return std::strstr(text,pattern)?1:0;
}
}
#if defined(_WIN32)
#define WEBBER_EXPORT extern "C" __declspec(dllexport)
#else
#define WEBBER_EXPORT extern "C"
#endif
WEBBER_EXPORT int gppx_plugin_init(const GPPX_HostAPI* host,GPPX_State* state) {
    if(!host||!state||host->abi_version!=GPPX_ABI_VERSION)return 1;
    return host->register_directive(state,"wbr_upper",upperDirective,nullptr)
        || host->register_directive(state,"wbr_lower",lowerDirective,nullptr)
        || host->register_macro(state,"WBR_UPPER",upperMacro,nullptr)
        || host->register_macro(state,"WBR_VERSION",versionMacro,nullptr)
        || host->register_embed(state,"wbr_asm",asmEmbed,nullptr)
        || host->register_embed(state,"wbr_hex",asmHexEmbed,nullptr)
        || host->register_pmatch(state,"wbr_contains",contains,nullptr);
}
