#include "GPPX.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace fs = std::filesystem;
using std::string;
using std::string_view;
namespace {
bool ident0(char c) {return std::isalpha((unsigned char)c)||c=='_';}
bool ident(char c) {return std::isalnum((unsigned char)c)||c=='_';}
string trim(string s) {size_t a=s.find_first_not_of(" \t\r\n"); if(a==string::npos)return {};size_t b=s.find_last_not_of(" \t\r\n");return s.substr(a,b-a+1);}
string left(string s) {size_t a=s.find_first_not_of(" \t");return a==string::npos?string():s.substr(a);}
bool starts(const string& s,const string& p) {return s.compare(0,p.size(),p)==0;}
string quote(string_view s) {string o="\"";for(unsigned char c:s){switch(c){case '\\':o+="\\\\";break;case '"':o+="\\\"";break;case '\n':o+="\\n";break;case '\r':o+="\\r";break;case '\t':o+="\\t";break;default:if(c<32||c>=127){o+='\\';o+=char('0'+(c>>6));o+=char('0'+((c>>3)&7));o+=char('0'+(c&7));}else o+=char(c);}}return o+"\"";}
string unquote(string s) {s=trim(s);if(s.size()<2||!((s.front()=='"'&&s.back()=='"')||(s.front()=='\''&&s.back()=='\'')))return s;string out;for(size_t i=1;i+1<s.size();++i){char c=s[i];if(c=='\\'&&i+2<s.size()){c=s[++i];switch(c){case 'n':c='\n';break;case 'r':c='\r';break;case 't':c='\t';break;}}out+=c;}return out;}
std::vector<string> linesOf(string_view src) {std::vector<string> out;size_t begin=0;while(begin<src.size()){auto pos=src.find('\n',begin);if(pos==string_view::npos){out.emplace_back(src.substr(begin));break;}out.emplace_back(src.substr(begin,pos-begin+1));begin=pos+1;}return out;}
string stripEol(string s) {if(!s.empty()&&s.back()=='\n')s.pop_back();if(!s.empty()&&s.back()=='\r')s.pop_back();return s;}
string lower(string x){for(auto& c:x)c=char(std::tolower((unsigned char)c));return x;}
string basenameExtension(const fs::path& p) {return lower(p.extension().string());}
string binaryRead(const fs::path& p){std::ifstream f(p,std::ios::binary);if(!f)throw gppx::Error("cannot read file: "+p.string());std::ostringstream o;o<<f.rdbuf();if(!f.eof()&&f.fail())throw gppx::Error("read error: "+p.string());return o.str();}
string hexBytes(std::string_view x,bool array){static const char* h="0123456789ABCDEF";string o;for(size_t i=0;i<x.size();++i){unsigned char b=(unsigned char)x[i];if(array){if(i)o+=", ";o+="0x";}o+=h[b>>4];o+=h[b&15];}return o;}
string base64(std::string_view x){static constexpr char table[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";string o;for(size_t i=0;i<x.size();i+=3){unsigned a=(unsigned char)x[i],b=i+1<x.size()?(unsigned char)x[i+1]:0,c=i+2<x.size()?(unsigned char)x[i+2]:0;unsigned v=(a<<16)|(b<<8)|c;o+=table[(v>>18)&63];o+=table[(v>>12)&63];o+=i+1<x.size()?table[(v>>6)&63]:'=';o+=i+2<x.size()?table[v&63]:'=';}return o;}
std::pair<string,string> takeName(const string& input){size_t i=0;while(i<input.size()&&ident(input[i]))++i;return {input.substr(0,i),input.substr(i)};}
/* Split top-level commas, retaining parentheses and quoted strings. */
std::vector<string> splitArgs(string_view s){std::vector<string> v;int depth=0;char q=0;bool escape=false;size_t begin=0;for(size_t i=0;i<s.size();++i){char c=s[i];if(q){if(escape)escape=false;else if(c=='\\')escape=true;else if(c==q)q=0;continue;}if(c=='\''||c=='"'){q=c;continue;}if(c=='('||c=='['||c=='{')++depth;else if(c==')'||c==']'||c=='}')--depth;else if(c==','&&depth==0){v.push_back(trim(string(s.substr(begin,i-begin))));begin=i+1;}}v.push_back(trim(string(s.substr(begin))));if(v.size()==1&&v[0].empty())v.clear();return v;}
bool parseCall(const string& src,size_t open,size_t& end,std::vector<string>& args){if(open>=src.size()||src[open]!='(')return false;int depth=0;char q=0;bool escape=false;for(size_t i=open;i<src.size();++i){char c=src[i];if(q){if(escape)escape=false;else if(c=='\\')escape=true;else if(c==q)q=0;continue;}if(c=='\''||c=='"'){q=c;continue;}if(c=='(')++depth;else if(c==')'){if(--depth==0){end=i+1;args=splitArgs(string_view(src).substr(open+1,i-open-1));return true;}}}return false;}
string replaceParams(string_view body,const std::vector<string>& names,const std::vector<string>& args){std::unordered_map<string,string> vars;for(size_t i=0;i<names.size();++i)vars[names[i]]=i<args.size()?args[i]:"";string output;for(size_t i=0;i<body.size();){char c=body[i];if(c=='"'||c=='\''){size_t start=i++;bool esc=false;while(i<body.size()){char x=body[i++];if(esc)esc=false;else if(x=='\\')esc=true;else if(x==c)break;}output+=string(body.substr(start,i-start));continue;}if(c=='#'&&i+1<body.size()&&body[i+1]!='#'){size_t p=i+1;while(p<body.size()&&(body[p]==' '||body[p]=='\t'))++p;if(p<body.size()&&ident0(body[p])){size_t j=p+1;while(j<body.size()&&ident(body[j]))++j;auto it=vars.find(string(body.substr(p,j-p)));if(it!=vars.end()){output+=quote(it->second);i=j;continue;}}}if(c=='#'&&i+1<body.size()&&body[i+1]=='#'){while(!output.empty()&&std::isspace((unsigned char)output.back()))output.pop_back();i+=2;while(i<body.size()&&std::isspace((unsigned char)body[i]))++i;continue;}if(ident0(c)){size_t j=i+1;while(j<body.size()&&ident(body[j]))++j;auto k=string(body.substr(i,j-i));auto it=vars.find(k);output+=it==vars.end()?k:it->second;i=j;}else{output+=c;++i;}}return output;}
}

struct GPPX_State {gppx::Engine* engine=nullptr;bool owned=false;string error;string report;};
namespace gppx {
struct Engine::Impl {
 struct LoadedLibraries {
   std::vector<void*> handles;
   ~LoadedLibraries(){
     for(auto handle:handles){
#if defined(_WIN32)
       FreeLibrary((HMODULE)handle);
#else
       dlclose(handle);
#endif
     }
   }
 };
 /* First field is destroyed last, *after* callback registrations. */
 LoadedLibraries libraries;
 struct Macro {bool function=false;bool variadic=false;std::vector<string> params;string value;};
 struct Rewrite {std::regex pattern;string replacement;};
 struct Frame {bool parent=true, active=false, taken=false, elseSeen=false;};
 Engine* self;
 GPPX_State state;
 Syntax syntax;
 std::map<string,Macro> macros;
 std::map<string,std::function<string(std::string_view)>> directives;
 std::map<string,std::function<string(const std::vector<string>&)>> callables;
 std::vector<std::function<string(std::string_view)>> rewriters;
 std::map<string,std::function<string(const fs::path&)>> readers;
 std::map<string,std::function<void(std::string_view)>> sinks;
 std::map<string,std::function<string(std::string_view)>> embeds;
 std::map<string,std::function<void(std::string_view)>> syntaxes;
 std::map<string,std::function<bool(std::string_view,std::string_view)>> pmatches;
 std::vector<Rewrite> regexRules;
 std::vector<fs::path> includeDirs;
 std::set<fs::path> once, imported;
 std::vector<fs::path> activeFiles;
 std::vector<Diagnostic> logs;
 std::map<string,std::vector<std::pair<bool,Macro>>> macroStacks;
  size_t line=1,counter=0,bytesProduced=0,maxOutput=16*1024*1024,maxDepth=64,maxIncludes=64,processDepth=0,callDepth=0;
 string path="<stdin>",lastErr;
 bool strict=true;
 explicit Impl(Engine* owner):self(owner){state.engine=owner;}
 [[noreturn]] void fail(string message){logs.push_back({path,line,message,false});lastErr=path+":"+std::to_string(line)+": "+message;throw Error(lastErr);}
 void warning(string message){logs.push_back({path,line,message,true});}
 void outputAppend(string& out,const string& data){if(bytesProduced>maxOutput||data.size()>maxOutput-bytesProduced)fail("output limit exceeded");bytesProduced+=data.size();out+=data;}
 fs::path resolve(const string& input) {
   fs::path p=unquote(input);
   std::vector<fs::path> candidates;
   if(p.is_absolute())candidates.push_back(p);
   else {if(!path.empty()&&path[0]!='<')candidates.push_back(fs::path(path).parent_path()/p);for(auto& inc:includeDirs)candidates.push_back(inc/p);candidates.push_back(p);}
   for(auto& x:candidates){std::error_code ec;if(fs::is_regular_file(x,ec)){auto canonical=fs::weakly_canonical(x,ec);return ec?fs::absolute(x):canonical;}}
   fail("file not found: "+input);
 }
  bool declared(const string& k)const{return macros.count(k)||callables.count(k)||k=="__FILE__"||k=="__LINE__"||k=="__COUNTER__"||k=="__GPPX__";}
  string invoke(const string& name,std::vector<string> args,size_t depth,const std::set<string>& busy) {
    if(callDepth>=maxDepth)fail("function nesting limit exceeded");
    ++callDepth;
    struct CallRestore {size_t& depth;~CallRestore(){--depth;}} callRestore{callDepth};
    auto fn=callables.find(name);
    if(fn!=callables.end()){
      /* Callbacks may replace their own registration. Keep the invoked target alive. */
      auto callback=fn->second;
      for(auto& arg:args)arg=expand(arg,depth+1,busy);
      return callback(args);
    }
    auto it=macros.find(name);
    if(it==macros.end()||!it->second.function)fail("unknown function: "+name);
    Macro def=it->second;
    size_t fixed=def.params.size()-(def.variadic?1:0);
    if(args.empty()&&fixed==1)args.emplace_back();
    if((!def.variadic&&args.size()!=def.params.size())||(def.variadic&&args.size()<fixed))fail("wrong number of macro arguments: "+name);
    std::vector<string> actual;
    for(size_t a=0;a<fixed;++a)actual.push_back(expand(args[a],depth+1,busy));
    if(def.variadic){string extra;for(size_t a=fixed;a<args.size();++a){if(a>fixed)extra+=", ";extra+=expand(args[a],depth+1,busy);}actual.push_back(extra);}
    return replaceParams(def.value,def.params,actual);
  }
 string expand(const string& s,size_t depth=0,std::set<string> busy={},bool *commentState=nullptr) {
   if(depth>maxDepth)fail("macro expansion depth exceeded");
   bool localComment=false;bool& block=commentState?*commentState:localComment;
    string out;
    for(size_t i=0;i<s.size();){
      if(out.size()>maxOutput)fail("output limit exceeded during expansion");
     if(block){if(!syntax.block_close.empty()&&s.compare(i,syntax.block_close.size(),syntax.block_close)==0){out+=syntax.block_close;i+=syntax.block_close.size();block=false;}else out+=s[i++];continue;}
     if(!syntax.block_open.empty()&&s.compare(i,syntax.block_open.size(),syntax.block_open)==0){out+=syntax.block_open;i+=syntax.block_open.size();block=true;continue;}
     if(!syntax.line_comment.empty()&&s.compare(i,syntax.line_comment.size(),syntax.line_comment)==0){out+=s.substr(i);break;}
     if(s[i]=='"'||s[i]=='\''){
       char q=s[i];size_t j=i+1;bool esc=false,closed=false;for(;j<s.size();++j){char c=s[j];if(esc)esc=false;else if(c=='\\')esc=true;else if(c==q){closed=true;++j;break;}}
       if(syntax.expand_strings){string inside=s.substr(i+1,j-i-1-(closed?1:0));out+=q;out+=expand(inside,depth+1,busy);if(closed)out+=q;}
       else out+=s.substr(i,j-i);
       i=j;continue;
     }
     if(!syntax.macro_open.empty()){
       if(s.compare(i,syntax.macro_open.size(),syntax.macro_open)!=0){out+=s[i++];continue;}
       size_t begin=i+syntax.macro_open.size(),end=string::npos;
       char quoteChar=0;bool escape=false;size_t nesting=0;
       for(size_t j=begin;j<s.size();){
         char current=s[j];
         if(quoteChar){
           if(escape)escape=false;
           else if(current=='\\')escape=true;
           else if(current==quoteChar)quoteChar=0;
           ++j;continue;
         }
         if(current=='\''||current=='"'){quoteChar=current;++j;continue;}
         if(syntax.macro_close==")"){
           if(current=='('){++nesting;++j;continue;}
           if(current==')'){
             if(nesting==0){end=j;break;}
             --nesting;
           }
           ++j;continue;
         }
         if(s.compare(j,syntax.macro_open.size(),syntax.macro_open)==0){
           ++nesting;j+=syntax.macro_open.size();continue;
         }
         if(s.compare(j,syntax.macro_close.size(),syntax.macro_close)==0){
           if(nesting==0){end=j;break;}
           --nesting;j+=syntax.macro_close.size();continue;
         }
         ++j;
       }
       if(end==string::npos){out+=s[i++];continue;}
       string body=trim(s.substr(begin,end-begin));
       /* Resolve nested explicit macros, then evaluate the enclosing bare expression. */
       body=expand(body,depth+1,busy);
       auto saved=syntax.macro_open;syntax.macro_open.clear();string expanded;
       try{expanded=expand(body,depth+1,busy);}catch(...){syntax.macro_open=saved;throw;}
       syntax.macro_open=saved;out+=expanded;i=end+syntax.macro_close.size();continue;
     }
     if(!ident0(s[i])){out+=s[i++];continue;}
     size_t e=i+1;while(e<s.size()&&ident(s[e]))++e;
     string name=s.substr(i,e-i);
     if(busy.count(name)){out+=name;i=e;continue;}
     if(name=="__FILE__"){out+=quote(path);i=e;continue;}
     if(name=="__LINE__"){out+=std::to_string(line);i=e;continue;}
     if(name=="__COUNTER__"){out+=std::to_string(counter++);i=e;continue;}
     if(name=="__GPPX__"){out+="1";i=e;continue;}
     auto m=macros.find(name);auto fn=callables.find(name);
     if(m==macros.end()&&fn==callables.end()){out+=name;i=e;continue;}
     bool takesArgs=(m!=macros.end()&&m->second.function)||fn!=callables.end();
     std::vector<string> args;size_t after=e;
     if(takesArgs){size_t p=e;while(p<s.size()&&(s[p]==' '||s[p]=='\t'))++p;if(!parseCall(s,p,after,args)){out+=name;i=e;continue;}}
     string replacement;
      if(takesArgs)replacement=invoke(name,args,depth,busy);
      else replacement=m->second.value;
     auto recursiveBusy=busy;recursiveBusy.insert(name);
     out+=expand(replacement,depth+1,std::move(recursiveBusy));i=takesArgs?after:e;
   }
    if(out.size()>maxOutput)fail("output limit exceeded during expansion");
    return out;
 }
 void addMacro(string desc,string value){desc=trim(desc);auto [name,tail]=takeName(desc);if(name.empty()||!ident0(name[0]))fail("invalid macro name: "+desc);Macro m;
   if(!tail.empty()&&tail[0]=='('){size_t end=0;std::vector<string> params;if(!parseCall(tail,0,end,params))fail("unclosed macro parameter list: "+desc);if(!trim(tail.substr(end)).empty())fail("unexpected characters after macro name");m.function=true;
     for(size_t i=0;i<params.size();++i){string param=trim(params[i]);if(param=="..."){if(i+1!=params.size())fail("variadic parameter must be last");m.variadic=true;param="__VA_ARGS__";}if(param.empty()||!ident0(param[0])||!std::all_of(param.begin(),param.end(),ident))fail("invalid parameter: "+param);if(std::find(m.params.begin(),m.params.end(),param)!=m.params.end())fail("duplicate macro parameter: "+param);m.params.push_back(param);}
   }else if(!trim(tail).empty())fail("unexpected tokens after macro name: "+desc);
   m.value=std::move(value);macros[name]=std::move(m);
 }
 /* Replace defined() operands before ordinary macro expansion. */
 string normalizeDefined(const string& expr){string out;for(size_t i=0;i<expr.size();){if(ident0(expr[i])){size_t j=i+1;while(j<expr.size()&&ident(expr[j]))++j;string k=expr.substr(i,j-i);if(k!="defined"){out+=k;i=j;continue;}size_t p=j;while(p<expr.size()&&std::isspace((unsigned char)expr[p]))++p;bool par=p<expr.size()&&expr[p]=='(';if(par)++p;while(p<expr.size()&&std::isspace((unsigned char)expr[p]))++p;size_t a=p;while(p<expr.size()&&ident(expr[p]))++p;string name=expr.substr(a,p-a);while(p<expr.size()&&std::isspace((unsigned char)expr[p]))++p;if(par){if(p==expr.size()||expr[p]!=')')fail("expected ')' after defined()");++p;}if(name.empty())fail("expected identifier after defined");out+=declared(name)?'1':'0';i=p;}else out+=expr[i++];}return out;}
 struct Expr {
   Impl& host;string s;size_t p=0;
   explicit Expr(Impl& h,string text):host(h),s(std::move(text)){}
   void ws(){while(p<s.size()&&std::isspace((unsigned char)s[p]))++p;}
   bool match(const string& tok){ws();if(s.compare(p,tok.size(),tok)==0){p+=tok.size();return true;}return false;}
   string op(){ws();static const char* toks[]={"||","&&","==","!=","<=",">=","<<",">>","|","^","&","<",">","+","-","*","/","%"};for(auto* x:toks)if(s.compare(p,strlen(x),x)==0)return x;return {};}
   static int prec(const string& o){if(o=="||")return 1;if(o=="&&")return 2;if(o=="|")return 3;if(o=="^")return 4;if(o=="&")return 5;if(o=="=="||o=="!=")return 6;if(o=="<"||o==">"||o=="<="||o==">=")return 7;if(o=="<<"||o==">>")return 8;if(o=="+"||o=="-")return 9;return 10;}
    long long atom(bool evaluate){ws();if(match("(")){auto v=value(1,evaluate);if(!match(")"))host.fail("unclosed parenthesis in expression");return v;}if(match("!"))return !atom(evaluate);if(match("~"))return ~atom(evaluate);if(match("+"))return atom(evaluate);if(match("-"))return (long long)(0ULL-(unsigned long long)atom(evaluate));
     ws();if(p>=s.size())host.fail("missing expression operand");
     if(ident0(s[p])){while(p<s.size()&&ident(s[p]))++p;return 0;} /* unknown identifiers evaluate to zero */
     if(s[p]=='\''){++p;if(p>=s.size())host.fail("invalid character literal");int c=(unsigned char)s[p++];if(p>=s.size()||s[p++]!='\'')host.fail("invalid character literal");return evaluate?c:0;}
     const char* first=s.c_str()+p;char* end=nullptr;errno=0;long long v=std::strtoll(first,&end,0);if(end==first||errno==ERANGE)host.fail("invalid integer in expression");p+=(size_t)(end-first);while(p<s.size()&&(s[p]=='u'||s[p]=='U'||s[p]=='l'||s[p]=='L'))++p;return evaluate?v:0;
   }
   long long value(int minPrec,bool evaluate){long long lhs=atom(evaluate);for(;;){ws();string o=op();if(o.empty()||prec(o)<minPrec)break;int precedence=prec(o);p+=o.size();bool rhsEval=evaluate&&!(o=="&&"&&!lhs)&&!(o=="||"&&lhs);long long rhs=value(precedence+1,rhsEval);if(!evaluate)continue;
      if(o=="||")lhs=lhs||rhs;else if(o=="&&")lhs=lhs&&rhs;else if(o=="|")lhs|=rhs;else if(o=="^")lhs^=rhs;else if(o=="&")lhs&=rhs;else if(o=="==")lhs=lhs==rhs;else if(o=="!=")lhs=lhs!=rhs;else if(o=="<")lhs=lhs<rhs;else if(o==">")lhs=lhs>rhs;else if(o=="<=")lhs=lhs<=rhs;else if(o==">=")lhs=lhs>=rhs;else if(o=="<<")lhs=(rhs<0||rhs>=64)?0:(long long)((unsigned long long)lhs<<rhs);
      else if(o==">>")lhs=(rhs<0||rhs>=64)?0:lhs>>rhs;
      else if(o=="+")lhs=(long long)((unsigned long long)lhs+(unsigned long long)rhs);
      else if(o=="-")lhs=(long long)((unsigned long long)lhs-(unsigned long long)rhs);
      else if(o=="*")lhs=(long long)((unsigned long long)lhs*(unsigned long long)rhs);
      else if(o=="/"){if(!rhs)host.fail("division by zero in expression");if(lhs==std::numeric_limits<long long>::min()&&rhs==-1)host.fail("signed division overflow");lhs/=rhs;}
      else if(o=="%"){if(!rhs)host.fail("modulo by zero in expression");if(lhs==std::numeric_limits<long long>::min()&&rhs==-1)host.fail("signed modulo overflow");lhs%=rhs;}
   }
    ws();
    if(minPrec<=1&&p<s.size()&&s[p]=='?'){
      ++p;auto yes=value(1,evaluate&&lhs!=0);
      if(!match(":"))host.fail("expected ':' in ternary expression");
      auto no=value(1,evaluate&&lhs==0);
      if(evaluate)lhs=lhs?yes:no;
    }
    return lhs;
   }
   long long all(){long long v=value(1,true);ws();if(p!=s.size())host.fail("unexpected token in expression: "+s.substr(p));return v;}
 };
 long long eval(const string& x){auto transformed=expand(normalizeDefined(x));return Expr(*this,transformed).all();}
 void setOption(const string& key,const string& value){string k=lower(trim(key)),v=unquote(value);if(k=="directive"||k=="prefix"){if(v.empty())fail("directive prefix must not be empty");syntax.directive=v;}
   else if(k=="macro_style"||k=="macros"){if(v=="bare"||v=="identifiers"){syntax.macro_open.clear();syntax.macro_close=")";}else if(v=="dollar"){syntax.macro_open="$(";syntax.macro_close=")";}else if(v=="mustache"){syntax.macro_open="{{";syntax.macro_close="}}";}else fail("unknown macro style: "+v);}
   else if(k=="macro_open")syntax.macro_open=v;else if(k=="macro_close"){if(v.empty())fail("macro_close must not be empty");syntax.macro_close=v;}
   else if(k=="line_comment")syntax.line_comment=v;else if(k=="block_open")syntax.block_open=v;else if(k=="block_close")syntax.block_close=v;
    else if(k=="expand_strings"||k=="strict"){
      if(v!="true"&&v!="1"&&v!="yes"&&v!="false"&&v!="0"&&v!="no")fail("invalid boolean for "+k);
      bool enabled=v=="true"||v=="1"||v=="yes";
      if(k=="strict")strict=enabled;else syntax.expand_strings=enabled;
    }
    else if(k=="max_depth"||k=="max_output"||k=="max_includes"){
      size_t n=0;auto result=std::from_chars(v.data(),v.data()+v.size(),n);
      if(result.ec!=std::errc()||result.ptr!=v.data()+v.size()||n==0)fail("invalid positive integer for "+k);
      if(k=="max_depth")maxDepth=n;else if(k=="max_output")maxOutput=n;else maxIncludes=n;
    }
    else if(k=="profile"){
      if(v!="c"&&v!="percent"&&v!="template"&&v!="jackscript")fail("unknown syntax profile: "+v);
      syntax=Syntax();
      if(v=="percent")syntax.directive="%";
      else if(v=="jackscript"){
        syntax.macro_open="$(";
        self->registerReader(".jcks",[](const fs::path& file){
          char* data=nullptr;
          if(gppx_utf8_load(file.string().c_str(),&data))throw Error("cannot read UTF-8 Jackscript source: "+file.string());
          std::unique_ptr<char,decltype(&gppx_free)> owned(data,&gppx_free);
          return string(data);
        });
      }else if(v=="template"){
        syntax.directive="@";syntax.macro_open="{{";syntax.macro_close="}}";
        syntax.line_comment.clear();syntax.block_open.clear();syntax.block_close.clear();
      }
    }
   else {auto f=syntaxes.find(k);if(f==syntaxes.end())fail("unknown syntax option: "+k);f->second(v);}
 }
 string rewrite(string input){for(auto& x:regexRules)input=std::regex_replace(input,x.pattern,x.replacement);for(auto& cb:rewriters)input=cb(input);return input;}
  string directiveResult(string s,bool hadNewline){if(s.empty())return {};if(hadNewline&&s.back()!='\n')s+='\n';return s;}
  void scanComments(const string& text,bool& block)const {
    char q=0;bool escape=false;
    for(size_t i=0;i<text.size();){
      if(block){
        if(!syntax.block_close.empty()&&text.compare(i,syntax.block_close.size(),syntax.block_close)==0){i+=syntax.block_close.size();block=false;}else ++i;
      }else if(q){char c=text[i++];if(escape)escape=false;else if(c=='\\')escape=true;else if(c==q)q=0;
      }else if(!syntax.line_comment.empty()&&text.compare(i,syntax.line_comment.size(),syntax.line_comment)==0)break;
      else if(!syntax.block_open.empty()&&text.compare(i,syntax.block_open.size(),syntax.block_open)==0){i+=syntax.block_open.size();block=true;}
      else {if(text[i]=='\''||text[i]=='"')q=text[i];++i;}
    }
  }
 bool matchExpression(const string& arg){
   auto pos=arg.find_first_of(" \t");if(pos==string::npos)fail("ifmatch syntax: ifmatch ENGINE PATTERN => SUBJECT");
   string matcher=trim(arg.substr(0,pos));auto pair=trim(arg.substr(pos));auto arrow=pair.find("=>");
   if(arrow==string::npos)fail("ifmatch requires '=>'");
   string pattern=unquote(trim(expand(pair.substr(0,arrow))));
   string subject=unquote(trim(expand(pair.substr(arrow+2))));
   return self->match(matcher,pattern,subject);
 }
  string processSource(string_view source,const string& sourcePath,size_t baseLine=1) {
    if(processDepth>=maxDepth)fail("processing nesting limit exceeded");
    ++processDepth;
    struct DepthRestore {size_t& depth;~DepthRestore(){--depth;}} depthRestore{processDepth};
   string savedPath=path;size_t savedLine=line;path=sourcePath;
   struct Restore {Impl& h;string p;size_t l;~Restore(){h.path=p;h.line=l;}} restore{*this,savedPath,savedLine};
   std::vector<string> lines=linesOf(source);std::vector<Frame> frames;string out;bool comment=false;
   auto active=[&](){return frames.empty()||frames.back().active;};
   auto splitDirective=[&](const string& raw)->std::pair<string,string>{if(comment)return {};string s=left(stripEol(raw));if(!starts(s,syntax.directive))return {};s=left(s.substr(syntax.directive.size()));if(s.empty()||!ident0(s[0]))return {};auto [k,tail]=takeName(s);return {k,trim(tail)};};
    auto getBlock=[&](size_t& index,const string& begin,const string& end)->string{
      std::vector<string> blocks{begin};string body;bool blockComment=false;
      for(size_t j=index+1;j<lines.size();++j){
        auto [name,args]=splitDirective(lines[j]);(void)args;
        const auto top=blocks.back();
        if(!blockComment||top=="raw"){
          if(name=="end"+top){blocks.pop_back();if(blocks.empty()){index=j;return body;}}
          else if(top=="raw"){if(name=="raw")blocks.push_back(name);}
          else if(top=="macro"){if(name=="macro"||name=="raw")blocks.push_back(name);}
          else if(name=="raw"||name=="macro"||name=="repeat"||name=="for")blocks.push_back(name);
        }
        if(top!="raw")scanComments(lines[j],blockComment);
        body+=lines[j];
      }
      fail("missing "+syntax.directive+end+" for "+syntax.directive+begin);
    };
    try {
    for(size_t i=0;i<lines.size();++i){line=baseLine+i;auto raw=lines[i];auto [cmd,arg]=splitDirective(raw);bool nl=!raw.empty()&&raw.back()=='\n';
     if(cmd=="if"||cmd=="ifdef"||cmd=="ifndef"||cmd=="ifmatch"){bool parent=active();bool decision=false;if(parent){if(cmd=="ifdef"||cmd=="ifndef"){auto k=trim(arg);if(k.empty())fail("expected identifier");decision=declared(k);if(cmd=="ifndef")decision=!decision;}else if(cmd=="ifmatch")decision=matchExpression(arg);else decision=eval(arg)!=0;}frames.push_back({parent,parent&&decision,parent&&decision,false});continue;}
     if(cmd=="elif"||cmd=="elifmatch"){if(frames.empty())fail("unexpected elif");auto& f=frames.back();if(f.elseSeen)fail("elif after else");bool yes=f.parent&&!f.taken&&(cmd=="elifmatch"?matchExpression(arg):eval(arg)!=0);f.active=yes;f.taken|=yes;continue;}
     if(cmd=="else"){if(frames.empty())fail("unexpected else");auto& f=frames.back();if(f.elseSeen)fail("duplicate else");f.active=f.parent&&!f.taken;f.taken=true;f.elseSeen=true;continue;}
     if(cmd=="endif"){if(frames.empty())fail("unexpected endif");frames.pop_back();continue;}
     if(cmd=="raw"||cmd=="macro"||cmd=="repeat"||cmd=="for"){
       size_t opening=i+1;string closing=cmd=="raw"?"endraw":cmd=="macro"?"endmacro":cmd=="repeat"?"endrepeat":"endfor";
       string body=getBlock(i,cmd,closing);
        if(!active())continue;
       if(cmd=="raw"){outputAppend(out,body);continue;}
       if(cmd=="macro"){addMacro(arg,body);continue;}
       if(cmd=="repeat"){long long n=eval(arg);if(n<0||n>100000)fail("repeat count must be within 0..100000");for(long long a=0;a<n;++a)out+=processSource(body,path,baseLine+opening);continue;}
       auto in=arg.find(" in ");if(in==string::npos)fail("for syntax is: for NAME in START..END or CSV");string var=trim(arg.substr(0,in)),range=trim(arg.substr(in+4));if(var.empty()||!ident0(var[0])||!std::all_of(var.begin(),var.end(),ident))fail("invalid for variable");std::vector<string> values;
       auto dots=range.find("..");if(dots!=string::npos){long long a=eval(range.substr(0,dots)),b=eval(range.substr(dots+2));if(std::fabs((long double)b-(long double)a)>100000)fail("for range too large");long long direction=a<=b?1:-1;for(long long j=a;;j+=direction){values.push_back(std::to_string(j));if(j==b)break;}}
       else values=splitArgs(range);
       auto old=macros.find(var);bool existed=old!=macros.end();Macro previous;if(existed)previous=old->second;
       try {for(auto& v:values){macros[var]=Macro{false,false,{},v};out+=processSource(body,path,baseLine+opening);}}
       catch(...){if(existed)macros[var]=previous;else macros.erase(var);throw;}
       if(existed)macros[var]=previous;else macros.erase(var);continue;
     }
      if(!active()){scanComments(raw,comment);continue;}
     if(!cmd.empty()){
       if(cmd=="define"){string a=left(arg);size_t j=0;while(j<a.size()&&ident(a[j]))++j;if(!j)fail("define requires a name");string desc=a.substr(0,j);if(j<a.size()&&a[j]=='('){size_t e=0;std::vector<string> unused;if(!parseCall(a,j,e,unused))fail("malformed macro definition");desc=a.substr(0,e);j=e;}addMacro(desc,left(a.substr(j)));continue;}
       if(cmd=="undef"){macros.erase(trim(arg));continue;}
       if(cmd=="include"||cmd=="include_once"||cmd=="import"){
         string name=trim(expand(arg));if(name.size()>1&&name[0]=='<'&&name.back()=='>')name=name.substr(1,name.size()-2);auto filename=resolve(name);
          if(imported.count(filename)||once.count(filename))continue;
         if(activeFiles.size()>=maxIncludes)fail("include nesting limit exceeded");
         if(std::find(activeFiles.begin(),activeFiles.end(),filename)!=activeFiles.end())fail("cyclic include: "+filename.string());
          bool importing=cmd!="include";
          if(importing)imported.insert(filename);
          activeFiles.push_back(filename);try{out+=processSource(self->read(filename),filename.string());}catch(...){activeFiles.pop_back();if(importing)imported.erase(filename);throw;}activeFiles.pop_back();continue;
       }
       if(cmd=="pragma"){if(arg=="once"){if(!path.empty()&&path[0]!='<'){std::error_code ec;once.insert(fs::weakly_canonical(path,ec));}continue;}warning("unknown pragma: "+arg);continue;}
       if(cmd=="syntax"){auto p=arg.find_first_of(" \t=");if(p==string::npos)fail("syntax requires a key and value");string key=arg.substr(0,p),val=trim(arg.substr(p));if(!val.empty()&&val[0]=='=')val=trim(val.substr(1));setOption(key,val);continue;}
       if(cmd=="plugin"){self->loadPlugin(resolve(expand(arg)));continue;}
       if(cmd=="error")fail(expand(arg));
       if(cmd=="warning"){warning(expand(arg));continue;}
       if(cmd=="assert"){if(!eval(arg))fail("assertion failed: "+arg);continue;}
       if(cmd=="emit"){outputAppend(out,directiveResult(rewrite(expand(arg)),nl));continue;}
       if(cmd=="eval"){outputAppend(out,std::to_string(eval(arg))+(nl?"\n":""));continue;}
       if(cmd=="embed"){
         string p=trim(arg),name,fmt="c_array";if(p.empty())fail("embed requires a filename");
         if(p[0]=='"'||p[0]=='\''){size_t e=p.find(p[0],1);while(e!=string::npos&&p[e-1]=='\\')e=p.find(p[0],e+1);if(e==string::npos)fail("unclosed embed path");name=p.substr(0,e+1);fmt=trim(p.substr(e+1));}else{auto z=p.find_first_of(" \t");name=p.substr(0,z);if(z!=string::npos)fmt=trim(p.substr(z));}if(fmt.empty())fmt="c_array";
         auto bytes=self->read(resolve(expand(name)));string data;auto extension=embeds.find(fmt);if(extension!=embeds.end())data=extension->second(bytes);else if(fmt=="hex")data=hexBytes(bytes,false);else if(fmt=="c_array")data=hexBytes(bytes,true);else if(fmt=="base64")data=base64(bytes);else if(fmt=="string")data=quote(bytes);else fail("unknown embed format: "+fmt);
         outputAppend(out,directiveResult(data,nl));continue;
       }
       if(cmd=="rewrite"){auto sep=arg.find("=>");if(sep==string::npos)fail("rewrite syntax: rewrite PATTERN => REPLACEMENT");try{regexRules.push_back({std::regex(unquote(trim(arg.substr(0,sep)))),unquote(trim(arg.substr(sep+2)))});}catch(const std::regex_error& e){fail("invalid rewrite regex: "+string(e.what()));}continue;}
       if(cmd=="clear_rewrites"){regexRules.clear();continue;}
       if(cmd=="push_macro"){auto k=trim(arg);auto f=macros.find(k);macroStacks[k].push_back(f==macros.end()?std::make_pair(false,Macro{}):std::make_pair(true,f->second));continue;}
       if(cmd=="pop_macro"){auto k=trim(arg);if(macroStacks[k].empty())fail("pop_macro without push_macro: "+k);auto old=macroStacks[k].back();macroStacks[k].pop_back();if(old.first)macros[k]=old.second;else macros.erase(k);continue;}
       if(cmd=="line"){ /* Line markers can override __LINE__ for the remaining source. */long long n=eval(arg);if(n<1)fail("line number must be positive");baseLine=(size_t)n-i-1;continue;}
       if(cmd=="region"||cmd=="endregion")continue;
       if(cmd=="sink"){auto p=arg.find_first_of(" \t");if(p==string::npos)fail("sink requires NAME and CONTENT");self->writeSink(arg.substr(0,p),expand(trim(arg.substr(p))));continue;}
       auto ext=directives.find(cmd);if(ext!=directives.end()){outputAppend(out,directiveResult(ext->second(arg),nl));continue;}
       if(strict)fail("unknown directive: "+cmd);
       outputAppend(out,raw);continue;
     }
     outputAppend(out,rewrite(expand(raw,0,{},&comment)));
   }
    if(!frames.empty())fail("unterminated conditional block");
    }catch(const std::exception& e){if(lastErr==e.what())throw;fail(e.what());}
    return out;
 }
};
Engine::Engine():impl_(std::make_unique<Impl>(this)){}
Engine::~Engine()=default;
Engine::Engine(Engine&& other)noexcept:impl_(std::move(other.impl_)){if(impl_){impl_->self=this;impl_->state.engine=this;}}
Engine& Engine::operator=(Engine&& other)noexcept{if(this!=&other){impl_=std::move(other.impl_);if(impl_){impl_->self=this;impl_->state.engine=this;}}return *this;}
string Engine::process(std::string_view input,string virtualPath){if(!impl_->processDepth){impl_->logs.clear();impl_->lastErr.clear();impl_->bytesProduced=0;}try{return impl_->processSource(input,virtualPath);}catch(const Error&){throw;}catch(const std::exception& e){impl_->fail(e.what());}}
string Engine::processFile(const fs::path& filename){if(!impl_->processDepth){impl_->logs.clear();impl_->lastErr.clear();impl_->bytesProduced=0;}std::error_code ec;auto absolute=fs::weakly_canonical(filename,ec);if(ec)absolute=fs::absolute(filename);if(impl_->activeFiles.size()>=impl_->maxIncludes)impl_->fail("include nesting limit exceeded");impl_->activeFiles.push_back(absolute);try{auto content=read(absolute);auto out=impl_->processSource(content,absolute.string());impl_->activeFiles.pop_back();return out;}catch(...){impl_->activeFiles.pop_back();throw;}}
void Engine::define(string name,string replacement){impl_->addMacro(std::move(name),std::move(replacement));}
string Engine::call(const string& name,const std::vector<string>& arguments){
  auto replacement=impl_->invoke(name,arguments,0,{});
  auto result=impl_->expand(replacement,1,{name});
  if(result.size()>impl_->maxOutput)impl_->fail("output limit exceeded");
  return result;
}
void Engine::undef(const string& name){impl_->macros.erase(name);}
void Engine::includePath(fs::path p){impl_->includeDirs.push_back(std::move(p));}
void Engine::set(string key,string value){impl_->setOption(key,value);}
void Engine::registerDirective(string name,std::function<string(std::string_view)> f){impl_->directives[std::move(name)]=std::move(f);}
void Engine::registerMacro(string name,std::function<string(const std::vector<string>&)> f){impl_->callables[std::move(name)]=std::move(f);}
void Engine::registerRewriter(std::function<string(std::string_view)> f){impl_->rewriters.push_back(std::move(f));}
void Engine::registerReader(string extension,std::function<string(const fs::path&)> f){impl_->readers[lower(std::move(extension))]=std::move(f);}
void Engine::registerSink(string name,std::function<void(std::string_view)> f){impl_->sinks[std::move(name)]=std::move(f);}
void Engine::registerEmbed(string name,std::function<string(std::string_view)> f){impl_->embeds[std::move(name)]=std::move(f);}
void Engine::registerSyntax(string name,std::function<void(std::string_view)> f){impl_->syntaxes[lower(std::move(name))]=std::move(f);}
void Engine::registerPMatch(string name,std::function<bool(std::string_view,std::string_view)> f){impl_->pmatches[std::move(name)]=std::move(f);}
std::vector<Diagnostic> Engine::diagnostics()const{return impl_->logs;}
void Engine::diagnostic(string message){impl_->warning(std::move(message));}
string Engine::lastError()const{return impl_->lastErr;}
bool Engine::match(const string& engine,std::string_view pattern,std::string_view subject){auto x=impl_->pmatches.find(engine);if(x!=impl_->pmatches.end())return x->second(pattern,subject);if(engine=="regex"){try{return std::regex_search(string(subject),std::regex(string(pattern)));}catch(const std::regex_error& e){throw Error(e.what());}}throw Error("unknown pattern matcher: "+engine);}
void Engine::writeSink(const string& name,std::string_view content){auto it=impl_->sinks.find(name);if(it!=impl_->sinks.end()){it->second(content);return;}if(name=="stdout"){std::fwrite(content.data(),1,content.size(),stdout);return;}if(name=="stderr"){std::fwrite(content.data(),1,content.size(),stderr);return;}if(name=="discard")return;throw Error("unknown sink: "+name);}
string Engine::read(const fs::path& filename){auto it=impl_->readers.find(basenameExtension(filename));if(it!=impl_->readers.end())return it->second(filename);return binaryRead(filename);}
GPPX_State* Engine::state()noexcept{return &impl_->state;}
}
namespace {
int catchC(GPPX_State* state,const std::function<void()>& body){if(!state||!state->engine)return 1;try{body();state->error.clear();return 0;}catch(const std::exception& e){state->error=e.what();return 1;}catch(...){state->error="unknown error";return 1;}}
string requiredReplacement(GPPX_State* s,int rc,char* value,const string& category){std::unique_ptr<char,decltype(&std::free)> result(value,&std::free);if(rc!=0)throw gppx::Error(category+" callback failed"+(s->error.empty()?string():": "+s->error));return value?string(value):string();}
GPPX_HostAPI pluginHost();
}
extern "C" {
GPPX_State* gppx_create(void){try{auto *e=new gppx::Engine();auto *s=e->state();s->owned=true;return s;}catch(...){return nullptr;}}
void gppx_destroy(GPPX_State* s){if(s&&s->owned)delete s->engine;}
int gppx_process(GPPX_State* s,const char* in,const char* file,char** output){if(output)*output=nullptr;return catchC(s,[&]{if(!in||!output)throw gppx::Error("null input or output");string result=s->engine->process(in,file?file:"<stdin>");*output=gppx_strdup(result.c_str());if(!*output)throw std::bad_alloc();});}
int gppx_process_file(GPPX_State* s,const char* file,char** output){if(output)*output=nullptr;return catchC(s,[&]{if(!file||!output)throw gppx::Error("null filename or output");auto result=s->engine->processFile(file);*output=gppx_strdup(result.c_str());if(!*output)throw std::bad_alloc();});}
int gppx_define(GPPX_State* s,const char* name,const char* value){return catchC(s,[&]{if(!name||!value)throw gppx::Error("null macro name/value");s->engine->define(name,value);});}
int gppx_function_call(GPPX_State* s,const char* name,int argc,const char* const* argv,char** output){
  if(output)*output=nullptr;
  return catchC(s,[&]{
    if(!name||!output||argc<0||(argc&&!argv))throw gppx::Error("invalid function arguments");
    std::vector<string> args;
    for(int i=0;i<argc;++i){if(!argv[i])throw gppx::Error("null function argument");args.emplace_back(argv[i]);}
    auto result=s->engine->call(name,args);
    *output=gppx_strdup(result.c_str());if(!*output)throw std::bad_alloc();
  });
}
int gppx_undef(GPPX_State* s,const char* name){return catchC(s,[&]{if(!name)throw gppx::Error("null macro name");s->engine->undef(name);});}
int gppx_add_include_path(GPPX_State* s,const char* p){return catchC(s,[&]{if(!p)throw gppx::Error("null include path");s->engine->includePath(p);});}
int gppx_set_option(GPPX_State* s,const char* k,const char* v){return catchC(s,[&]{if(!k||!v)throw gppx::Error("null syntax option");s->engine->set(k,v);});}
int gppx_register_directive(GPPX_State* s,const char* k,GPPX_DirectiveFn cb,void* data){return catchC(s,[&]{if(!k||!cb)throw gppx::Error("invalid directive callback");s->engine->registerDirective(k,[s,cb,data](std::string_view a){char* out=nullptr;string arg(a);int r=cb(s,arg.c_str(),&out,data);return requiredReplacement(s,r,out,"directive");});});}
int gppx_register_macro(GPPX_State* s,const char* k,GPPX_MacroFn cb,void* data){return catchC(s,[&]{if(!k||!cb)throw gppx::Error("invalid macro callback");s->engine->registerMacro(k,[s,cb,data](const std::vector<string>& args){std::vector<const char*> ptrs;for(auto& x:args)ptrs.push_back(x.c_str());char* out=nullptr;int r=cb(s,(int)ptrs.size(),ptrs.data(),&out,data);return requiredReplacement(s,r,out,"macro");});});}
int gppx_register_rewriter(GPPX_State* s,GPPX_RewriteFn cb,void* data){return catchC(s,[&]{if(!cb)throw gppx::Error("invalid rewriter callback");s->engine->registerRewriter([s,cb,data](std::string_view a){char* out=nullptr;string arg(a);int r=cb(s,arg.c_str(),&out,data);return requiredReplacement(s,r,out,"rewriter");});});}
int gppx_register_reader(GPPX_State* s,const char* ext,GPPX_ReaderFn cb,void* data){return catchC(s,[&]{if(!ext||!cb)throw gppx::Error("invalid reader callback");s->engine->registerReader(ext,[s,cb,data](const fs::path& p){char* out=nullptr;auto filename=p.string();int r=cb(s,filename.c_str(),&out,data);return requiredReplacement(s,r,out,"reader");});});}
int gppx_register_sink(GPPX_State* s,const char* name,GPPX_SinkFn cb,void* data){return catchC(s,[&]{if(!name||!cb)throw gppx::Error("invalid sink callback");string nameCopy=name;s->engine->registerSink(nameCopy,[s,cb,data,nameCopy](std::string_view v){if(cb(s,nameCopy.c_str(),v.data(),v.size(),data))throw gppx::Error("sink callback failed");});});}
int gppx_register_embed(GPPX_State* s,const char* name,GPPX_EmbedFn cb,void* data){return catchC(s,[&]{if(!name||!cb)throw gppx::Error("invalid embed callback");s->engine->registerEmbed(name,[s,cb,data](std::string_view bytes){char* out=nullptr;int r=cb(s,(const unsigned char*)bytes.data(),bytes.size(),&out,data);return requiredReplacement(s,r,out,"embed");});});}
int gppx_register_syntax(GPPX_State* s,const char* name,GPPX_SyntaxFn cb,void* data){return catchC(s,[&]{if(!name||!cb)throw gppx::Error("invalid syntax callback");s->engine->registerSyntax(name,[s,cb,data](std::string_view str){auto a=string(str);if(cb(s,a.c_str(),data))throw gppx::Error("syntax callback failed");});});}
int gppx_register_pmatch(GPPX_State* s,const char* name,GPPX_PMatchFn cb,void* data){return catchC(s,[&]{if(!name||!cb)throw gppx::Error("invalid pattern matcher");s->engine->registerPMatch(name,[s,cb,data](std::string_view p,std::string_view sub){auto a=string(p),b=string(sub);int rc=cb(s,a.c_str(),b.c_str(),data);if(rc<0)throw gppx::Error("pattern matcher callback failed");return rc>0;});});}
const char* gppx_last_error(const GPPX_State* s){return s?s->error.c_str():"invalid GPPX_State";}
const char* gppx_diagnostics(const GPPX_State* s){if(!s)return "invalid GPPX_State";auto* mutableState=const_cast<GPPX_State*>(s);std::ostringstream o;for(auto& d:s->engine->diagnostics())o<<d.path<<':'<<d.line<<": "<<(d.warning?"warning: ":"error: ")<<d.message<<'\n';mutableState->report=o.str();return mutableState->report.c_str();}
void gppx_free(void* p){std::free(p);}
char* gppx_strdup(const char* s){if(!s)return nullptr;size_t n=strlen(s)+1;char* p=(char*)std::malloc(n);if(p)memcpy(p,s,n);return p;}
}
namespace {
void pluginWarning(GPPX_State* s,const char* m){(void)catchC(s,[&]{s->engine->diagnostic(m?m:"(null)");});}
GPPX_HostAPI pluginHost(){GPPX_HostAPI host{};host.abi_version=GPPX_ABI_VERSION;host.register_directive=gppx_register_directive;host.register_macro=gppx_register_macro;host.register_rewriter=gppx_register_rewriter;host.register_reader=gppx_register_reader;host.register_sink=gppx_register_sink;host.register_embed=gppx_register_embed;host.register_syntax=gppx_register_syntax;host.register_pmatch=gppx_register_pmatch;host.define=gppx_define;host.set_option=gppx_set_option;host.diagnostic=pluginWarning;return host;}
}
namespace gppx {
void Engine::loadPlugin(const fs::path& filename){
#if defined(_WIN32)
  auto raw=LoadLibraryA(filename.string().c_str());if(!raw)throw Error("failed to load plugin: "+filename.string());
  auto init=(GPPX_PluginInit)GetProcAddress(raw,"gppx_plugin_init");if(!init){FreeLibrary(raw);throw Error("missing gppx_plugin_init in "+filename.string());}
#else
  void* raw=dlopen(filename.string().c_str(),RTLD_NOW|RTLD_LOCAL);if(!raw)throw Error("failed to load plugin: "+filename.string()+": "+dlerror());
  auto init=reinterpret_cast<GPPX_PluginInit>(dlsym(raw,"gppx_plugin_init"));if(!init){dlclose(raw);throw Error("missing gppx_plugin_init in "+filename.string());}
#endif
  /* Keep even a partially initialized plugin loaded until all callbacks are destroyed. */
  impl_->libraries.handles.push_back((void*)raw);
  auto host=pluginHost();int rc=0;
  try{rc=init(&host,state());}catch(...){rc=1;}
  if(rc!=0)throw Error("plugin initialization failed: "+filename.string());
}
}
