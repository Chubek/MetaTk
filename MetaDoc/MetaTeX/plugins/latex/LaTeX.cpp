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
    auto nameTokens=e.readArgument();
    std::string name;
    if(nameTokens.size()==1 && nameTokens[0].kind==TokenKind::Control)name=nameTokens[0].controlName();
    else {std::string raw=e.tokensText(nameTokens);if(raw.size()>1&&raw[0]=='\\')name=raw.substr(1);}
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
    e.defineMacro(name,std::move(m));
}
}
extern "C" const char* metatex_plugin_id(){return "latex";}
extern "C" void metatex_register_plugin(Engine* engine){
    Engine& e=*engine;
    e.registerCommand("documentclass",[](Engine& x){x.readOptionalArgument();auto name=x.readTextArgument();
        if(name!="article"&&name!="report"&&name!="book"&&name!="standalone"&&name!="minimal")x.report("document class "+name+" uses article layout fallback");});
    e.registerCommand("usepackage",[](Engine& x){x.readOptionalArgument();auto names=x.readTextArgument();
        std::size_t at=0;
        while(at<names.size()){auto end=names.find(',',at);if(end==std::string::npos)end=names.size();
            auto name=trim(names.substr(at,end-at));
            if(name=="metavect"&&x.hasCommand("metavect")){at=end+1;continue;}
            if(name!="amsmath"&&name!="amssymb"&&name!="graphicx"&&name!="hyperref"&&name!="geometry"&&name!="xcolor"&&name!="fontenc"&&name!="inputenc"&&name!="babel"&&name!="enumitem"&&name!="mathtools"&&name!="")
                x.report("package "+name+" is not installed/implemented");
            at=end+1;
        }});
    e.registerCommand("RequirePackage",[](Engine& x){x.readOptionalArgument();x.readTextArgument();});
    e.registerCommand("newcommand",[](Engine& x){newCommand(x,0);});
    e.registerCommand("newenvironment",[](Engine& x){
        auto name=x.readTextArgument();
        auto n=x.readOptionalText("0");
        if(n!="0")x.report("newenvironment arguments not yet supported",true);
        auto begin=x.readArgument(),end=x.readArgument();
        x.registerEnvironment(name,[begin](Engine& a){a.execute(begin);},[end](Engine& a){a.execute(end);});
    });
    e.registerCommand("newtheorem",[](Engine& x){
        auto name=x.readTextArgument();auto label=x.readTextArgument();
        x.registerEnvironment(name,[name,label](Engine& a){
            a.stepCounter(name);a.startBlock(BlockKind::Heading,3);
            a.addText(label+" "+std::to_string(a.counter(name)));a.paragraph();
        },[](Engine& a){a.paragraph();});
    });
    e.registerCommand("renewcommand",[](Engine& x){newCommand(x,1);});
    e.registerCommand("providecommand",[](Engine& x){newCommand(x,2);});
    e.registerCommand("begin",[](Engine& x){x.beginEnvironment(x.readTextArgument());});
    e.registerCommand("end",[](Engine& x){x.endEnvironment(x.readTextArgument());});
    e.registerCommand("section",[](Engine& x){x.readOptionalArgument();x.heading(1);});
    e.registerCommand("subsection",[](Engine& x){x.readOptionalArgument();x.heading(2);});
    e.registerCommand("subsubsection",[](Engine& x){x.readOptionalArgument();x.heading(3);});
    e.registerCommand("chapter",[](Engine& x){x.readOptionalArgument();x.heading(1);});
    e.registerCommand("part",[](Engine& x){x.readOptionalArgument();x.heading(1);});
    e.registerCommand("paragraph",[](Engine& x){x.heading(4);});
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
    e.registerCommand("arabic",[](Engine& x){x.addText(std::to_string(x.counter(x.readTextArgument())));});
    e.registerCommand("textnormal",[](Engine& x){x.styledArgument(SpanKind::Text);});
    e.registerCommand("textsf",[](Engine& x){x.styledArgument(SpanKind::Text);});
    e.registerCommand("verb",[](Engine& x){x.addSpan(SpanKind::Code,x.readDelimitedText());});
    e.registerCommand("mbox",[](Engine& x){x.execute(x.readArgument());});
    e.registerCommand("text",[](Engine& x){x.execute(x.readArgument());});
    e.registerCommand("textrm",[](Engine& x){x.execute(x.readArgument());});
    e.registerCommand("textcolor",[](Engine& x){x.readTextArgument();x.execute(x.readArgument());});
    e.registerCommand("centering",[](Engine&){});
    e.registerCommand("cite",[](Engine& x){x.addText("["+x.readTextArgument()+"]");});
    e.registerCommand("url",[](Engine& x){auto url=x.readTextArgument();x.addSpan(SpanKind::Link,url,url);});
    e.registerCommand("href",[](Engine& x){auto url=x.readTextArgument();x.addSpan(SpanKind::Link,x.readTextArgument(),url);});
    e.registerCommand("includegraphics",[](Engine& x){x.readOptionalArgument();auto image=x.readTextArgument();x.addSpan(SpanKind::Image,"Figure",image);});
    e.registerCommand("footnote",[](Engine& x){x.addText(" [note: "+x.readTextArgument()+"]");});
    e.registerCommand("caption",[](Engine& x){x.addText("Figure: "+x.readTextArgument());});
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
    e.registerEnvironment("center",[](Engine& x){x.paragraph();},[](Engine& x){x.paragraph();});
    e.registerEnvironment("quote",[](Engine& x){x.startBlock(BlockKind::Quote);},[](Engine& x){x.paragraph();});
    e.registerEnvironment("abstract",[](Engine& x){x.startBlock(BlockKind::Quote);},[](Engine& x){x.paragraph();});
    e.registerEnvironment("figure",[](Engine& x){x.paragraph();},[](Engine& x){x.paragraph();});
    e.registerEnvironment("table",[](Engine& x){x.paragraph();},[](Engine& x){x.paragraph();});
    e.registerEnvironment("equation",[](Engine& x){x.math(true,"end");x.readTextArgument();x.endEnvironment("equation");});
    e.registerEnvironment("equation*",[](Engine& x){x.math(true,"end");x.readTextArgument();x.endEnvironment("equation*");});
    e.registerEnvironment("align",[](Engine& x){x.math(true,"end");x.readTextArgument();x.endEnvironment("align");});
    e.registerEnvironment("align*",[](Engine& x){x.math(true,"end");x.readTextArgument();x.endEnvironment("align*");});
}
