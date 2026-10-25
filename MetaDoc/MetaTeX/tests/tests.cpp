#include "MetaTeX.h"
#include <cassert>
#include <iostream>
using namespace metatex;
int main(){
    auto t=Lexer::scan("alpha %comment\n\\textbf{X}$x^2$\n\nBeta");
    assert(!t.empty());bool comment=false;
    for(const auto& x:t)if(x.value=="comment")comment=true;
    assert(!comment);
    Engine e;
    auto doc=e.compile("\\def\\hello#1{Hello, #1!}\\hello{world}\\par $x^2+\\alpha$ \\iftrue yes\\else no\\fi");
    auto txt=renderText(doc),html=renderHTML(doc),pdf=renderPDF(doc);
    assert(txt.find("Hello, world!")!=std::string::npos);
    assert(html.find("<msup>")!=std::string::npos);
    assert(txt.find("yes")!=std::string::npos);
    assert(txt.find(" no")==std::string::npos);
    assert(pdf.rfind("%PDF-1.4",0)==0);
    assert(pdf.find("xref")!=std::string::npos);
    auto f=e.compile("\\def\\a{A}{\\def\\a{B}\\a}\\a");
    assert(renderText(f).find("BA")!=std::string::npos);
    std::cout<<"MetaTeX core tests passed\n";
}
