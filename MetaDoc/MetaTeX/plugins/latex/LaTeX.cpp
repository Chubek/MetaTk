#include "MetaTeX.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <stdexcept>
#include <string>
using namespace metatex;
namespace {
std::string trim(std::string s){while(!s.empty()&&std::isspace((unsigned char)s.front()))s.erase(s.begin());while(!s.empty()&&std::isspace((unsigned char)s.back()))s.pop_back();return s;}
int integer(const std::string& text){
    std::string_view value=text;if(!value.empty()&&value.front()=='+')value.remove_prefix(1);
    int result=0;auto parsed=std::from_chars(value.data(),value.data()+value.size(),result);
    if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size())throw std::runtime_error("invalid integer");
    return result;
}
void newCommand(Engine& e,int mode){
    e.readStar();
    auto nameTokens=e.readArgument();
    std::string name;
    if(nameTokens.size()==1 && nameTokens[0].kind==TokenKind::Control)name=nameTokens[0].controlName();
    if(name.empty()){e.report("expected control sequence in \\newcommand",true);return;}
    std::string arity=e.readOptionalText("0");int n=0;
    try{n=integer(arity);}catch(...){e.report("invalid newcommand argument count",true);return;}
    if(n<0||n>9){e.report("newcommand supports at most nine arguments",true);return;}
    auto optionalDefault=e.readOptionalArgument();
    Macro m{n,e.readArgument()};
    if(optionalDefault&&n==0){e.report("optional macro default requires at least one parameter",true);return;}
    m.optionalFirst=std::move(optionalDefault);
    bool found=e.hasCommand(name);
    if(mode==0 && found){e.report("\\newcommand: already defined: "+name,true);return;}
    if(mode==1 && !found){e.report("\\renewcommand: not defined: "+name,true);return;}
    if(mode==2 && found)return;
    for(std::size_t i=0;i<m.replacement.size();++i){
        if(m.replacement[i].kind!=TokenKind::Parameter)continue;
        if(++i<m.replacement.size()){
            const auto& t=m.replacement[i];
            if(t.kind==TokenKind::Parameter)continue;
            if(t.kind==TokenKind::Character&&t.value.size()==1&&t.value[0]>='1'&&t.value[0]<='0'+n)continue;
        }
        e.report("invalid parameter reference in command definition",true);return;
    }
    e.defineMacro(name,std::move(m));
}
void package(Engine& x){
    x.readOptionalArgument();auto names=x.readTextArgument();std::size_t at=0;
    while(at<names.size()){
        auto end=names.find(',',at);if(end==std::string::npos)end=names.size();
        auto name=trim(names.substr(at,end-at));
        if(name=="metavect"&&x.hasCommand("metavect")){}
        else if(name=="amsmath"||name=="amssymb"||name=="graphicx"||name=="hyperref"||name=="geometry"||name=="xcolor"||name=="fontenc"||name=="inputenc"||name=="babel"||name=="enumitem"||name=="mathtools")
            x.report("package "+name+": using native subset; external package/options are not loaded");
        else if(!name.empty())x.report("package "+name+" is not installed/implemented");
        at=end+1;
    }
}
bool validReplacement(Engine& e,const Macro& m){
    for(std::size_t i=0;i<m.replacement.size();++i){
        if(m.replacement[i].kind!=TokenKind::Parameter)continue;
        if(++i<m.replacement.size()){
            const auto& t=m.replacement[i];
            if(t.kind==TokenKind::Parameter)continue;
            if(t.kind==TokenKind::Character&&t.value.size()==1&&t.value[0]>='1'&&t.value[0]<='0'+m.parameters)continue;
        }
        e.report("invalid parameter reference in environment definition",true);return false;
    }return true;
}
void environment(Engine& e,bool renew){
    e.readStar();auto name=e.readTextArgument();auto arity=e.readOptionalText("0");
    auto optional=e.readOptionalArgument();auto begin=e.readArgument(),end=e.readArgument();
    int n=0;try{n=integer(arity);}catch(...){e.report("invalid environment argument count",true);return;}
    if(n<0||n>9||(optional&&n==0)){e.report("invalid environment parameter count/default",true);return;}
    if(name.empty()||e.hasEnvironment(name)!=renew){e.report(renew?"environment not defined: "+name:"environment already defined or empty: "+name,true);return;}
    Macro body{n,std::move(begin)};body.optionalFirst=std::move(optional);
    if(!validReplacement(e,body)||!validReplacement(e,Macro{0,end}))return;
    e.defineTokenEnvironment(name,std::move(body),std::move(end));
}
void theorem(Engine& e){
    bool starred=e.readStar();auto name=e.readTextArgument();auto shared=e.readOptionalText();
    auto title=e.readTextArgument();auto parent=e.readOptionalText();
    if(name.empty()||e.hasEnvironment(name)){e.report("theorem environment already defined or empty: "+name,true);return;}
    if((starred&&(!shared.empty()||!parent.empty()))||(!shared.empty()&&!parent.empty())){e.report("incompatible theorem counter options",true);return;}
    auto counter=shared.empty()?name:shared;
    if(!starred){
        if(!shared.empty()&&!e.hasCounter(shared)){e.report("unknown shared theorem counter: "+shared,true);return;}
        if(shared.empty()){
            if(e.hasCounter(name)||(!parent.empty()&&!e.hasCounter(parent))){e.report("invalid theorem counter: "+name,true);return;}
            e.defineCounter(name,parent);
        }
    }
    e.defineEnvironment(name,[title,counter,starred](Engine& x){
        auto subtitle=x.readOptionalText();std::string heading=title;
        if(!starred){x.stepCounter(counter);auto value=x.formattedCounter(counter);x.setReferenceValue(value);heading+=" "+value;}
        if(!subtitle.empty())heading+=" ("+subtitle+")";
        x.beginBlockContext(BlockKind::Quote,"theorem");x.addSpan(SpanKind::Bold,heading+". ");
    },[](Engine& x){x.endBlockContext();});
}
std::string roman(int n){
    if(n<=0||n>3999)throw std::runtime_error("roman counter must be in 1..3999");
    std::string out;for(auto [value,text]:{std::pair{1000,"m"},{900,"cm"},{500,"d"},{400,"cd"},{100,"c"},{90,"xc"},{50,"l"},{40,"xl"},{10,"x"},{9,"ix"},{5,"v"},{4,"iv"},{1,"i"}})
        while(n>=value){out+=text;n-=value;}
    return out;
}
void caption(Engine& e,const std::string& counter,const std::string& title){
    bool numbered=!e.readStar();e.readOptionalArgument();auto body=e.readArgument();
    if(numbered){e.stepCounter(counter);auto value=e.formattedCounter(counter);e.setReferenceValue(value);e.addText(title+" "+value+": ");}
    else e.addText(title+": ");
    e.executeGrouped(body);
}
}
extern "C" const char* metatex_plugin_id(){return "latex";}
extern "C" void metatex_register_plugin(Engine* engine){
    Engine& e=*engine;
    e.registerCommand("documentclass",[](Engine& x){x.readOptionalArgument();auto name=x.readTextArgument();
        if(name!="article"&&name!="report"&&name!="book"&&name!="standalone"&&name!="minimal")x.report("document class "+name+" uses article layout fallback");});
    e.registerCommand("usepackage",package);
    e.registerCommand("RequirePackage",package);
    e.registerCommand("newcommand",[](Engine& x){newCommand(x,0);});
    e.registerCommand("newenvironment",[](Engine& x){environment(x,false);});
    e.registerCommand("renewenvironment",[](Engine& x){environment(x,true);});
    e.registerCommand("newtheorem",theorem);
    e.registerCommand("renewcommand",[](Engine& x){newCommand(x,1);});
    e.registerCommand("providecommand",[](Engine& x){newCommand(x,2);});
    e.registerCommand("begin",[](Engine& x){x.beginEnvironment(x.readTextArgument());});
    e.registerCommand("end",[](Engine& x){x.endEnvironment(x.readTextArgument());});
    for(auto [name,level]:{std::pair{"section",1},{"subsection",2},{"subsubsection",3},{"chapter",1},{"part",1},{"paragraph",4},{"subparagraph",5}})
        e.registerCommand(name,[level](Engine& x){bool numbered=!x.readStar();x.readOptionalArgument();x.heading(level,numbered);});
    e.registerCommand("title",[](Engine& x){x.setTitle(x.readTextArgument());});
    e.registerCommand("author",[](Engine& x){x.setAuthor(x.readTextArgument());});
    e.registerCommand("date",[](Engine& x){x.setDate(x.readTextArgument());});
    e.registerCommand("maketitle",[](Engine& x){x.makeTitle();});
    e.registerCommand("tableofcontents",[](Engine& x){x.startBlock(BlockKind::TableOfContents);x.paragraph();});
    e.registerCommand("item",[](Engine& x){x.item();});
    e.registerCommand("label",[](Engine& x){x.label(x.readTextArgument());});
    e.registerCommand("ref",[](Engine& x){x.reference(x.readTextArgument());});
    e.registerCommand("eqref",[](Engine& x){x.addText("(");x.reference(x.readTextArgument());x.addText(")");});
    e.registerCommand("pageref",[](Engine& x){x.reference(x.readTextArgument());});
    e.registerCommand("setcounter",[](Engine& x){auto n=x.readTextArgument();try{x.setCounter(n,integer(x.readTextArgument()));}catch(...){x.report("invalid counter value",true);}});
    e.registerCommand("addtocounter",[](Engine& x){auto n=x.readTextArgument();try{x.addToCounter(n,integer(x.readTextArgument()));}catch(...){x.report("invalid counter value",true);}});
    e.registerCommand("stepcounter",[](Engine& x){x.stepCounter(x.readTextArgument());});
    e.registerCommand("newcounter",[](Engine& x){auto name=x.readTextArgument();auto parent=x.readOptionalText();
        bool exists=x.hasCounter(name);x.defineCounter(name,parent);
        if(!exists&&x.hasCounter(name))x.defineCommand("the"+name,[name](Engine& a){a.addText(a.formattedCounter(name));});
    });
    e.registerCommand("refstepcounter",[](Engine& x){auto name=x.readTextArgument();x.stepCounter(name);x.setReferenceValue(x.formattedCounter(name));});
    e.registerCommand("arabic",[](Engine& x){x.addText(std::to_string(x.counter(x.readTextArgument())));});
    for(std::string format:{"roman","Roman","alph","Alph"})e.registerCommand(format,[format](Engine& x){
        auto n=x.counter(x.readTextArgument());try{
            std::string text;
            if(format=="roman"||format=="Roman")text=roman(n);
            else {if(n<1||n>26)throw std::runtime_error("alphabetic counter must be in 1..26");text=char('a'+n-1);}
            if(format=="Roman"||format=="Alph")for(auto& c:text)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            x.addText(text);
        }catch(const std::exception& ex){x.report(ex.what(),true);}
    });
    e.registerCommand("textnormal",[](Engine& x){x.styledArgument(SpanKind::Text);});
    e.registerCommand("textsf",[](Engine& x){x.styledArgument(SpanKind::Text);});
    e.registerCommand("verb",[](Engine& x){x.addSpan(SpanKind::Code,x.readDelimitedText());});
    e.registerCommand("mbox",[](Engine& x){x.execute(x.readArgument());});
    e.registerCommand("text",[](Engine& x){x.execute(x.readArgument());});
    e.registerCommand("textrm",[](Engine& x){x.execute(x.readArgument());});
    e.registerCommand("textcolor",[](Engine& x){x.readTextArgument();x.report("textcolor: native output does not apply text colors");x.executeGrouped(x.readArgument());});
    e.registerCommand("centering",[](Engine&){});
    e.registerCommand("cite",[](Engine& x){x.addText("["+x.readTextArgument()+"]");});
    e.registerCommand("url",[](Engine& x){auto url=x.readTextArgument();x.addSpan(SpanKind::Link,url,url);});
    e.registerCommand("href",[](Engine& x){auto url=x.readTextArgument();x.addSpan(SpanKind::Link,x.readTextArgument(),url);});
    e.registerCommand("includegraphics",[](Engine& x){x.readOptionalArgument();auto image=x.readTextArgument();x.addSpan(SpanKind::Image,"Figure",image);});
    e.registerCommand("footnote",[](Engine& x){x.addText(" [note: "+x.readTextArgument()+"]");});
    e.registerCommand("caption",[](Engine& x){caption(x,"figure","Figure");});
    e.registerCommand("newpage",[](Engine& x){x.startBlock(BlockKind::PageBreak);x.paragraph();});
    e.registerCommand("clearpage",[](Engine& x){x.startBlock(BlockKind::PageBreak);x.paragraph();});
    e.registerCommand("medskip",[](Engine& x){x.paragraph();});
    e.registerCommand("bigskip",[](Engine& x){x.paragraph();});
    e.registerCommand("smallskip",[](Engine& x){x.paragraph();});
    e.registerCommand("hrule",[](Engine& x){x.startBlock(BlockKind::HorizontalRule);x.paragraph();});
    e.registerCommand("hrulefill",[](Engine& x){x.addText("________________");});
    e.registerEnvironment("document",[](Engine&){});
    e.registerEnvironment("itemize",[](Engine& x){x.paragraph();},[](Engine& x){x.paragraph();});
    e.registerEnvironment("enumerate",[](Engine& x){x.paragraph();},[](Engine& x){x.paragraph();});
    e.registerEnvironment("description",[](Engine& x){x.paragraph();},[](Engine& x){x.paragraph();});
    e.registerEnvironment("center",[](Engine& x){x.paragraph();},[](Engine& x){x.paragraph();});
    for(std::string name:{"quote","quotation","abstract"})e.registerEnvironment(name,[](Engine& x){x.beginBlockContext(BlockKind::Quote);},[](Engine& x){x.endBlockContext();});
    for(std::string name:{"figure","table"})e.registerEnvironment(name,[name](Engine& x){
        x.readOptionalArgument();x.paragraph();x.defineCommand("caption",[name](Engine& a){caption(a,name,name=="table"?"Table":"Figure");});
    },[](Engine& x){x.paragraph();});
    for(std::string name:{"equation","equation*","align","align*","gather","gather*","displaymath"})
        e.registerEnvironment(name,[name](Engine& x){x.mathEnvironment(name,name=="equation"||name=="align"||name=="gather");});
    e.registerEnvironment("verbatim",[](Engine& x){auto text=x.readLiteralEnvironment("verbatim");x.codeBlock(std::move(text));x.endEnvironment("verbatim");});
    e.registerEnvironment("proof",[](Engine& x){auto title=x.readOptionalText("Proof");x.beginBlockContext(BlockKind::Quote,"proof");x.addSpan(SpanKind::Italic,title+". ");},[](Engine& x){x.addText(" QED");x.endBlockContext();});
}
